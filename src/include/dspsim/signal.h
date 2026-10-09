#pragma once
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/ndarray.h>

#include <vector>
#include <memory>

namespace dspsim
{
    class SensitivityEvent;
    class PortBase;

    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    class SignalBase : public Model
    {
    public:
        SignalBase(const std::string &name = "");

        // Called during the update phase to commit pending changes to the signal.
        virtual void update() = 0;

        // Access the sensitivity events for this signal.
        SensitivityEvent &change() { return change_event_; }
        SensitivityEvent &pos() { return posedge_event_; }
        SensitivityEvent &neg() { return negedge_event_; }
        // Implicit conversion to SensitivityEvent (change event)
        operator SensitivityEvent &() { return change(); }

        bool &scheduled_flag() { return scheduled_; }

    protected:
        // Set while this signal sits in Context::_signal_update_stack; used by FlaggedStack.
        bool scheduled_;

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
        Signal<T> &init(const T &value);

        /*
            Properties
        */
        int width() const { return width_; }

        virtual const std::string repr() const override { return ""; }

        const T &read() const { return q_; }
        const T &operator()() const { return read(); }

        void write(const T &value);
        const T &operator=(const T &value)
        {
            write(value);
            return read();
        }

        void update() override;
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

    private:
        int width_;
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
