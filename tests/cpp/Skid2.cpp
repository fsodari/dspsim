#include "Skid2.h"
using VModType = Skid2;

#if VM_TRACE_VCD
#include <verilated_vcd_c.h>
using TraceType = VerilatedVcdC;
#elif VM_TRACE_FST
#include <verilated_fst_c.h>
using TraceType = VerilatedFstC;
#else
using TraceType = dspsim::NoTrace;
#endif

void VModType::_dump_trace()
{
    if (tracep)
    {
        static_cast<TraceType *>(tracep.get())->dump(context()->time());
    }
}
void VModType::_open_trace(const std::filesystem::path &trace_path, int levels, int options)
{
    if (!tracep)
    {
        vcontext->traceEverOn(true);
        tracep = std::make_shared<TraceType>();

        static_cast<TraceType *>(tracep.get())->set_time_unit(context()->time_unit().c_str());
        static_cast<TraceType *>(tracep.get())->set_time_resolution(context()->time_unit().c_str());

        top->trace(tracep.get(), levels, options);
        static_cast<TraceType *>(tracep.get())->open(trace_path.string().c_str());
    }
}

void VModType::_close_trace()
{
    if (tracep)
    {
        static_cast<TraceType *>(tracep.get())->close();
    }
}
