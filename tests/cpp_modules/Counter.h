#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/signal.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace demo
{
    /// A type that Python does not know: what uses it cannot be bound.
    struct Opaque
    {
        int value = 0;
    };

    /// A base class with methods of its own.
    struct Labeled : public dspsim::Module
    {
        Labeled(dspsim::ModuleName)
        {
        }

        const std::string &label() const { return label_; }
        void set_label(const std::string &label) { label_ = label; }
        /// Hidden by Counter::step().
        int step() const { return -1; }

    private:
        std::string label_ = "none";
    };

    /// Counts up by `step` on every rising clock edge and starts again when it reaches `limit`.
    struct Counter : public Labeled
    {
        static constexpr int start = 0;

        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Output<int32_t> q{"q"};

        Counter(dspsim::ModuleName name) : Counter(name, 1)
        {
        }
        Counter(dspsim::ModuleName name, int step, int limit = 100) : Labeled(name), step_(step), limit_(limit)
        {
            DSPSIM_CORO(eval);
        }
        /// Takes a type that Python does not know.
        Counter(dspsim::ModuleName name, const Opaque &config) : Counter(name, config.value)
        {
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());
                count_ += step_;
                if (count_ >= limit_)
                {
                    count_ = start;
                }
                q.write(count_);
            }
        }

        int step() const { return step_; }
        void set_step(int step) { step_ = step; }
        int limit() const { return limit_; }

        // Overloads.
        void load(int value) { count_ = value; }
        void load(const std::vector<int> &values)
        {
            count_ = 0;
            for (int value : values)
            {
                count_ += value;
            }
        }

        // Default arguments: literals, and an expression that only means something inside the class.
        std::vector<int> multiples(std::size_t n = 3) const
        {
            std::vector<int> values;
            for (std::size_t i = 1; i <= n; ++i)
            {
                values.push_back(static_cast<int>(i) * step_);
            }
            return values;
        }
        void reset(int value = start, bool keep_step = true)
        {
            count_ = value;
            step_ = keep_step ? step_ : 1;
        }
        std::optional<int> remaining(bool enabled = true) const
        {
            return enabled ? std::optional<int>(limit_ - count_) : std::nullopt;
        }
        std::pair<std::string, int> describe(const std::string &prefix = "counter") const
        {
            return {prefix + ":" + label(), count_};
        }

        static int version() { return 2; }

        // Types of the framework.
        dspsim::Output<int32_t> &output() { return q; }
        int32_t read_from(const dspsim::Signal<int32_t> &signal) const { return signal.read(); }

        // The module itself, as a result and as an argument.
        Counter &self() { return *this; }
        void copy_from(const Counter &other) { count_ = other.count_; }

        // Coroutines, which Python awaits. q is read: it has the count of the clock edge before.
        dspsim::Task<> wait_cycles(int cycles = 1)
        {
            for (int i = 0; i < cycles; ++i)
            {
                co_await wait(clk.pos());
            }
        }
        dspsim::Task<bool> wait_for(int value)
        {
            while (q.read() != value)
            {
                co_await wait(clk.pos());
            }
            co_return true;
        }
        dspsim::Task<int> value_after(int cycles)
        {
            co_await wait_cycles(cycles);
            co_return q.read();
        }
        dspsim::Task<std::vector<int>> record(std::size_t n)
        {
            std::vector<int> values;
            while (values.size() < n)
            {
                co_await wait(clk.pos());
                values.push_back(q.read());
            }
            co_return values;
        }

        // What cannot be bound.
        void configure(const Opaque &config) { step_ = config.value; }
        void fill(int *values, int n) const
        {
            for (int i = 0; i < n; ++i)
            {
                values[i] = count_;
            }
        }
        /// A coroutine that refers to its argument after the call has returned.
        dspsim::Task<int> sum_later(const std::vector<int> &values)
        {
            co_await wait(clk.pos());
            int sum = 0;
            for (int value : values)
            {
                sum += value;
            }
            co_return sum;
        }
        template <typename U>
        U scaled(U factor) const
        {
            return factor * count_;
        }
        int operator()(int offset) const { return count_ + offset; }

    protected:
        void hidden() {}

    private:
        int step_;
        int limit_;
        int count_ = start;
    };

    /// q <= d, `delay` clock cycles later (at most MAX).
    template <typename T, int MAX = 16>
    struct Delay : public dspsim::Module
    {
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> d{"d"};
        dspsim::Output<T> q{"q"};

        Delay(dspsim::ModuleName name) : Delay(name, 1)
        {
        }
        Delay(dspsim::ModuleName, int delay) : line_(MAX), delay_(delay)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait(clk.pos());
                line_.insert(line_.begin(), d.read());
                line_.pop_back();
                q.write(line_[delay_ - 1]);
            }
        }

        int delay() const { return delay_; }
        void set_delay(int delay) { delay_ = delay; }
        /// The samples in the delay line, newest first.
        std::vector<T> contents(std::size_t n = MAX) const { return std::vector<T>(line_.begin(), line_.begin() + n); }

    private:
        std::vector<T> line_;
        int delay_;
    };

    /// A second module whose task has a result type that the framework does not bind.
    struct Ticker : public dspsim::Module
    {
        dspsim::Input<uint8_t> clk{"clk"};

        Ticker(dspsim::ModuleName)
        {
        }

        /// The number of clock edges until the time is at least `time`.
        dspsim::Task<int> ticks_until(uint64_t time)
        {
            int ticks = 0;
            while (context()->time() < time)
            {
                co_await wait(clk.pos());
                ++ticks;
            }
            co_return ticks;
        }
    };

    /// A module that Python cannot construct: no constructor takes a name.
    struct Anonymous : public dspsim::Module
    {
        Anonymous()
        {
        }
    };
} // namespace demo
