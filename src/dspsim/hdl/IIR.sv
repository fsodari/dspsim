/*
    IIR filter using second order sections.
*/
module IIR #(
    parameter DW = 24,
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter COEFW = 18,
    parameter COEFQ = 16,
    parameter ORDER = 6,
    parameter STAGE_ORDER = 2
) (
    input  logic clk,
    input  logic rst,

    // Input stream
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    // Output stream.
    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // Coefficients through config bus.
    input  logic cyc_i,
    input  logic stb_i,
    input  logic we_i,
    output logic ack_o,
    output logic stall_o,
    input  logic [CFGAW-1:0] addr_i,
    input  logic signed [CFGDW-1:0] data_i,
    output logic signed [CFGDW-1:0] data_o
);

// Number of stages.
localparam N_STAGES = ORDER / STAGE_ORDER;

// Number of coefficients in each stage.
localparam COEF_PER_STAGE = 2 * (STAGE_ORDER + 1);

// Total number of coefficients.
localparam N = N_STAGES * COEF_PER_STAGE;

// Wishbone registers for coefficients.
logic signed [COEFW-1:0] coefs [N];
WbRegs #(
    .CFGAW(CFGAW),
    .CFGDW(CFGDW),
    .REGW(COEFW),
    .N_CTL(N),
    .N_STS(0),
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
    /* verilator lint_off PINCONNECTEMPTY */
    .sts_regs()
    /* verilator lint_on PINCONNECTEMPTY */
);

generate
    // Busses to sequentially chain iir_core modules.
    logic signed [DW-1:0] sos_tdata [N_STAGES+1];
    logic sos_tvalid[N_STAGES+1], sos_tready[N_STAGES+1];

    // First bus
    assign sos_tdata[0] = s_axis_tdata;
    assign sos_tvalid[0] = s_axis_tvalid;
    assign s_axis_tready = sos_tready[0];

    // Last bus goes to output.
    assign m_axis_tdata = sos_tdata[N_STAGES];
    assign m_axis_tvalid = sos_tvalid[N_STAGES];
    assign sos_tready[N_STAGES] = m_axis_tready;

    for (genvar i = 0; i < N_STAGES; i = i + 1) begin : second_order_stages
        iir_core #(
            .DW(DW),
            .COEFW(COEFW),
            .COEFQ(COEFQ),
            .ORDER(STAGE_ORDER)
        ) iir_core_i (
            .clk,
            .rst,
            .s_axis_tdata(sos_tdata[i]),
            .s_axis_tvalid(sos_tvalid[i]),
            .s_axis_tready(sos_tready[i]),
            .m_axis_tdata(sos_tdata[i+1]),
            .m_axis_tvalid(sos_tvalid[i+1]),
            .m_axis_tready(sos_tready[i+1]),
            .coefs(coefs[i*COEF_PER_STAGE:(i+1)*COEF_PER_STAGE - 1])
        );
    end
endgenerate

endmodule
