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
        // Set while this signal sits in Context::_signal_update_stack; used by FlaggedStack.
        bool _scheduled = false;

    public:
        SignalBase(const std::string &name = "");

        // void _add_driver(PortBase *driver);
        // void _add_subscriber(PortBase *subscriber);
        virtual void update() = 0;
        using Model::id;

        // Allow a module to be sensitized directly to this signal (e.g. `always << some_signal;`),
        // without needing an intermediate Port.
        // Defined inline: read on every eval() in the hot path, must be inlinable without LTO.
        SensitivityEvent *pos() { return &_posedge_event; }
        SensitivityEvent *neg() { return &_negedge_event; }
        SensitivityEvent *_change() { return &_change_event; }
        operator SensitivityEvent *() { return _change(); }

        // Set if there was a posedge event in the previous update cycle.
        bool posedge() const { return _posedge_flag; }
        // Set if there was a negedge event in the previous update cycle.
        bool negedge() const { return _negedge_flag; }

        bool changed() const { return _changed_flag; }
        void _clear_event_flag()
        {
            _posedge_flag = false;
            _negedge_flag = false;
            _changed_flag = false;
        }
        bool &_scheduled_flag() { return _scheduled; }
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
        Signal(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);
        Signal<T> &init(const T &value);

        /*
            Properties
        */
        int width() const { return _width; }
        bool is_signed() const { return _is_signed; }

        virtual const std::string repr() const override { return ""; }

        void write(const T &value);
        const T &read() const { return _q; }
        // Used for python d property
        const T &_read_d() const { return _d; }

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
