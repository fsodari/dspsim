#pragma once
#include <dspsim/module.h>
#include <verilated.h>
#include <memory>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace dspsim
{
    class VModuleBase : public Module
    {
    public:
        std::vector<OutputBase *> _outputs;

    public:
        VModuleBase(ModuleName &name) : Module(name)
        {
            context()->logger->info("Initializing VModuleBase: {}", this->name());
        }
    };

    // Ports used for VModels. Can bind to the verilator model ports.
    template <typename T>
    class VInput : public Input<T>
    {
    private:
        T *_ext_port;
        VModuleBase *_vmodel_base;

    public:
        // using Input<T>::Input;
        VInput(const std::string &name) : Input<T>(name)
        {
            this->context()->logger->info("Initializing VInput: {}", this->name());
            _vmodel_base = static_cast<VModuleBase *>(this->parent());

            // Always sensitive to inputs.
            _vmodel_base->always << *this;
        }

        VInput(const std::string &name, T &ext_port) : VInput<T>(name)
        {
            bind_ext_port(ext_port);
        }

        virtual void _notify(EventType event) override
        {
            // Call base class notify. _notify is called during an update cycle.
            Input<T>::_notify(event);
            // Update the external port with the current value of this input.
            *_ext_port = this->read();
        }

        void bind_ext_port(T &ext_port)
        {
            _ext_port = &ext_port;
        }
    };

    template <typename T>
    class VOutput : public Output<T>
    {
    private:
        T *_ext_port;
        VModuleBase *_vmodel_base;

    public:
        VOutput(const std::string &name) : Output<T>(name)
        {
            _vmodel_base = static_cast<VModuleBase *>(this->parent());
            _vmodel_base->_outputs.push_back(this);
        }
        VOutput(const std::string &name, T &ext_port) : VOutput<T>(name)
        {
            bind_ext_port(ext_port);
        }

        virtual void _notify(EventType event) override
        {
            this->context()->logger->info("VOutput _notify called for {}", this->name());
            // Call base class notify.
            Output<T>::_notify(event);
            // Update the bound signal with the external port.
            this->write(*_ext_port);
        }

        void bind_ext_port(T &ext_port)
        {
            _ext_port = &ext_port;
        }
    };

    class NoTrace;

    // template <typename V, typename TraceType = NoTrace>
    // class VModel : public VModuleBase
    // {
    // protected:
    //     std::unique_ptr<VerilatedContext> vcontext;
    //     std::unique_ptr<V> top;
    //     std::unique_ptr<TraceType> tracep;

    // public:
    //     VModel(ModuleName &name) : VModuleBase(name)
    //     {
    //         vcontext = std::make_unique<VerilatedContext>();
    //         top = std::make_unique<V>(vcontext.get());
    //     }
    //     ~VModel()
    //     {
    //         close();
    //     }

    //     void eval() override
    //     {
    //         top->eval_step();
    //     }

    //     void update() override
    //     {
    //         top->eval_end_step();
    //         if (tracep)
    //         {
    //             tracep->dump(context()->time());
    //         }
    //         this->_sync_outputs();
    //     }

    //     void trace(const fs::path &trace_path, int levels = 99, int options = 0)
    //     {
    //         if (!tracep)
    //         {
    //             vcontext->traceEverOn(true);
    //             tracep = std::make_unique<TraceType>();

    //             tracep->set_time_unit(context()->time_unit().c_str());
    //             tracep->set_time_resolution(context()->time_unit().c_str());

    //             top->trace(tracep.get(), levels, options);
    //             tracep->open(trace_path.string().c_str());
    //         }
    //     }

    //     void close()
    //     {
    //         if (tracep)
    //         {
    //             tracep->close();
    //         }
    //     }
    // };

    // /*
    //     Specialization for no tracing.
    // */
    // template <typename V>
    // class VModel<V, NoTrace> : public VModuleBase
    // {
    // protected:
    //     std::unique_ptr<VerilatedContext> vcontext;
    //     std::unique_ptr<V> top;

    // public:
    //     VModel(ModuleName &name) : VModuleBase(name)
    //     {
    //         vcontext = std::make_unique<VerilatedContext>();
    //         top = std::make_unique<V>(vcontext.get());
    //     }

    //     void eval() override
    //     {
    //         top->eval_step();
    //     }
    //     void update() override
    //     {
    //         top->eval_end_step();
    //         this->_sync_outputs();
    //     }
    // };

} // namespace dspsim
