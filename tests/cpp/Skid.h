#include <dspsim/vmodule/vmodule.h>
#include <VSkid.h>

DSPSIM_VMOD(Skid, VSkid)
{
public:
    // Parameters
    // Requires knowing the verilog module name, not just the prefix. Need to be generated.
    // static constexpr auto DW = VSkid___024root::Skid__DOT__DW;

    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT(rst, 1);
    DSPSIM_VINPUT(s_axis_tdata, 24);
    DSPSIM_VINPUT(s_axis_tvalid, 1);
    DSPSIM_VOUTPUT(s_axis_tready, 1);
    DSPSIM_VOUTPUT(m_axis_tdata, 24);
    DSPSIM_VOUTPUT(m_axis_tvalid, 1);
    DSPSIM_VINPUT(m_axis_tready, 1);

    // Standard eval func. Sensitive to all inputs.
    DSPSIM_VCTOR(Skid, VSkid);

    void _dump_trace();
    void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0);
    void _close_trace();
};
