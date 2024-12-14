module SomeExample #(
    parameter DW = 32
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

Skid #(.DW(DW)) skid_i (.*);

endmodule
