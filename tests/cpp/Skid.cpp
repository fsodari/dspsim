#include "Skid.h"
using VModType = Skid;

// Set during the verilation step.
#if VM_TRACE_VCD
#include <verilated_vcd_c.h>
using TraceType = VerilatedVcdC;
#elif VM_TRACE_FST
#include <verilated_fst_c.h>
using TraceType = VerilatedFstC;
#endif

void VModType::_dump_trace()
{
#if VM_TRACE_VCD || VM_TRACE_FST
    if (tracep)
    {
        static_cast<TraceType *>(tracep.get())->dump(context()->time());
    }
#endif
}
void VModType::_open_trace(const std::filesystem::path &trace_path, int levels, int options)
{
#if VM_TRACE_VCD || VM_TRACE_FST
    if (!tracep)
    {
        vcontext->traceEverOn(true);
        tracep = std::make_shared<TraceType>();

        static_cast<TraceType *>(tracep.get())->set_time_unit(context()->time_unit().c_str());
        static_cast<TraceType *>(tracep.get())->set_time_resolution(context()->time_unit().c_str());

        top->trace(tracep.get(), levels, options);
        static_cast<TraceType *>(tracep.get())->open(trace_path.string().c_str());
    }
#else
    this->context()->log("warn", "Tracing is not enabled for module: " + this->hier_name());
#endif
}

void VModType::_close_trace()
{
#if VM_TRACE_VCD || VM_TRACE_FST
    if (tracep)
    {
        static_cast<TraceType *>(tracep.get())->close();
    }
#endif
}

// Verilator on windows won't build unless this is defined.
double sc_time_stamp()
{
    return 0;
}
