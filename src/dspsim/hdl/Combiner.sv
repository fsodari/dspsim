module Combiner #(
    parameter DW = 24,
    parameter N = 2
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata[N],
    input  logic s_axis_tvalid[N],
    output logic s_axis_tready[N],

    output logic [DW-1:0] m_axis_tdata[N],
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);
    
endmodule
