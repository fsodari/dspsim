module Macc2 #(
    parameter ADW = 24,
    parameter BDW = 18,
    parameter ODW = 48
) (
    input  logic clk,
    input  logic rst,

    // Input A stream input. when tlast goes high, the accumulator is read out and reset.
    input  logic signed [ADW-1:0] s_axis_atdata,
    input  logic s_axis_atvalid,
    output logic s_axis_atready,
    input  logic s_axis_atlast,

    input  logic signed [BDW-1:0] s_axis_btdata,
    input  logic s_axis_btvalid,
    output logic s_axis_btready,
    /* verilator lint_off UNUSED */
    input  logic s_axis_btlast,
    /* verilator lint_on UNUSED */
    /*
        The a channel drives tlast. Assumes/requires that a and b have the same frame sizes.
        I'm not sure how or if I could handle an error if tlast is not aligned?
    */

    // Output of the accumulator after a complete frame is received.
    output logic signed [ODW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

logic signed [ADW-1:0] cmb_atdata;
logic signed [BDW-1:0] cmb_btdata;
logic cmb_atvalid, cmb_btvalid, cmb_tready, cmb_atlast;

// // Synchronize the two input streams.
// combine2 #(.ADW(ADW + 1), .BDW(BDW)) combine_i (
//     .clk,
//     .rst,
//     .s_axis_atdata({s_axis_atdata, s_axis_atlast}),
//     .s_axis_atvalid,
//     .s_axis_atready,

//     .s_axis_btdata(s_axis_btdata),
//     .s_axis_btvalid,
//     .s_axis_btready,

//     .m_axis_atdata({cmb_atdata, cmb_atlast}),
//     .m_axis_btdata(cmb_btdata),
//     .m_axis_tvalid(cmb_tvalid),
//     .m_axis_tready(cmb_tready)
// );

logic cmb_tvalid;
assign cmb_tvalid = cmb_atvalid && cmb_btvalid;
Skid #(.DW(ADW + 1)) skid_a_i (
    .clk,
    .rst,
    .s_axis_tdata({s_axis_atdata, s_axis_atlast}),
    .s_axis_tvalid(s_axis_atvalid),
    .s_axis_tready(s_axis_atready),
    .m_axis_tdata({cmb_atdata, cmb_atlast}),
    .m_axis_tvalid(cmb_atvalid),
    .m_axis_tready(cmb_tready && cmb_tvalid)
);
Skid #(.DW(BDW)) skid_b_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axis_btdata),
    .s_axis_tvalid(s_axis_btvalid),
    .s_axis_tready(s_axis_btready),
    .m_axis_tdata(cmb_btdata),
    .m_axis_tvalid(cmb_btvalid),
    .m_axis_tready(cmb_tready && cmb_tvalid)
);


// Pass the synced streams to the base Macc component.
Macc #(.ADW(ADW), .BDW(BDW), .ODW(ODW)) macc_i (
    .clk,
    .rst,
    .s_axis_atdata(cmb_atdata),
    .s_axis_btdata(cmb_btdata),
    .s_axis_tvalid(cmb_tvalid),
    .s_axis_tready(cmb_tready),
    .s_axis_tlast(cmb_atlast),
    .m_axis_tdata,
    .m_axis_tvalid,
    .m_axis_tready
);

endmodule
