/*
    Standard module format with typical components instantiated.
*/
module SomeModule #(
    parameter DW = 24,      // Stream data width
    parameter COEFW = 18,   // Coefficient width
    parameter COEFQ = 16,   // Coefficient fixed-point q
    parameter CFGAW = 32,   // Config port Address Width
    parameter CFGDW = 32,   // Config port data width (might be wider than COEFW)
    parameter NCOEF = 32,   // Number of coefficients in the config interface.
    parameter NSTS = 32     // Number of debug/status registers.
) (
    input  logic clk,
    input  logic rst,

    // Input Stream
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    // Output stream
    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // Config port
    input  logic cyc_i,
    input  logic stb_i,
    input  logic we_i,
    output logic ack_o,
    output logic stall_o,
    input  logic [CFGAW-1:0] addr_i,
    input  logic signed [CFGDW-1:0] data_i,
    output logic signed [CFGDW-1:0] data_o
);

// Input Skid Buffer
logic signed [DW-1:0] skid_tdata;
logic skid_tvalid, skid_tready;
Skid #(.DW(DW)) skid_i (
    .clk,
    .rst,
    .s_axis_tdata,
    .s_axis_tvalid,
    .s_axis_tready,
    .m_axis_tdata(skid_tdata),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);

// Config Port to registers
logic signed [COEFW-1:0] coefs [NCOEF];

// Set these in the code.
logic signed [COEFW-1:0] sts [NSTS];
WbRegs #(
    .CFGAW(CFGAW),
    .CFGDW(CFGDW),
    .REGW(COEFW),
    .N_CTL(NCOEF),
    .N_STS(NSTS),
    .SIGN_EXTEND(1)
) wb_regs_i (
    .clk,
    .rst,
    .cyc_i,
    .stb_i,
    .we_i,
    .ack_o,
    .stall_o,
    .addr_i,
    .data_i,
    .data_o,
    .ctl_regs(coefs),
    .sts_regs(sts)
);

// Module logic

// This likely needs to change depending on module behavior.
assign skid_tready = !m_axis_tvalid || m_axis_tready;

always @(posedge clk) begin
    // Output transaction accepted, clear tvalid.
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // New input transaction
    if (skid_tvalid && skid_tready) begin
        // Do something.
        m_axis_tdata <= skid_tdata + $signed(coefs[0]);
        m_axis_tvalid <= 1;
    end

    // reset
    if (rst) begin
        m_axis_tvalid <= 0;

        for (int i = 0; i < NSTS; i = i + 1) begin
            sts[i] <= 0;
        end
    end
end

endmodule
