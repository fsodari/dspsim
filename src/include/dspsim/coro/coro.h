#pragma once
#include <iostream>
#include <coroutine>
#include <queue>
#include <string>

namespace dspsim
{
    class Context;

    class SimContext
    {
    public:
        using Time = long long; // e.g., simulation ticks/picoseconds

        Time current_time() const { return now_; }

        void schedule(Time delta, std::coroutine_handle<> h)
        {
            queue_.push({now_ + delta, h});
        }

        void run()
        {
            while (!queue_.empty())
            {
                auto top = queue_.top();
                queue_.pop();
                if (top.time < now_)
                    continue; // stale event
                now_ = top.time;
                top.handle.resume();
            }
        }

    private:
        struct EventNode
        {
            Time time;
            std::coroutine_handle<> handle;
            bool operator>(const EventNode &o) const { return time > o.time; }
        };
        Time now_ = 0;
        std::priority_queue<EventNode, std::vector<EventNode>, std::greater<>> queue_;
    };

    // Global or thread-local context pointer for easy access inside awaiters
    inline thread_local SimContext *current_sim = nullptr;

    struct Task
    {
        struct promise_type
        {
            /*
            When a coroutine function is first invoked, the compiler sets up the coroutine state,
            creates the promise_type object, and immediately calls promise.get_return_object().
            The object returned by this function is what the caller of the coroutine function
            actually receives when the coroutine first suspends or finishes
            */
            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            // returning suspend_always here means the coroutine will always suspend initially.
            // returning suspend_never would start immediately.
            std::suspend_always initial_suspend() noexcept { return {}; }

            // Called when the coroutine exits. suspend_always will suspend the coroutine at the end, requiring cleanup.
            // suspend_never will immediately destroy the coroutine without suspending at the end.
            std::suspend_always final_suspend() noexcept { return {}; }

            // triggered with co_return. Other option is return_value(T) if the coroutine returns a value.
            void return_void() {}
            void unhandled_exception() { std::terminate(); }
        };

        //
        std::coroutine_handle<promise_type> handle;
        ~Task()
        {
            if (handle)
                handle.destroy();
        }
    };

    // Awaitable for waiting a specific time delta
    struct wait
    {
        int delta;
        Context *_context;
        // Event *_event;

        wait(int delta, Context *context) : delta(delta), _context(context) {}
        // wait(SensitivityEvent *event, Context *context) : _dynamic_event(event), _context(context) {}

        // Determine if the coroutine needs to suspend or it can continue immediately.
        // relative events will never be ready immediately. Absolute events could. Or passing 0 would do nothing?
        bool await_ready() const noexcept { return delta <= 0; }

        // If await_ready is false, Schedule the time event and coro is suspended.
        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            // Add coro to time event priority queue.
            current_sim->schedule(delta, h);
        }

        // Scheduler will call this to resume execution.
        void resume() {}

        // Returns result when the coroutine resumes. Can return a value if needed.
        void await_resume() noexcept {}
    };
    struct wait_event
    {
        // SensitivityEvent *_dynamic_event;
        Context *_context;

        // Check if the dynamic event has occured already.
        /*
            For example:
            while(True)
            {
                // Both could be valid simultaneously, so the coroutine will not yield after the first event.
                co_await wait_event{clk1.posedge_event(), context()};
                co_await wait_event{clk2.posedge_event(), context()};
            }
        */
        bool await_ready() const noexcept { return delta <= 0; }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            // current_sim->schedule(delta, h);
        }

        void resume() {}

        void await_resume() noexcept {}
    };

    Task sc_thread_example(int id)
    {
        while (true)
        {
            std::cout << "Thread " << id << " active at time: "
                      << current_sim->current_time() << "\n";
            // Wait for 10 time units (analogous to wait(10, SC_NS))
            co_await wait{10};
        }
    }

    int main()
    {
        SimContext sim;
        current_sim = &sim;

        // Register tasks in a vector in the context so the context has ownership.
        auto t1 = sc_thread_example(1);
        auto t2 = sc_thread_example(2);
        // Add handles to a UniqueStack

        // Kickstart them at t=0
        sim.schedule(0, t1.handle);
        sim.schedule(5, t2.handle); // offset start

        sim.run();
        return 0;
    }
} // namespace dspsim