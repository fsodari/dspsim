#include "dspsim/signal.h"

namespace dspsim
{
    template <typename T>
    Signal<T>::Signal(T init, int width, bool sign_ext)
    {
        d_local = init;
        d = &d_local;
        q = init;
        prev_q = !init; // !init ?

        set_width(width);
        set_sign_extend(sign_ext);
    }

    template <typename T>
    void Signal<T>::set_width(int width)
    {
        m_width = width;
        if (m_width == default_bitwidth<T>::value)
        {
            // m_bitmask = static_cast<StdintSignedMap<T>::type>(-1);
            m_bitmask = -1;
            m_sign_bit = 0;
            m_sign_mask = m_bitmask;
        }
        else
        {
            m_bitmask = ((T)1 << width) - 1;
            m_sign_bit = (T)1 << (width - 1);
            m_sign_mask = m_sign_bit - 1;
        }
    }

    template <typename T>
    void Signal<T>::eval_end_step()
    {
        prev_q = q;
        q = *d;
    }

    template <typename T>
    Signal<T>::operator const T() const
    {
        return this->read();
    }

    template <typename T>
    Signal<T> &Signal<T>::operator=(const T &other)
    {
        this->write(other);
        return *this;
    }

    template <typename T>
    Signal<T> &Signal<T>::operator=(const Signal<T> &other)
    {
        this->write(other.read());
        return *this;
    }

    template <typename T>
    void Signal<T>::write(T value)
    {
        // apply a bitmask when writing to the d pin.
        // This way, verilator models will see the correct number of bits set.
        *d = value & m_bitmask;
    }

    template <typename T>
    T Signal<T>::read() const
    {
        if (m_extend)
        {
            return _sign_extend(q, m_sign_bit, m_sign_mask);
        }
        else
        {
            return q;
        }
    }
    template <typename T>
    T Signal<T>::_read_d() const
    {
        return *d;
    }

    template <typename T>
    void Signal<T>::_force(T value)
    {
        *d = value;
        q = value;
    }

    // template <typename T, typename BT>
    // void Signal<T>::_bind(BT &other)
    // {
    //     d = &other;
    // }

    template <typename T>
    Dff<T>::Dff(Signal<uint8_t> &clk, T initial, int width, bool sign_ext) : Signal<T>(initial, width, sign_ext), clk(clk)
    {
    }

    template <typename T>
    void Dff<T>::eval_step()
    {
        update = clk.posedge();
    }

    template <typename T>
    void Dff<T>::eval_end_step()
    {
        this->prev_q = this->q;
        if (update)
        {
            this->q = this->_read_d();
        }
    }

    template <typename T>
    Dff<T>::operator const T() const
    {
        return this->read();
    }

    template <typename T>
    Signal<T> &Dff<T>::operator=(const T &other)
    {
        this->write(other);
        return *this;
    }

    template <typename T>
    Signal<T> &Dff<T>::operator=(const Signal<T> &other)
    {
        this->write(other.read());
        return *this;
    }

    // Explicit template instantiation
    // template class Signal<int8_t>;
    // template class Signal<int16_t>;
    // template class Signal<int32_t>;
    // template class Signal<int64_t>;
    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    // template class Dff<int8_t>;
    // template class Dff<int16_t>;
    // template class Dff<int32_t>;
    // template class Dff<int64_t>;
    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;

} // namespace dspsim
