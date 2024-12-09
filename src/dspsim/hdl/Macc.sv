// Multiply-Accumulate
module Macc #(
    parameter DW = 24,
    parameter COEFW = 18,
    localparam MW = DW + COEFW
) (
    input  logic clk,
    input  logic rst,
    
    input  logic signed [DW-1:0] s_axis_atdata,
    input  logic s_axis_atvalid,
    output logic s_axis_atready,

    input  logic signed [COEFW-1:0] s_axis_btdata,
    input  logic s_axis_btvalid,
    output logic s_axis_btready,

    output logic signed [MW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);
    
endmodule
