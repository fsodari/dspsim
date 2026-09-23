#include <dspsim/vmodule/vmodule.h>
#include <VSkid.h>
// #include <VSkid___024root.h>

DSPSIM_VMOD(Skid, VSkid)
{
public:
    // Parameters
    // Requires knowing the verilog module name, not just the prefix.
    // static constexpr auto DW = VSkid___024root::Skid__DOT__DW;

    DSPSIM_VINPUT(clk);
    DSPSIM_VINPUT(rst);
    DSPSIM_VINPUT(s_axis_tdata);
    DSPSIM_VINPUT(s_axis_tvalid);
    DSPSIM_VOUTPUT(s_axis_tready);
    DSPSIM_VOUTPUT(m_axis_tdata);
    DSPSIM_VOUTPUT(m_axis_tvalid);
    DSPSIM_VINPUT(m_axis_tready);

    DSPSIM_VCTOR(Skid, VSkid)
    {
        DSPSIM_METHOD(&Skid::eval_top)->always("*");
        // Sensitive to all inputs.
        // always("*");
    }

    void _dump_trace();
    void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0);
    void _close_trace();
};
