#pragma once
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/ndarray.h>
#include <dspsim/bits.h>
#include <dspsim/bitsel.h>
#include <dspsim/value_type.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace dspsim
{
    class DerivedSignalBase;
    class RecordSignalBase;
    template <typename T>
    class FlaggedStack;

    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    class SignalBase : public Model
    {
        // The scheduler commits the pending values in the update phase.
        friend class Context;
        // Bit selections read and write the raw bits of their source signals.
        friend class BitSel;
        // Derived signals register with their sources, and write through to them.
        friend class DerivedSignalBase;
        // A signal of a struct finds the signals of its fields among the derived signals of their source.
        friend class RecordSignalBase;
        // The update stack keeps its "is scheduled" flag in the signal.
        template <typename T>
        friend class FlaggedStack;

    public:
        SignalBase(const std::string &name, int width);

        /// This signal as a signal of a struct, or nullptr if its value is not a struct. A cast would do,
        /// but not everywhere between a model's extension module and the framework (see value_typeid()).
        virtual RecordSignalBase *record();

        /// Width of the signal in bits.
        /// Inline: bit selections read it for every update of a derived signal.
        int width() const { return width_; }

        /// True if the signal holds an integral value, so its bits can be selected with slice() or pack().
        virtual bool is_integral() const = 0;

        /// Select bits [hi:lo] of the signal. Slices are unsigned, as in SystemVerilog.
        BitSel slice(int hi, int lo);
        BitSel operator[](BitRange range);
        BitSel operator[](int bit);

        // Access the sensitivity events for this signal.
        // Defined inline: processes wait on these on every evaluation (wait(clk.pos())).
        SensitivityEvent &change() { return change_event_; }
        SensitivityEvent &pos() { return posedge_event_; }
        SensitivityEvent &neg() { return negedge_event_; }
        // Implicit conversion to SensitivityEvent (change event)
        operator SensitivityEvent &() { return change(); }

        /*
            The value without its type, for code that does not know it (the Python bindings).
        */
        /// What the value is made of, or nullptr for a type that has no description (a struct without a
        /// DSPSIM_STRUCT). See <dspsim/value_type.h>.
        virtual const ValueType *value_type() const = 0;
        /// The type of the value. Compare it by name: a model's extension module and the framework each have
        /// a std::type_info of a type.
        virtual const std::type_info &value_typeid() const = 0;
        /// The bytes of the committed value: sizeof the value type of them.
        virtual const void *value_data() const = 0;
        /// The bytes of the pending value: what was written last.
        virtual const void *pending_data() const = 0;
        /// write() the value that these bytes are. Throws std::logic_error if the value type cannot be copied
        /// as bytes.
        virtual void write_data(const void *value) = 0;

    protected:
        /// A signal of a given context, which can be elaborated already: see Model. For derived signals.
        SignalBase(Context *context, const std::string &name, int width);

        // Called during the update phase to commit pending changes to the signal. An override commits this
        // signal and notifies its events. It must not write to a signal or request the update of a channel:
        // writes belong to the evaluation phase. The engine does not check this.
        virtual void update() = 0;

        /// Schedule update() for the update phase of this delta cycle, however often it is called.
        /// Signal<T>::write() calls it when the pending value differs from the committed one. It is not
        /// inline, so that a Signal<T> of a user's type, which is compiled outside of the core library, needs
        /// nothing of the scheduler (signal.h does not include context.h).
        void request_update();

        /// The committed value as raw bits, masked to width(). Only called on integral signals (BitSel checks).
        virtual uint64_t read_bits() const = 0;
        /// Replace the bits of the pending value selected by mask with the bits of value, and schedule an update.
        /// Only called on integral signals (BitSel checks).
        virtual void write_bits(uint64_t value, uint64_t mask) = 0;

        /// The selection a derived signal is computed from, or nullptr for any other signal.
        /// Selections of a derived signal refer to its sources instead.
        const BitSel *source_selection() const;

        /// The storage that a field of this signal's value is a part of: the signal itself with its pending
        /// and committed value, or, for a signal that is a field of another one, that signal and the field
        /// in its values.
        virtual detail::FieldRoot field_root() = 0;
        /// field_root() of a derived signal. The signal is nullptr if it is not a view of a field.
        detail::FieldRoot derived_field_root();
        /// write() to a derived signal: the value goes to its sources. `value` points at a value of the
        /// signal's type.
        void write_through(const void *value);

        // Schedule the derived signals for recomputation after this signal has updated.
        void schedule_dependents();
        // Recompute the derived signals immediately after this signal has been initialized.
        void refresh_dependents();

    private:
        /// Register a derived signal that is recomputed whenever this signal changes.
        void add_dependent(DerivedSignalBase *dependent);
        // Set while this signal sits in Context::signal_update_stack_; used by FlaggedStack.
        // Inline: checked on every write().
        bool &scheduled_flag() { return scheduled_; }

    private:
        int width_;
        // Derived signals computed from this signal's bits.
        std::vector<DerivedSignalBase *> dependents_;

        /*
            Everything below is read by write()/update() on every cycle. It is declared last so that it shares
            cache lines with the value fields (d_, q_) of Signal<T>, which follow in memory.
        */
    protected:
        // Set while this signal sits in Context::signal_update_stack_; used by FlaggedStack.
        bool scheduled_;
        // True once dependents_ is non-empty (saves reading the vector in update()).
        bool has_dependents_ = false;

    private:
        SensitivityEvent change_event_;
        SensitivityEvent posedge_event_;
        SensitivityEvent negedge_event_;

    protected:
        // Set by a derived signal (a view of a bit selection or of a field of another signal) to itself:
        // what is written to it goes to its sources. Every write() checks it, also one that does not change
        // the value and touches nothing else but d_ and q_: it is the last member, right before them, so
        // that such a write stays within one or two cache lines.
        DerivedSignalBase *derived_ = nullptr;
    };

    /*
        A signal whose value is a struct: the base class of Signal<T> for a class type T (other than a
        std::array). It adds nothing to a signal but its fields by name, for code that does not know the
        type. Python has such a signal as a RecordSignal, whose fields are attributes.
    */
    class RecordSignalBase : public SignalBase
    {
    public:
        using SignalBase::SignalBase;

        RecordSignalBase *record() override;

        /// The field of that name, as a signal of its own (see FieldSignal in <dspsim/field_signal.h>). It
        /// is created when it is first asked for, at any time, and it is the same signal after that. The
        /// struct needs a DSPSIM_STRUCT. Throws std::invalid_argument if it has none, has no such field,
        /// or if the field is an array (see field_array()).
        SignalBase &field(const std::string &name);
        /// The elements of the field of that name as an array of signals, each a view of one element. A field
        /// that is not an array gives an array without dimensions, of one signal.
        detail::NdView<SignalBase> field_array(const std::string &name);
        /// The names of the fields, in the order of the DSPSIM_STRUCT. Empty without one.
        std::vector<std::string> field_names() const;
    };

    namespace detail
    {
        /// A value type that is a struct, whose signals and ports have fields.
        template <typename T>
        concept RecordValue = std::is_class_v<T> && !is_std_array<T>::value;

        /// The base class of Signal<T>.
        template <typename T>
        using signal_base_t = std::conditional_t<RecordValue<T>, RecordSignalBase, SignalBase>;
    }

    /*
        A signal with a value of type T: what a process writes becomes the value that the processes read in
        the update phase of the delta cycle, and the processes that are sensitive to the signal are then
        evaluated if the value changed.

        T is any type that can be copied, compared with == and constructed without arguments: a number, or
        a struct, a std::array, an enumeration ... of a model. With a struct as its value, a signal carries
        all the fields at once: a write replaces the whole value, the signal commits once and has one change
        event, whichever field changed. (A PortStruct is the other way to give a module a struct port: a port
        and a signal per field, which can have different directions and be driven by different processes.)

            struct Sample
            {
                int16_t re = 0;
                int16_t im = 0;
                bool operator==(const Sample &) const = default;
            };
            Signal<Sample> x{"x"};          // Input<Sample>, Output<Sample>, SignalArray<Sample> ...
            x.write({3, -4});

        Only a number has edges and a width in bits, and only an integer can be sliced. The events pos()
        and neg() of a signal of another type never occur.

        field() gives one field of a struct as a signal of its own, for a port of the field's type or a
        process that waits for that field alone. Python reads and writes a struct that has a DSPSIM_STRUCT
        (<dspsim/value_type.h>) as a numpy structured value.

        The member functions are defined in this header, below the class, for the types of models. The
        signals of the numeric types are instantiated in the core library (see the end of this file).
    */
    template <typename T>
    class Signal : public detail::signal_base_t<T>
    {
        static_assert(std::copyable<T> && std::default_initializable<T> && std::equality_comparable<T>,
                      "The value type of a Signal must be copyable, default constructible and comparable with ==");

    public:
        using Base = detail::signal_base_t<T>;

        /// For a type that is not a number the width is not used.
        Signal(const std::string &name = "", int width = default_bitwidth<T>::value, T init = {});
        virtual ~Signal() = default;
        /// Set the committed and pending value without notifying events. Derived signals have no value of
        /// their own, so init() throws std::logic_error for them; init the source signals instead.
        Signal<T> &init(const T &value);

        virtual const std::string repr() const override;

        // read() and the operators below are defined inline: model code calls them on every evaluation.
        const T &read() const { return q_; }
        const T &operator()() const { return read(); }

        /// Set the pending value and schedule an update. For a derived signal, the bits are written through to
        /// the source signals instead, so writes to a slice and to the whole signal combine (last write wins per bit).
        void write(const T &value);
        const T &operator=(const T &value)
        {
            write(value);
            return read();
        }

        bool is_integral() const override;

        const ValueType *value_type() const override;
        const std::type_info &value_typeid() const override;
        const void *value_data() const override;
        const void *pending_data() const override;
        void write_data(const void *value) override;

        /// A field of the struct that is the value, as a signal of its own: `bus.field(&Bus::tag)` is a
        /// Signal<uint8_t> that has the value of that field in the delta cycle in which the struct changes,
        /// and its events. What is written to it changes that field of the struct and leaves the others.
        /// Each call creates a signal, which the context owns. Defined in <dspsim/field_signal.h>, where
        /// FieldSignal is.
        template <typename F, typename S = T>
        Signal<F> &field(F S::*member, const std::string &name = "")
            requires std::same_as<S, T>;
        /// An element of the std::array that is the value, as a signal of its own. Like field().
        template <typename A = T>
        Signal<typename A::value_type> &element(std::size_t index, const std::string &name = "")
            requires std::same_as<A, T> && detail::is_std_array<A>::value;
        /// The field of that name: RecordSignalBase::field(), which the template above would hide.
        SignalBase &field(const std::string &name)
            requires detail::RecordValue<T>
        {
            return this->RecordSignalBase::field(name);
        }

        /*
            Static Methods
        */
        static std::shared_ptr<Signal<T>> create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = {});

        // The pending value. Shouldn't be needed in a design, but it's available.
        const T &read_d_() const;

    protected:
        /// A signal of a given context, which can be elaborated already: see Model. For derived signals.
        Signal(Context *context, const std::string &name, int width, T init);

        void update() override;
        uint64_t read_bits() const override;
        void write_bits(uint64_t value, uint64_t mask) override;
        detail::FieldRoot field_root() override;

    protected:
        T d_, q_;
    };

    /*
        The member functions of Signal<T>. They are in the header so that a model can have signals of its
        own types; for the numeric types they are compiled once, in the core library.
    */
    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init)
        : Base(name, width), d_(init), q_(std::move(init))
    {
    }

    template <typename T>
    Signal<T>::Signal(Context *context, const std::string &name, int width, T init)
        : Base(context, name, width), d_(init), q_(std::move(init))
    {
    }

    template <typename T>
    Signal<T> &Signal<T>::init(const T &value)
    {
        if (this->derived_)
        {
            throw std::logic_error("Cannot init derived signal " + this->hier_name() + ": init its source signals instead");
        }
        d_ = value;
        q_ = value;
        this->refresh_dependents();
        return *this;
    }

    template <typename T>
    const std::string Signal<T>::repr() const
    {
        return "";
    }

    template <typename T>
    void Signal<T>::write(const T &value)
    {
        // A derived signal is a view of its sources: write the value through to them. The sources schedule
        // their updates, and this signal is recomputed from them in the update phase.
        if (this->derived_) [[unlikely]]
        {
            this->write_through(&value);
            return;
        }
        d_ = value;

        // A signal that is written back to its committed value stays scheduled: update() ignores it, which
        // is cheaper than taking it off the stack.
        if (d_ != q_) [[likely]]
        {
            this->request_update();
        }
    }

    template <typename T>
    bool Signal<T>::is_integral() const
    {
        return std::is_integral_v<T>;
    }

    template <typename T>
    const ValueType *Signal<T>::value_type() const
    {
        return value_type_of<T>();
    }

    template <typename T>
    const std::type_info &Signal<T>::value_typeid() const
    {
        return typeid(T);
    }

    template <typename T>
    const void *Signal<T>::value_data() const
    {
        return &q_;
    }

    template <typename T>
    const void *Signal<T>::pending_data() const
    {
        return &d_;
    }

    template <typename T>
    void Signal<T>::write_data(const void *value)
    {
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            // The bytes need not be aligned.
            T copy;
            std::memcpy(&copy, value, sizeof(T));
            write(copy);
        }
        else
        {
            (void)value;
            throw std::logic_error("The value of signal " + this->hier_name() + " cannot be copied as bytes");
        }
    }

    template <typename T>
    std::shared_ptr<Signal<T>> Signal<T>::create(const std::string &name, int width, T init)
    {
        return Model::create<Signal<T>>(name, width, std::move(init));
    }

    template <typename T>
    const T &Signal<T>::read_d_() const
    {
        return d_;
    }

    template <typename T>
    void Signal<T>::update()
    {
        if (d_ == q_) [[unlikely]]
        {
            return;
        }

        if constexpr (std::is_arithmetic_v<T>)
        {
            if (d_ && !q_)
            {
                this->pos().notify_committed();
            }
            else if (!d_ && q_)
            {
                this->neg().notify_committed();
            }
        }
        this->change().notify_committed();

        this->q_ = this->d_;

        if (this->has_dependents_) [[unlikely]]
        {
            this->schedule_dependents();
        }
    }

    template <typename T>
    uint64_t Signal<T>::read_bits() const
    {
        if constexpr (std::is_integral_v<T>)
        {
            return zext(static_cast<uint64_t>(q_), this->width());
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
                write(static_cast<T>(sext(raw, this->width())));
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

    template <typename T>
    detail::FieldRoot Signal<T>::field_root()
    {
        return this->derived_ ? this->derived_field_root() : detail::FieldRoot{this, &d_, &q_};
    }

    // Instantiated in signal.cpp.
    extern template class Signal<uint8_t>;
    extern template class Signal<uint16_t>;
    extern template class Signal<uint32_t>;
    extern template class Signal<uint64_t>;
    extern template class Signal<int8_t>;
    extern template class Signal<int16_t>;
    extern template class Signal<int32_t>;
    extern template class Signal<int64_t>;
    extern template class Signal<float>;
    extern template class Signal<double>;
    extern template class Signal<bool>;

    /*
        Non-owning view of a selection of a SignalArray (or another view). Obtained with SignalArray::slice.
        The array must outlive the view.
    */
    template <typename T>
    class SignalArrayView : public detail::NdView<Signal<T>>
    {
    public:
        SignalArrayView(detail::NdView<Signal<T>> v) : detail::NdView<Signal<T>>(std::move(v)) {}

        SignalArrayView view() const { return *this; }
        SignalArrayView slice(const Slices &slices) const { return detail::NdView<Signal<T>>::slice(slices); }
        SignalArrayView select(const std::vector<Range> &ranges) const { return detail::NdView<Signal<T>>::select(ranges); }
    };

    /*
        Multidimensional array of signals. The shape is given at construction (any number of dimensions).
            SignalArray<int> a{"a", {2, 3}};
            a[{1, 2}] = 5;   // or a.at({1, 2})
        Elements are named "a[i][j]".
    */
    template <typename T>
    class SignalArray : public detail::NdArray<Signal<T>>
    {
    public:
        SignalArray(const std::string &name, Shape shape,
                    int width = default_bitwidth<T>::value, T init = {})
            : detail::NdArray<Signal<T>>(name, std::move(shape), [&](const std::string &n, std::size_t)
                                         { return std::make_unique<Signal<T>>(n, width, init); })
        {
        }

        // Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        SignalArrayView<T> view() const { return detail::NdArray<Signal<T>>::view(); }
        SignalArrayView<T> slice(const Slices &slices) const { return detail::NdArray<Signal<T>>::slice(slices); }
        SignalArrayView<T> select(const std::vector<Range> &ranges) const { return detail::NdArray<Signal<T>>::select(ranges); }

        // Heap-allocate an array whose lifetime is managed by the context (like Signal::create).
        // The context shares ownership of the array, and with it of its elements.
        static std::shared_ptr<SignalArray<T>> create(const std::string &name, Shape shape,
                                                      int width = default_bitwidth<T>::value, T init = {})
        {
            auto array = std::make_shared<SignalArray<T>>(name, std::move(shape), width, init);
            // An array without elements registered nothing with the context: nothing refers to it.
            if (array->size() > 0)
            {
                detail::own_object(array->flat(0).context(), array);
            }
            return array;
        }
    };

    /// The signals of an array (SignalArray<T> or a view of one) as an array of signals without the type of
    /// their value. The array must outlive it.
    template <typename Array>
    detail::NdView<SignalBase> signal_base_array(const Array &signals)
    {
        std::vector<SignalBase *> elements;
        elements.reserve(signals.size());
        for (auto &signal : signals)
        {
            elements.push_back(&signal);
        }
        return {signals.shape(), std::move(elements)};
    }

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim

// A field of a signal is a signal: FieldSignal and Signal<T>::field() need the classes above. The header
// includes this one, so either can be included first.
#include <dspsim/field_signal.h>
