#include "dspsim/signal.h"

namespace dspsim
{
    // Explicit template instantiation
    template class Signal<uint8_t>;
    template class Signal<uint16_t>;
    template class Signal<uint32_t>;
    template class Signal<uint64_t>;
    template class Dff<uint8_t>;
    template class Dff<uint16_t>;
    template class Dff<uint32_t>;
    template class Dff<uint64_t>;

} // namespace dspsim
