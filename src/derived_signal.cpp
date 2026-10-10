#include <dspsim/derived_signal.h>
#include <dspsim/bits.h>
#include <dspsim/context.h>

#include <climits>
#include <stdexcept>

namespace dspsim
{
    template <typename T>
    DerivedSignal<T>::DerivedSignal(const std::string &name, const BitSel &selection)
        : Signal<T>(name, selection.width()), selection_(selection)
    {
        if (selection_.width() > static_cast<int>(sizeof(T) * CHAR_BIT))
        {
            throw std::invalid_argument("Selection of width " + std::to_string(selection_.width()) +
                                        " does not fit in derived signal " + this->hier_name());
        }
        for (const auto &part : selection_.parts())
        {
            part.signal->add_dependent(this);
        }
        this->source_selection_ = &selection_;
        refresh();
    }

    template <typename T>
    void DerivedSignal<T>::pull()
    {
        this->d_ = from_bits(selection_.read());
        Signal<T>::update();
    }

    template <typename T>
    void DerivedSignal<T>::refresh()
    {
        this->d_ = from_bits(selection_.read());
        this->q_ = this->d_;
    }

    template <typename T>
    T DerivedSignal<T>::from_bits(uint64_t bits) const
    {
        if constexpr (std::is_signed_v<T>)
        {
            return static_cast<T>(sext(bits, this->width()));
        }
        else
        {
            return static_cast<T>(bits);
        }
    }

    SignalBase &BitSel::signal(const std::string &name) const
    {
        if (width_ <= 8)
        {
            return signal<uint8_t>(name);
        }
        else if (width_ <= 16)
        {
            return signal<uint16_t>(name);
        }
        else if (width_ <= 32)
        {
            return signal<uint32_t>(name);
        }
        else
        {
            return signal<uint64_t>(name);
        }
    }

    template <std::integral T>
    DerivedSignal<T> &BitSel::signal(const std::string &name) const
    {
        // Selections are never empty, and all sources belong to the design under construction.
        if (parts_.front().signal->context()->elaborated())
        {
            throw std::logic_error("Cannot create a signal from a selection after elaboration");
        }
        return *DerivedSignal<T>::create(name, *this);
    }

    template class DerivedSignal<uint8_t>;
    template class DerivedSignal<uint16_t>;
    template class DerivedSignal<uint32_t>;
    template class DerivedSignal<uint64_t>;
    template class DerivedSignal<int8_t>;
    template class DerivedSignal<int16_t>;
    template class DerivedSignal<int32_t>;
    template class DerivedSignal<int64_t>;

    template DerivedSignal<uint8_t> &BitSel::signal<uint8_t>(const std::string &) const;
    template DerivedSignal<uint16_t> &BitSel::signal<uint16_t>(const std::string &) const;
    template DerivedSignal<uint32_t> &BitSel::signal<uint32_t>(const std::string &) const;
    template DerivedSignal<uint64_t> &BitSel::signal<uint64_t>(const std::string &) const;
    template DerivedSignal<int8_t> &BitSel::signal<int8_t>(const std::string &) const;
    template DerivedSignal<int16_t> &BitSel::signal<int16_t>(const std::string &) const;
    template DerivedSignal<int32_t> &BitSel::signal<int32_t>(const std::string &) const;
    template DerivedSignal<int64_t> &BitSel::signal<int64_t>(const std::string &) const;
} // namespace dspsim
