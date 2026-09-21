#pragma once
// #include <dspsim/forward.h>
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/utils/unique_stack.h>

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
    protected:
        // std::vector<PortBase *> _drivers;
        // std::vector<PortBase *> _subscribers;
        SensitivityEvent _change_event;
        SensitivityEvent _posedge_event;
        SensitivityEvent _negedge_event;
        bool _posedge_flag;
        bool _negedge_flag;
        bool _changed_flag;

    public:
        SignalBase(const std::string &name = "");

        // void _add_driver(PortBase *driver);
        // void _add_subscriber(PortBase *subscriber);
        virtual void update() = 0;
        using Model::id;

        // Allow a module to be sensitized directly to this signal (e.g. `always << some_signal;`),
        // without needing an intermediate Port.
        SensitivityEvent &pos();
        SensitivityEvent &neg();
        SensitivityEvent &_change();
        operator SensitivityEvent &();

        // Set if there was a posedge event in the previous update cycle.
        bool posedge() const;
        // Set if there was a negedge event in the previous update cycle.
        bool negedge() const;

        bool changed() const;
        void _clear_event_flag();
    };

    // template <typename T>
    // using SignalPtr = std::shared_ptr<class Signal<T>>;

    template <typename T>
    class Signal : public SignalBase
    {
    private:
        int _width;
        bool _is_signed;
        int _parent_id;

    protected:
        T _d, _q;

    public:
        Signal(const std::string &name, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);

        Signal<T> &init(const T &value);
        /*
            Properties
        */
        int width() const;
        bool is_signed() const;

        virtual const std::string repr() const override;

        void write(const T &value);
        const T &read() const;
        // Used for python d property
        const T &_read_d() const;

        virtual void update() override;
        /*
            Static Methods
        */
        static auto create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false)
        {
            return Model::create<Signal<T>>(name, width, init, is_signed);
        }
    };

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim