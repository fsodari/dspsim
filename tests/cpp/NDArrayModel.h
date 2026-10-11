#include <dspsim/vmodule/vmodule.h>
#include <VNDArrayModel.h>

DSPSIM_VMOD(NDArrayModel, VNDArrayModel)
{
public:
    // Ports.
    DSPSIM_VINPUT(clk, 1);
    DSPSIM_VINPUT(rst, 1);
    DSPSIM_VINPUT_ARRAY(a, 8);
    DSPSIM_VOUTPUT_ARRAY(b, 8);

    DSPSIM_CTOR(NDArrayModel)
    {
        DSPSIM_METHOD(eval)
            ->always("*");
    }

    // Tracing methods.
    void open_trace_impl(const std::filesystem::path &trace_path, int levels = 99, int options = 0);
    void dump_trace_impl();
    void close_trace_impl();
};
