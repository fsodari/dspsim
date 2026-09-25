#include <dspsim/vmodule/vmodule.h>
#include <VSkid.h>

DSPSIM_VMOD(Skid, VSkid)
{
public:
    // Parameters.

    // Ports.
    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT(rst, 1);
    DSPSIM_VINPUT(s_axis_tdata, 24);
    DSPSIM_VINPUT(s_axis_tvalid, 1);
    DSPSIM_VOUTPUT(s_axis_tready, 1);
    DSPSIM_VOUTPUT(m_axis_tdata, 24);
    DSPSIM_VOUTPUT(m_axis_tvalid, 1);
    DSPSIM_VINPUT(m_axis_tready, 1);

    DSPSIM_CTOR(Skid)
    {
        DSPSIM_METHOD(eval) // Standard eval func for verilated modules.
            ->always("*");  // Default is sensitive to all inputs. Can be changed by generator.
    }

    // Tracing methods.
    void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0);
    void _dump_trace();
    void _close_trace();
};
