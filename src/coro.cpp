#include <dspsim/coro.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

#include <cstddef>
#include <exception>
#include <new>
#include <utility>

namespace dspsim
{
    namespace
    {
        /*
            Freed coroutine frames of one thread, for reuse by the next coroutine of the same size.

            Frames are small (a task that waits for a handshake is about 100 bytes) and short-lived, and a
            design allocates the same few sizes over and over. Sizes are rounded up to a multiple of 16 bytes,
            and each of those sizes has a list of free frames, up to a limit: a burst of frames that is freed
            at once goes back to the allocator. Larger frames always do.

            A frame may be freed by another thread than the one that allocated it (a context that is handed
            to another thread). It then joins the pool of the thread that freed it, which is fine: every
            block comes from operator new with its rounded size.
        */
        class FramePool
        {
        public:
            FramePool() = default;
            FramePool(const FramePool &) = delete;
            FramePool &operator=(const FramePool &) = delete;
            ~FramePool()
            {
                for (Block *&head : free_)
                {
                    while (head)
                    {
                        Block *next = head->next;
                        ::operator delete(head);
                        head = next;
                    }
                }
            }

            void *allocate(std::size_t size)
            {
                const std::size_t size_class = (size + granularity - 1) / granularity;
                if (size_class < size_classes) [[likely]]
                {
                    if (Block *block = free_[size_class]) [[likely]]
                    {
                        free_[size_class] = block->next;
                        --count_[size_class];
                        return block;
                    }
                    return ::operator new(size_class * granularity);
                }
                return ::operator new(size);
            }

            void release(void *frame, std::size_t size) noexcept
            {
                const std::size_t size_class = (size + granularity - 1) / granularity;
                if (size_class < size_classes && count_[size_class] < max_free) [[likely]]
                {
                    Block *block = static_cast<Block *>(frame);
                    block->next = free_[size_class];
                    free_[size_class] = block;
                    ++count_[size_class];
                    return;
                }
                ::operator delete(frame);
            }

        private:
            struct Block
            {
                Block *next;
            };
            // What operator new aligns to, so a pooled frame is aligned like any other.
            static constexpr std::size_t granularity = 16;
            // Frames of up to 1024 bytes are pooled.
            static constexpr std::size_t size_classes = 65;
            // Free frames kept per size.
            static constexpr std::size_t max_free = 4096;

            Block *free_[size_classes] = {};
            std::size_t count_[size_classes] = {};
        };

#if defined(__SANITIZE_ADDRESS__)
        // The address sanitizer finds a use of a destroyed coroutine frame only if the frame is really freed.
        constexpr bool pool_frames = false;
#else
        constexpr bool pool_frames = true;
#endif
        thread_local FramePool frame_pool;
    } // namespace

    void *detail::TaskPromiseBase::operator new(std::size_t size)
    {
        return pool_frames ? frame_pool.allocate(size) : ::operator new(size);
    }

    void detail::TaskPromiseBase::operator delete(void *frame, std::size_t size) noexcept
    {
        if (pool_frames)
        {
            frame_pool.release(frame, size);
        }
        else
        {
            ::operator delete(frame);
        }
    }

    void detail::TaskPromiseBase::rethrow()
    {
        std::rethrow_exception(std::exchange(exception_, nullptr));
    }

    void WaitBase::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        process_->set_resume_point(handle);
    }

    void WaitBase::schedule_event(SensitivityEvent &event)
    {
        process_->schedule_dynamic_event(event);
    }

    void WaitBase::schedule_time(uint64_t time_delta)
    {
        process_->schedule_time_wait(time_delta);
    }

    /*
        Time Event awaitable.
    */
    WaitTimeEvent::WaitTimeEvent(uint64_t time_delta, ProcessBase *process)
        : WaitBase(process)
    {
        schedule_time(time_delta);
    }

    /*
        Sensitivity Event awaitable.
    */
    WaitSensitivityEvent::WaitSensitivityEvent(ProcessBase *process)
        : WaitBase(process)
    {
        // Static sensitivity is already registered. Nothing to schedule.
    }

    WaitSensitivityEvent::WaitSensitivityEvent(SensitivityEvent &event, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        schedule_event(event);
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(std::reference_wrapper<SensitivityEvent> event, ProcessBase *process, uint64_t timeout)
        : WaitSensitivityEvent(event.get(), process, timeout)
    {
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto &event : events)
        {
            schedule_event(event.get());
        }
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<SensitivityEvent *> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto *event : events)
        {
            schedule_event(*event);
        }
        schedule_timeout(timeout);
    }

    void WaitSensitivityEvent::schedule_timeout(uint64_t timeout)
    {
        if (timeout == 0)
        {
            return;
        }
        has_timeout_ = true;
        deadline_ = process()->context()->time() + timeout;
        schedule_time(timeout);
    }

    WaitResult WaitSensitivityEvent::await_resume() const noexcept
    {
        // The timeout is processed before any event of the same time step, and cancels the events, so the
        // process reaches the deadline only through the timeout.
        if (has_timeout_ && process()->context()->time() >= deadline_)
        {
            return WaitResult::Timeout;
        }
        return WaitResult::Triggered;
    }
} // namespace dspsim
