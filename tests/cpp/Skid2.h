#include <dspsim/vmodule/vmodule.h>
#include <VSkid2.h>

DSPSIM_VMOD(Skid2, VSkid2)
{
public:
    // Parameters
    // Requires knowing the verilog module name, not just the prefix. Must be generated.

    DSPSIM_VINPUT(clk);
    DSPSIM_VINPUT(rst);
    DSPSIM_VINPUT(s_axis_tdata);
    DSPSIM_VINPUT(s_axis_tvalid);
    DSPSIM_VOUTPUT(s_axis_tready);
    DSPSIM_VOUTPUT(m_axis_tdata);
    DSPSIM_VOUTPUT(m_axis_tvalid);
    DSPSIM_VINPUT(m_axis_tready);

    // Standard eval func. Sensitive to all inputs.
    DSPSIM_VCTOR(Skid2, VSkid2);

    void _dump_trace();
    void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0);
    void _close_trace();
};
