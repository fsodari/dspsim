module Gain #(
    parameter DW = 24,
    parameter COEFW = 18,
    parameter COEFQ = 16
) (
    input  logic clk,
    input  logic rst,

    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    input  logic signed [COEFW-1:0] gain
);

localparam MW = DW + COEFW;

logic signed [DW-1:0] skid_tdata;
logic skid_tvalid, skid_tready;

logic signed [MW-1:0] mult;
logic signed [DW-1:0] mult_o;
logic mult_valid;

// Skid buffer input
Skid #(.DW(DW)) skid_i (
    .clk(clk),
    .rst(rst),
    .s_axis_tdata(s_axis_tdata),
    .s_axis_tvalid(s_axis_tvalid),
    .s_axis_tready(s_axis_tready),
    .m_axis_tdata(skid_tdata),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);

// Multiplier
assign mult = skid_tdata * gain;
// Multiplier shifted output.
assign mult_o = {mult>>COEFQ}[DW-1:0];

//
assign skid_tready = (!m_axis_tvalid || m_axis_tready);
always @(posedge clk) begin

    // Output transaction accepted. Clear valid.
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // Input data is accepted when we have new data and the output is
    if (skid_tvalid && skid_tready) begin
        m_axis_tdata <= mult_o;
        m_axis_tvalid <= 1;
    end

    if (rst) begin
        m_axis_tvalid <= 0;
        // m_axis_tdata <= 0;
    end
end

endmodule
