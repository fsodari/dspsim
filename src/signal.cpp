#include "dspsim/signal.h"

namespace dspsim
{
    template <typename T>
    Signal<T>::Signal(T init)
    {
        d_local = init;
        d = &d_local;
        q = init;
        prev_q = !init; // !init ?
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
    Signal<T> &Signal<T>::operator=(const Signal<T> &other)
    {
        this->write(other.read());
        return *this;
    }

    template <typename T>
    void Signal<T>::write(T value)
    {
        *d = value;
    }

    template <typename T>
    T Signal<T>::read() const
    {
        return q;
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

    template <typename T>
    void Signal<T>::_bind(T &other)
    {
        d = &other;
    }

    template <typename T>
    std::shared_ptr<Signal<T>> Signal<T>::create(T initial)
    {
        auto s = std::make_shared<Signal<T>>(initial);
        s->context()->own_model(s);
        return s;
    }

    template <typename T>
    Dff<T>::Dff(Signal<uint8_t> &clk, T initial) : Signal<T>(initial), clk(clk)
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
    std::shared_ptr<Dff<T>> Dff<T>::create(Signal<uint8_t> &clk, T initial)
    {
        // return create<Dff<T>>(clk, initial);
        auto dff = std::make_shared<Dff<T>>(clk, initial);
        dff->context()->own_model(dff);
        return dff;
    }

    // Explicit template instantiation
    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;

} // namespace dspsim
