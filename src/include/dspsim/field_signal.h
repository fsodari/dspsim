#pragma once
#include <dspsim/detail/derived_signal_base.h>
#include <dspsim/signal.h>

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>

namespace dspsim
{
    /*
        A signal of type F that is a view of one field of the struct that is the value of another signal (or
        of one element of a field that is an array): the same idea as a DerivedSignal for a selection of bits.

            Signal<Bus> bus{"bus"};
            monitor.tag.bind(bus.field(&Bus::tag));     // an Input<uint8_t> reads one field
            driver.data.bind(bus.field(&Bus::data));    // an Output<uint32_t> writes one field

        It has the value of the field in the delta cycle in which the struct changes, and its own events: a
        process that waits for it is not evaluated when only other fields change. What is written to it
        changes that field of the pending value of the struct and leaves the others, so writes to several
        fields and to the whole struct in one evaluation phase combine (the last write of a field wins).

        This is how a module with a struct as the value of a port connects to modules with a port per field.
        A struct signal costs nothing for the fields that nobody asked for.

        Field signals are created by Signal<T>::field() or RecordSignalBase::field(name) and are owned by
        the context. Unlike other signals they can be created at any time, also while the simulation runs:
        nothing has to find them when the design is elaborated. The source must outlive the simulation.
    */
    template <typename F>
    class FieldSignal final : public Signal<F>, public DerivedSignalBase
    {
    public:
        /// field.signal is the source, field.d and field.q are the field in its pending and committed value.
        FieldSignal(const std::string &name, const detail::FieldRoot &field);

        /// Create a field signal owned by the context.
        static std::shared_ptr<FieldSignal<F>> create(const std::string &name, const detail::FieldRoot &field);

        /// The signal whose value the field is a part of.
        SignalBase &source() const;

    private:
        void commit() override;
        void refresh() override;
        void write_through(const void *value) override;
        detail::FieldRoot field_root() override;
        SignalBase *field_signal(const void *q) override;

    private:
        SignalBase *source_;
        F *source_d_;
        const F *source_q_;
    };

    namespace detail
    {
        /// Throws std::logic_error unless a field signal of `signal` can be created: `root` is its
        /// field_root(), which has no signal if `signal` is a view of a bit selection.
        void check_field_root(const SignalBase &signal, const FieldRoot &root);
        /// Name of the signal of the field at `offset` bytes of the value of `signal`: "bus.tag" if the value
        /// has a DSPSIM_STRUCT that lists a field there, "bus.field" otherwise.
        std::string field_signal_name(const SignalBase &signal, std::size_t offset);

        template <typename E>
        SignalBase &make_field_signal(const std::string &name, const FieldRoot &field)
        {
            return *FieldSignal<E>::create(name, field);
        }
    }

    template <typename F>
    FieldSignal<F>::FieldSignal(const std::string &name, const detail::FieldRoot &field)
        : Signal<F>(field.signal->context(), name, default_bitwidth<F>::value, *static_cast<const F *>(field.q)),
          source_(field.signal),
          source_d_(static_cast<F *>(field.d)),
          source_q_(static_cast<const F *>(field.q))
    {
        depend_on(*source_);
        this->derived_ = this;
    }

    template <typename F>
    std::shared_ptr<FieldSignal<F>> FieldSignal<F>::create(const std::string &name, const detail::FieldRoot &field)
    {
        return Model::create<FieldSignal<F>>(name, field);
    }

    template <typename F>
    SignalBase &FieldSignal<F>::source() const
    {
        return *source_;
    }

    template <typename F>
    void FieldSignal<F>::commit()
    {
        this->d_ = *source_q_;
        Signal<F>::update();
    }

    template <typename F>
    void FieldSignal<F>::refresh()
    {
        this->d_ = *source_q_;
        this->q_ = this->d_;
    }

    template <typename F>
    void FieldSignal<F>::write_through(const void *value)
    {
        *source_d_ = *static_cast<const F *>(value);
        // What was written last, for read_d_(). The value itself comes back from the source when it updates.
        this->d_ = *source_d_;
        // The source compares its pending value with the committed one when it updates.
        request_update(*source_);
    }

    template <typename F>
    detail::FieldRoot FieldSignal<F>::field_root()
    {
        return {source_, source_d_, source_q_};
    }

    template <typename F>
    SignalBase *FieldSignal<F>::field_signal(const void *q)
    {
        return q == source_q_ ? this : nullptr;
    }

    template <typename T>
    template <typename F, typename S>
    Signal<F> &Signal<T>::field(F S::*member, const std::string &name)
        requires std::same_as<S, T>
    {
        const detail::FieldRoot root = this->field_root();
        detail::check_field_root(*this, root);
        const detail::FieldRoot field{root.signal, &(static_cast<T *>(root.d)->*member), &(static_cast<const T *>(root.q)->*member)};
        const auto offset = static_cast<std::size_t>(static_cast<const char *>(field.q) - static_cast<const char *>(root.q));
        return *FieldSignal<F>::create(name.empty() ? detail::field_signal_name(*this, offset) : name, field);
    }

    template <typename T>
    template <typename A>
    Signal<typename A::value_type> &Signal<T>::element(std::size_t index, const std::string &name)
        requires std::same_as<A, T> && detail::is_std_array<A>::value
    {
        const detail::FieldRoot root = this->field_root();
        detail::check_field_root(*this, root);
        const detail::FieldRoot field{root.signal, &static_cast<T *>(root.d)->at(index), &static_cast<const T *>(root.q)->at(index)};
        const std::string element_name = name.empty() ? this->name() + "[" + std::to_string(index) + "]" : name;
        return *FieldSignal<typename A::value_type>::create(element_name, field);
    }

    // Instantiated in derived_signal.cpp.
    extern template class FieldSignal<uint8_t>;
    extern template class FieldSignal<uint16_t>;
    extern template class FieldSignal<uint32_t>;
    extern template class FieldSignal<uint64_t>;
    extern template class FieldSignal<int8_t>;
    extern template class FieldSignal<int16_t>;
    extern template class FieldSignal<int32_t>;
    extern template class FieldSignal<int64_t>;
    extern template class FieldSignal<float>;
    extern template class FieldSignal<double>;
    extern template class FieldSignal<bool>;
} // namespace dspsim
