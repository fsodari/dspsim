#include "dspsim/signal.h"

namespace dspsim
{
    template <typename T>
    Signal<T>::Signal(T init, int width)
    {
        d_local = init;
        d = &d_local;
        q = init;
        prev_q = !init; // !init ?

        set_width(width);
    }

    template <typename T>
    void Signal<T>::set_width(int width)
    {
        m_width = width;
        if (m_width == default_bitwidth<T>::value)
        {
            m_bitmask = static_cast<T>(-1);
            m_sign_bit = 0;
            m_sign_mask = m_bitmask;
        }
        else
        {
            m_bitmask = (1 << width) - 1;
            m_sign_bit = 1 << (width - 1);
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
    Dff<T>::Dff(Signal<uint8_t> &clk, T initial, int width) : Signal<T>(initial, width), clk(clk)
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

    template <typename T>
    std::shared_ptr<Dff<T>> Dff<T>::create(Signal<uint8_t> &clk, T initial, int width)
    {
        // // return create<Dff<T>>(clk, initial);
        // auto dff = std::make_shared<Dff<T>>(clk, initial);
        // dff->context()->own_model(dff);
        // return dff;
        // return Context::create_and_register<Dff<T>>(clk, initial);
        return Model::create<Dff<T>>(clk, initial, width);
    }

    // Explicit template instantiation
    template class Signal<int8_t>;
    template class Signal<int16_t>;
    template class Signal<int32_t>;
    template class Signal<int64_t>;
    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Dff<int8_t>;
    template class Dff<int16_t>;
    template class Dff<int32_t>;
    template class Dff<int64_t>;
    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;

} // namespace dspsim
