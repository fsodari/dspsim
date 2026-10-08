#pragma once
#include <dspsim/context.h>
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
        Signal(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);
        virtual ~Signal() = default;
        Signal<T> &init(const T &value);

        /*
            Properties
        */
        int width() const { return width_; }
        bool is_signed() const { return is_signed_; }

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
        static auto create(const std::string &name = "", int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false)
        {
            return Model::create<Signal<T>>(name, width, init, is_signed);
        }

        // This shouldn't be used, but it's available.
        const T &read_d_() const { return d_; }

    protected:
        T d_, q_;

    private:
        int width_;
        bool is_signed_;
        int parent_id_;
    };

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim
