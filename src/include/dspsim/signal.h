#pragma once
#include <dspsim/model.h>

namespace dspsim
{
    template <typename T>
    class Signal : public Model<Signal<T>>
    {
    };
} // namespace dspsim
