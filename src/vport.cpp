#include <dspsim/vmodule/vport.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    template <typename T>
    VInput<T>::VInput(const std::string &name) : Input<T>(name)
    {
        this->context()->logger->info("Initializing VInput: {}", this->name());
        _vmodel_base = static_cast<VModuleBase *>(this->parent());
        _vmodel_base->_inputs.push_back(this);

        // Always sensitive to inputs.
        _vmodel_base->always << *this;
    }

    template <typename T>
    VInput<T>::VInput(const std::string &name, T &ext_port) : VInput<T>(name)
    {
        bind_ext_port(ext_port);
    }

    // template <typename T>
    // void VInput<T>::_notify(EventType event)
    // {
    //     // Call base class notify. _notify is called during an update cycle.
    //     Input<T>::_notify(event);
    //     // Update the external port with the current value of this input.
    //     *_ext_port = this->read();
    // }
    template <typename T>
    void VInput<T>::_sync()
    {
        *_ext_port = this->read();
    }

    template <typename T>
    void VInput<T>::bind_ext_port(T &ext_port)
    {
        _ext_port = &ext_port;
    }

    template <typename T>
    VOutput<T>::VOutput(const std::string &name) : Output<T>(name)
    {
        _vmodel_base = static_cast<VModuleBase *>(this->parent());
        _vmodel_base->_outputs.push_back(this);
    }
    template <typename T>
    VOutput<T>::VOutput(const std::string &name, T &ext_port) : VOutput<T>(name)
    {
        bind_ext_port(ext_port);
    }

    // template <typename T>
    // void VOutput<T>::_notify(EventType event)
    // {
    //     this->context()->logger->info("VOutput _notify called for {}", this->name());
    //     // Call base class notify.
    //     Output<T>::_notify(event);
    //     // Update the bound signal with the external port.
    //     this->write(*_ext_port);
    // }
    template <typename T>
    void VOutput<T>::_sync()
    {
        this->write(*_ext_port);
    }

    template <typename T>
    void VOutput<T>::bind_ext_port(T &ext_port)
    {
        _ext_port = &ext_port;
    }

    template class VInput<uint8_t>;
    template class VInput<uint16_t>;
    template class VInput<uint32_t>;
    template class VInput<uint64_t>;
    template class VInput<int>;

    template class VOutput<uint8_t>;
    template class VOutput<uint16_t>;
    template class VOutput<uint32_t>;
    template class VOutput<uint64_t>;
    template class VOutput<int>;
}
