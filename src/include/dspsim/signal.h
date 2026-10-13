#pragma once
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/ndarray.h>
#include <dspsim/bits.h>
#include <dspsim/bitsel.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dspsim
{
    class DerivedSignalBase;
    template <typename T>
    class DerivedSignal;
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
        // Derived signals register with their sources.
        template <typename T>
        friend class DerivedSignal;
        // The update stack keeps its "is scheduled" flag in the signal.
        template <typename T>
        friend class FlaggedStack;

    public:
        SignalBase(const std::string &name, int width);

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

    protected:
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

        /// The selection a derived signal is computed from, or nullptr for an ordinary signal.
        /// Selections of a derived signal refer to its sources instead, and writes to it go through to them.
        const BitSel *source_selection() const;

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
        // Set by DerivedSignal: the selection this signal is a view of. Checked on the write path.
        const BitSel *source_selection_ = nullptr;

    private:
        SensitivityEvent change_event_;
        SensitivityEvent posedge_event_;
        SensitivityEvent negedge_event_;
    };

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

        The member functions are defined in this header, below the class, for the types of models. The
        signals of the numeric types are instantiated in the core library (see the end of this file).
    */
    template <typename T>
    class Signal : public SignalBase
    {
        static_assert(std::copyable<T> && std::default_initializable<T> && std::equality_comparable<T>,
                      "The value type of a Signal must be copyable, default constructible and comparable with ==");

    public:
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

        /*
            Static Methods
        */
        static std::shared_ptr<Signal<T>> create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = {});

        // The pending value. Shouldn't be needed in a design, but it's available.
        const T &read_d_() const;

    protected:
        void update() override;
        uint64_t read_bits() const override;
        void write_bits(uint64_t value, uint64_t mask) override;

    protected:
        T d_, q_;
    };

    /*
        The member functions of Signal<T>. They are in the header so that a model can have signals of its
        own types; for the numeric types they are compiled once, in the core library.
    */
    template <typename T>
    Signal<T>::Signal(const std::string &name, int width, T init)
        : SignalBase(name, width), d_(init), q_(std::move(init))
    {
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
    const std::string Signal<T>::repr() const
    {
        return "";
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

        // A signal that is written back to its committed value stays scheduled: update() ignores it, which
        // is cheaper than taking it off the stack.
        if (d_ != q_) [[likely]]
        {
            request_update();
        }
    }

    template <typename T>
    bool Signal<T>::is_integral() const
    {
        return std::is_integral_v<T>;
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
                pos().notify_committed();
            }
            else if (!d_ && q_)
            {
                neg().notify_committed();
            }
        }
        change().notify_committed();

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

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim
