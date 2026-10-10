#pragma once
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/ndarray.h>
#include <dspsim/bitsel.h>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace dspsim
{
    class SensitivityEvent;
    class PortBase;
    class DerivedSignalBase;

    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    class SignalBase : public Model
    {
    public:
        SignalBase(const std::string &name, int width);

        /// Width of the signal in bits.
        int width() const { return width_; }

        // Called during the update phase to commit pending changes to the signal.
        virtual void update() = 0;

        /// True if the signal holds an integral value, so its bits can be selected with slice() or pack().
        virtual bool is_integral() const = 0;
        /// The committed value as raw bits, masked to width(). Only called on integral signals (BitSel checks).
        virtual uint64_t read_bits() const = 0;
        /// Replace the bits of the pending value selected by mask with the bits of value, and schedule an update.
        /// Only called on integral signals (BitSel checks).
        virtual void write_bits(uint64_t value, uint64_t mask) = 0;

        /// Select bits [hi:lo] of the signal. Slices are unsigned, as in SystemVerilog.
        BitSel slice(int hi, int lo);
        BitSel operator[](BitRange range) { return slice(range.hi, range.lo); }
        BitSel operator[](int bit) { return slice(bit, bit); }

        /// The selection a derived signal is computed from, or nullptr for an ordinary signal.
        /// Selections of a derived signal refer to its sources instead, and writes to it go through to them.
        const BitSel *source_selection() const { return source_selection_; }
        /// Register a derived signal that is recomputed whenever this signal changes.
        void add_dependent(DerivedSignalBase *dependent);

        // Access the sensitivity events for this signal.
        SensitivityEvent &change() { return change_event_; }
        SensitivityEvent &pos() { return posedge_event_; }
        SensitivityEvent &neg() { return negedge_event_; }
        // Implicit conversion to SensitivityEvent (change event)
        operator SensitivityEvent &() { return change(); }

        bool &scheduled_flag() { return scheduled_; }

    protected:
        // Schedule the derived signals for recomputation after this signal has updated.
        void schedule_dependents();
        // Recompute the derived signals immediately after this signal has been initialized.
        void refresh_dependents();

    private:
        int width_;
        // Derived signals computed from this signal's bits.
        std::vector<DerivedSignalBase *> dependents_;

        /*
            Everything below is read by write()/update() on every cycle. It is declared last so that it shares
            cache lines with the value fields (d_, q_) of Signal<T>, which follow in memory.
        */
    protected:
        // Set while this signal sits in Context::_signal_update_stack; used by FlaggedStack.
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

    template <typename T>
    class Signal : public SignalBase
    {

    public:
        Signal(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0);
        virtual ~Signal() = default;
        /// Set the committed and pending value without notifying events. Derived signals have no value of
        /// their own, so init() throws std::logic_error for them; init the source signals instead.
        Signal<T> &init(const T &value);

        virtual const std::string repr() const override { return ""; }

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

        void update() override;

        bool is_integral() const override { return std::is_integral_v<T>; }
        uint64_t read_bits() const override;
        void write_bits(uint64_t value, uint64_t mask) override;

        /*
            Static Methods
        */
        static auto create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0)
        {
            return Model::create<Signal<T>>(name, width, init);
        }

        // This shouldn't be used, but it's available.
        const T &read_d_() const { return d_; }

    protected:
        T d_, q_;
    };

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
                    int width = default_bitwidth<T>::value, T init = 0)
            : detail::NdArray<Signal<T>>(name, std::move(shape), [&](const std::string &n, std::size_t)
                                         { return std::make_unique<Signal<T>>(n, width, init); })
        {
        }

        // Views of the whole array or a sub-selection. One Slice per leading dimension; the rest are kept whole.
        SignalArrayView<T> view() const { return detail::NdArray<Signal<T>>::view(); }
        SignalArrayView<T> slice(const Slices &slices) const { return detail::NdArray<Signal<T>>::slice(slices); }
        SignalArrayView<T> select(const std::vector<Range> &ranges) const { return detail::NdArray<Signal<T>>::select(ranges); }

        // Heap-allocate an array whose lifetime is managed by the context (like Signal::create).
        // Each element shares ownership of the array, so it lives as long as the context holds any element.
        static std::shared_ptr<SignalArray<T>> create(const std::string &name, Shape shape,
                                                      int width = default_bitwidth<T>::value, T init = 0)
        {
            auto array = std::make_shared<SignalArray<T>>(name, std::move(shape), width, init);
            for (std::size_t i = 0; i < array->size(); ++i)
            {
                Signal<T> &element = array->flat(i);
                _own_model_helper(element.context(), std::shared_ptr<Model>(array, &element));
            }
            return array;
        }
    };

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim
