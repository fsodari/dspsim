module Mixer #(
    parameter DW = 24,
    parameter COEFW = 18,
    parameter N = 4,
    parameter TIDW = 8
) (
    input  logic clk,
    input  logic rst,

    // Input data stream
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    input  logic [TIDW-1:0] s_axis_tid,

    // Output data stream
    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // Coefficients
    input  logic signed [COEFW-1:0] coefs[N]
);
    
endmodule
