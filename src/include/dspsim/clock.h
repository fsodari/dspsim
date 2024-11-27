#pragma once
#include <dspsim/model.h>

namespace dspsim
{
    template <typename T = uint8_t>
    class Clock : public Signal<T>
    {
    };
} // namespace dspsim
