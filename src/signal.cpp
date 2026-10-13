#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/derived_signal.h>

#include <algorithm>

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
        context()->add_signal(this);
    }

    BitSel SignalBase::slice(int hi, int lo)
    {
        return BitSel(*this, hi, lo);
    }

    BitSel SignalBase::operator[](BitRange range)
    {
        return slice(range.hi, range.lo);
    }

    BitSel SignalBase::operator[](int bit)
    {
        return slice(bit, bit);
    }

    const BitSel *SignalBase::source_selection() const
    {
        return source_selection_;
    }

    void SignalBase::request_update()
    {
        context()->signal_update_stack_.push_back(this);
    }

    void SignalBase::schedule_dependents()
    {
        for (auto *dependent : dependents_)
        {
            context()->post_update_stack_.push_back(dependent);
        }
    }

    void SignalBase::refresh_dependents()
    {
        for (auto *dependent : dependents_)
        {
            dependent->refresh();
        }
    }

    void SignalBase::add_dependent(DerivedSignalBase *dependent)
    {
        if (std::find(dependents_.begin(), dependents_.end(), dependent) == dependents_.end())
        {
            dependents_.push_back(dependent);
            has_dependents_ = true;
        }
    }

    // The member functions of Signal<T> are defined in the header.
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
