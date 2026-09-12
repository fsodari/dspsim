#pragma once
#include <dspsim/model.h>

namespace dspsim
{
    template <typename T>
    struct default_bitwidth
    {
        static constexpr int value = sizeof(T) * 8;
    };

    template <typename T>
    class Signal;

    template <typename T>
    using SignalPtr = std::shared_ptr<class Signal<T>>;

    template <typename T>
    class Signal : public Model
    {
    public:
        Signal(int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false);
        ~Signal();

        /*
            Properties
        */
        int width() const { return _width; }
        bool is_signed() const { return _is_signed; }

        virtual const std::string repr() const override;

        T d() const { return *_d; }
        void set_d(T value) { *_d = value; }

        T q() const { return *_q; }

        /*
            Methods
        */
        virtual void eval_step() override {}
        virtual void eval_end_step() override;

        /*
            Static Methods
        */
        static auto create(int width = default_bitwidth<T>::value, T init = 0, bool is_signed = false)
        {
            return Model::create<Signal<T>>(width, init, is_signed);
        }

    private:
        int _width;
        bool _is_signed;
        int _parent_id;

    protected:
        T _d_local, _q_local;
        T *_d, *_q;
        T _q_prev;
    };

    using Signal8 = Signal<uint8_t>;
    using Signal16 = Signal<uint16_t>;
    using Signal32 = Signal<uint32_t>;
    using Signal64 = Signal<uint64_t>;

} // namespace dspsim