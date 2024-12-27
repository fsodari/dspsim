module Macc #(
    parameter ADW = 24,
    parameter BDW = 18,
    parameter ODW = 48
) (
    input  logic clk,
    input  logic rst,

    // Input A stream input. when tlast goes high, the accumulator is read out and reset.
    input  logic signed [ADW-1:0] s_axis_atdata,
    input  logic signed [BDW-1:0] s_axis_btdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    input  logic s_axis_tlast,

    // Output of the accumulator after a complete frame is received.
    output logic signed [ODW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

// Input combiner
logic signed [ADW-1:0] skid_atdata;
logic signed [BDW-1:0] skid_btdata;
logic skid_tvalid, skid_tready, skid_tlast;

Skid #(
    .DW(ADW + BDW + 1)
) skid_i (
    .clk,
    .rst,
    .s_axis_tdata({s_axis_atdata, s_axis_btdata, s_axis_tlast}),
    .s_axis_tvalid,
    .s_axis_tready,
    .m_axis_tdata({skid_atdata, skid_btdata, skid_tlast}),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);

// Macc core
// logic macc_ce = 0;
logic macc_sload = 0;
logic signed [ADW-1:0] macc_a;
logic signed [BDW-1:0] macc_b;
logic signed [ODW-1:0] macc_accum;
macc_core #(
    .ADW(ADW),
    .BDW(BDW),
    .ODW(ODW)
) macc_core_i (
    .clk,
    .ce(1),
    // .sload(skid_tvalid && skid_tready && skid_tlast),
    .sload(macc_sload),
    .a(macc_a),
    .b(macc_b),
    .accum_o(macc_accum)
);

assign skid_tready = !m_axis_tvalid || m_axis_tready;

localparam LATENCY = 4;
int latency_ctr = 0;

logic sload_r = 0, sload_r2 = 0;
always @(posedge clk) begin
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // Check if output is ready.
    if (latency_ctr > 0) begin
        latency_ctr <= latency_ctr - 1;
        if (latency_ctr == 1) begin
            m_axis_tdata <= macc_accum;
            m_axis_tvalid <= 1;
        end
    end

    // Set inputs to 0 unless we get new data. Accumulator always runs.
    macc_a <= 0;
    macc_b <= 0;

    sload_r <= 0;
    sload_r2 <= sload_r;
    macc_sload <= sload_r2;

    if (skid_tvalid && skid_tready) begin
        // Drive inputs to accumulator.
        macc_a <= skid_atdata;
        macc_b <= skid_btdata;

        // Reset the accumulator on the last sample of the frame.
        sload_r <= skid_tlast;

        if (skid_tlast) begin
            latency_ctr <= LATENCY;
        end
    end

    if (rst) begin
        m_axis_tvalid <= 0;
        latency_ctr <= 0;
    end
end

endmodule
