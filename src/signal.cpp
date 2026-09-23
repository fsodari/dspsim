#include <dspsim/signal.h>
#include <dspsim/context.h>
#include <dspsim/port.h>
#include <dspsim/module.h>
#include <dspsim/event.h>
#include <format>

#include "internal.h"

namespace dspsim
{
    SignalBase::SignalBase(const std::string &name)
        : Model(name, "signal"),
          _posedge_flag(false),
          _negedge_flag(false),
          _changed_flag(false)
    {
        context()->_add_signal(this);
    }
}
