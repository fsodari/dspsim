#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/port.h>
#include <dspsim/module.h>
#include <dspsim/event.h>
#include <dspsim/bits.h>
#include <dspsim/derived_signal.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name, int width)
        : Model(name, "signal"),
          width_(width),
          dependents_(),
          scheduled_(false),
          change_event_(context()),
          posedge_event_(context()),
          negedge_event_(context())
    {
        context()->_add_signal(this);
    }

    BitSel SignalBase::slice(int hi, int lo)
    {
        return BitSel(*this, hi, lo);
    }

    void SignalBase::add_dependent(DerivedSignalBase *dependent)
    {
        if (std::find(dependents_.begin(), dependents_.end(), dependent) == dependents_.end())
        {
            dependents_.push_back(dependent);
            has_dependents_ = true;
        }
    }

    void SignalBase::schedule_dependents()
    {
        for (auto *dependent : dependents_)
        {
            context()->_derived_update_stack.push_back(dependent);
        }
    }

    void SignalBase::refresh_dependents()
    {
        for (auto *dependent : dependents_)
        {
            dependent->refresh();
        }
    }

    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init)
        : SignalBase(name, width)
    {
        d_ = init;
        q_ = init;
    }

    template <typename T>
    Signal<T> &Signal<T>::init(const T &value)
    {
        if (source_selection_)
        {
            throw std::logic_error("Cannot init derived signal " + hier_name() + ": init its source signals instead");
        }
        d_ = value;
        q_ = value;
        refresh_dependents();
        return *this;
    }

    template <typename T>
    void Signal<T>::write(const T &value)
    {
        if constexpr (std::is_integral_v<T>)
        {
            // A derived signal is a view of its sources: write the bits through to them. The sources schedule
            // their updates, and this signal is recomputed from them in the update phase.
            if (source_selection_) [[unlikely]]
            {
                source_selection_->write(static_cast<uint64_t>(value));
                return;
            }
        }
        d_ = value;

        if (d_ != q_) [[likely]]
        {
            // Schedule for update
            context()->_signal_update_stack.push_back(this);
        }
        // Erasing is probably more expensive than just ignoring a change during the update cycle.
        // else [[unlikely]]
        // {
        //     // If the signal is written more than once, and reset so that it no longer needs to be updated, remove it from the update stack.
        //     // This is an expensive operation. It would be ideal to avoid this, but some non-blocking assignment patterns
        //     // will write the same signal multiple times within the same update cycle.
        //     auto it = context()->_signal_update_stack.find(this);

        //     if (it != context()->_signal_update_stack.end())
        //     {
        //         context()->_signal_update_stack.erase(it);
        //     }
        // }
    }

    template <typename T>
    void Signal<T>::update()
    {
        if (d_ == q_) [[unlikely]]
        {
            return;
        }

        if (d_ && !q_)
        {
            pos().notify();
        }
        else if (!d_ && q_)
        {
            neg().notify();
        }
        change().notify();

        this->q_ = this->d_;

        if (has_dependents_) [[unlikely]]
        {
            schedule_dependents();
        }
    }

    template <typename T>
    uint64_t Signal<T>::read_bits() const
    {
        if constexpr (std::is_integral_v<T>)
        {
            return zext(static_cast<uint64_t>(q_), width());
        }
        else
        {
            // BitSel only refers to integral signals.
            std::unreachable();
        }
    }

    template <typename T>
    void Signal<T>::write_bits(uint64_t value, uint64_t mask)
    {
        if constexpr (std::is_integral_v<T>)
        {
            const uint64_t raw = (static_cast<uint64_t>(d_) & ~mask) | (value & mask);
            if constexpr (std::is_signed_v<T>)
            {
                // Keep signed values sign extended from the signal's width.
                write(static_cast<T>(sext(raw, width())));
            }
            else
            {
                write(static_cast<T>(raw));
            }
        }
        else
        {
            (void)value;
            (void)mask;
            std::unreachable();
        }
    }

    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Signal<int8_t>;
    template class Signal<int16_t>;
    template class Signal<int32_t>;
    template class Signal<int64_t>;
    template class Signal<float>;
    template class Signal<double>;
}
