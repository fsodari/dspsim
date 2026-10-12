#include <dspsim/vmodule/vmodule.h>
#include <VSkidVhdl.h>

// hdl/vhdl/Skid.vhd: the SystemVerilog that dspsim_vhdl_source() made of the entity, verilated like any other
// module. The ports have the types of the VHDL ports: s_axis_tdata is a signed vector again.
DSPSIM_VMOD(SkidVhdl, VSkidVhdl)
{
public:
    // Ports.
    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT(rst, 1);
    DSPSIM_VINPUT_S(s_axis_tdata, 24);
    DSPSIM_VINPUT(s_axis_tvalid, 1);
    DSPSIM_VOUTPUT(s_axis_tready, 1);
    DSPSIM_VOUTPUT_S(m_axis_tdata, 24);
    DSPSIM_VOUTPUT(m_axis_tvalid, 1);
    DSPSIM_VINPUT(m_axis_tready, 1);

    DSPSIM_CTOR(SkidVhdl)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};
