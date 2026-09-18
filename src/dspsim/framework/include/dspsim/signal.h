#pragma once
#include <dspsim/model.h>
#include <dspsim/forward.h>
// #include <dspsim/port.h>

namespace dspsim
{
    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    class SignalBase : public Model
    {
    public:
        SignalBase(const std::string &name = "");
        virtual const std::string kind() const { return "signal"; }

        void add_driver(PortBase *driver)
        {
            _drivers.push_back(driver);
        }
        void add_subscriber(PortBase *subscriber)
        {
            _subscribers.push_back(subscriber);
        }
        std::vector<PortBase *> _subscribers;

    protected:
        std::vector<PortBase *> _drivers;
    };

    template <typename T>
    using SignalPtr = std::shared_ptr<class Signal<T>>;

    template <typename T>
    class Signal : public SignalBase
    {
    public:
        Signal(const std::string &name, int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);
        ~Signal();

        Signal<T> &init(const T &value);
        /*
            Properties
        */
        int width() const { return _width; }
        bool is_signed() const { return _is_signed; }

        virtual const std::string repr() const override;

        void write(const T &value);
        const T &read() const;

        virtual void eval() override;
        virtual void update() override;
        /*
            Static Methods
        */
        static auto create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false)
        {
            return Model::create<Signal<T>>(name, width, init, is_signed);
        }

    private:
        int _width;
        bool _is_signed;
        int _parent_id;

    protected:
        T _d, _q;
    };

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim