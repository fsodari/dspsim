#include <dspsim/vmodule/vmodule.h>
#include <VWideModel.h>

DSPSIM_VMOD(WideModel, VWideModel)
{
public:
    // Ports.
    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT_WIDE(a, 100);
    DSPSIM_VINPUT_WIDE(b, 100);
    DSPSIM_VOUTPUT_WIDE(sum, 101);
    DSPSIM_VINPUT_WIDE(s, 100);
    DSPSIM_VOUTPUT_WIDE(neg, 100);
    DSPSIM_VINPUT_WIDE(arr_in, 128);
    DSPSIM_VOUTPUT_WIDE(arr_out, 128);

    DSPSIM_CTOR(WideModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }
};
