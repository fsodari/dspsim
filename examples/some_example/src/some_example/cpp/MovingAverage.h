#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>

#include <cstddef>
#include <cstdint>
#include <deque>

/// q <= the average of the last `length` values of d, on every rising clock edge.
DSPSIM_MODULE(MovingAverage)
{
public:
    dspsim::Input<uint8_t> clk{"clk"};
    dspsim::Input<int32_t> d{"d"};
    dspsim::Output<int32_t> q{"q"};

    MovingAverage(dspsim::ModuleName name) : MovingAverage(name, 4)
    {
    }
    MovingAverage(dspsim::ModuleName, int length) : length_(length)
    {
        DSPSIM_CORO(eval);
    }

    dspsim::Task<> eval()
    {
        while (true)
        {
            co_await wait(clk.pos());
            samples_.push_back(d.read());
            sum_ += samples_.back();
            if (samples_.size() > static_cast<std::size_t>(length_))
            {
                sum_ -= samples_.front();
                samples_.pop_front();
            }
            q.write(static_cast<int32_t>(sum_ / length_));
        }
    }

    int length() const { return length_; }
    /// Forget the samples so far.
    void clear()
    {
        samples_.clear();
        sum_ = 0;
    }

private:
    int length_;
    std::deque<int32_t> samples_;
    int64_t sum_ = 0;
};
