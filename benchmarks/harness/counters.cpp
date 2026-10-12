#include "counters.h"

#include <chrono>

#if defined(__linux__)
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <time.h>
#endif

namespace benchmarks
{
    namespace
    {
        // CPU time of the calling thread, and the part of it spent in the kernel.
        struct ThreadTime
        {
            uint64_t cpu_ns = 0;
            uint64_t kernel_ns = 0;
        };

        ThreadTime thread_time()
        {
            ThreadTime time;
#if defined(_WIN32)
            FILETIME creation, exit, kernel, user;
            if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user))
            {
                auto ticks = [](const FILETIME &t)
                { return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
                // FILETIME counts 100 ns intervals.
                time.cpu_ns = (ticks(kernel) + ticks(user)) * 100;
                time.kernel_ns = ticks(kernel) * 100;
            }
#else
            timespec ts;
            if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
            {
                time.cpu_ns = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000u + static_cast<uint64_t>(ts.tv_nsec);
            }
#if defined(__linux__)
            rusage usage;
            if (getrusage(RUSAGE_THREAD, &usage) == 0)
            {
                time.kernel_ns = static_cast<uint64_t>(usage.ru_stime.tv_sec) * 1'000'000'000u +
                                 static_cast<uint64_t>(usage.ru_stime.tv_usec) * 1'000u;
            }
#endif
#endif
            return time;
        }

#if defined(__linux__)
        // Open one counter for the calling thread, on whichever CPU it runs. group_fd is -1 for the group leader.
        int open_counter(uint32_t type, uint64_t config, int group_fd)
        {
            perf_event_attr attr;
            std::memset(&attr, 0, sizeof(attr));
            attr.size = sizeof(attr);
            attr.type = type;
            attr.config = config;
            // The group starts and stops with its leader.
            attr.disabled = group_fd == -1;
            // User mode only: this is what an unprivileged process may count (perf_event_paranoid = 2),
            // and it leaves interrupts and page faults out of the result.
            attr.exclude_kernel = 1;
            attr.exclude_hv = 1;
            attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
            return static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, group_fd, 0));
        }
#endif
    } // namespace

    struct Counters::State
    {
        uint64_t wall_start = 0;
        ThreadTime time_start;
#if defined(__linux__)
        // The group leader counts instructions. -1 if the hardware counters are not available.
        int leader = -1;
        int cycles = -1;
        int l1d_misses = -1;
        int branch_misses = -1;
#elif defined(_WIN32)
        ULONG64 cycles_start = 0;
#endif
    };

    Counters::Counters() : state_(std::make_unique<State>())
    {
#if defined(__linux__)
        State &s = *state_;
        // BENCH_COUNTERS=time turns the hardware counters off, to see what a machine without them reports.
        const char *choice = std::getenv("BENCH_COUNTERS");
        if (choice != nullptr && std::strcmp(choice, "time") == 0)
        {
            return;
        }
        s.leader = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, -1);
        if (s.leader >= 0)
        {
            s.cycles = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, s.leader);
            // The remaining two are diagnostics. A virtual machine may not offer them.
            s.l1d_misses = open_counter(PERF_TYPE_HW_CACHE,
                                        PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                                            (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
                                        s.leader);
            s.branch_misses = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, s.leader);
        }
#endif
    }

    Counters::~Counters()
    {
#if defined(__linux__)
        for (int fd : {state_->branch_misses, state_->l1d_misses, state_->cycles, state_->leader})
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }
#endif
    }

    void Counters::start()
    {
        State &s = *state_;
        s.time_start = thread_time();
        s.wall_start = now_ns();
#if defined(__linux__)
        if (s.leader >= 0)
        {
            ioctl(s.leader, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
            ioctl(s.leader, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
        }
#elif defined(_WIN32)
        QueryThreadCycleTime(GetCurrentThread(), &s.cycles_start);
#endif
    }

    Counts Counters::stop()
    {
        State &s = *state_;
        Counts counts;
#if defined(__linux__)
        if (s.leader >= 0)
        {
            ioctl(s.leader, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        }
#elif defined(_WIN32)
        ULONG64 cycles_end = 0;
        QueryThreadCycleTime(GetCurrentThread(), &cycles_end);
#endif
        counts.wall_ns = now_ns() - s.wall_start;
        const ThreadTime time_end = thread_time();
        counts.cpu_ns = time_end.cpu_ns - s.time_start.cpu_ns;
        counts.kernel_ns = time_end.kernel_ns - s.time_start.kernel_ns;

#if defined(__linux__)
        if (s.leader >= 0)
        {
            // Layout with PERF_FORMAT_GROUP: number of counters, time enabled, time running, then one value
            // per counter in the order they were opened.
            uint64_t data[3 + 4] = {};
            if (read(s.leader, data, sizeof(data)) > 0)
            {
                const uint64_t enabled = data[1];
                const uint64_t running = data[2];
                // The kernel rotates groups when there are more of them than hardware counters. The values
                // then cover only the time the group was running: scale them up and say so.
                const double scale = (running > 0 && running < enabled) ? static_cast<double>(enabled) / static_cast<double>(running) : 1.0;
                counts.multiplexed = running < enabled;
                auto scaled = [scale](uint64_t value)
                { return static_cast<uint64_t>(static_cast<double>(value) * scale); };

                uint64_t *value = data + 3;
                counts.source = "perf_event";
                counts.instructions = scaled(*value++);
                if (s.cycles >= 0)
                {
                    counts.cycles = scaled(*value++);
                }
                if (s.l1d_misses >= 0)
                {
                    counts.l1d_misses = scaled(*value++);
                }
                if (s.branch_misses >= 0)
                {
                    counts.branch_misses = scaled(*value++);
                }
            }
        }
#elif defined(_WIN32)
        counts.source = "thread_cycle_time";
        counts.cycles = cycles_end - s.cycles_start;
#endif
        return counts;
    }

    uint64_t now_ns()
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    uint64_t peak_rss_kb()
    {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS info;
        if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
        {
            return 0;
        }
        return static_cast<uint64_t>(info.PeakWorkingSetSize / 1024);
#elif defined(__linux__)
        // Not getrusage(): its ru_maxrss survives fork() and exec(), so a process started from a large
        // parent (the Python driver) would report the size of the parent. VmHWM belongs to this program.
        uint64_t peak_kb = 0;
        if (std::FILE *status = std::fopen("/proc/self/status", "r"))
        {
            char line[256];
            while (std::fgets(line, sizeof(line), status))
            {
                if (std::strncmp(line, "VmHWM:", 6) == 0)
                {
                    peak_kb = std::strtoull(line + 6, nullptr, 10);
                    break;
                }
            }
            std::fclose(status);
        }
        return peak_kb;
#else
        rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) != 0)
        {
            return 0;
        }
        // macOS reports bytes.
        return static_cast<uint64_t>(usage.ru_maxrss) / 1024;
#endif
    }
}
