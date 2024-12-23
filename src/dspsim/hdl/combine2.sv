module combine2 #(
    parameter ADW = 24,
    parameter BDW = 18
) (
    input  logic clk,
    input  logic rst,

    input  logic [ADW-1:0] s_axis_atdata,
    input  logic s_axis_atvalid,
    output logic s_axis_atready,

    input  logic [BDW-1:0] s_axis_btdata,
    input  logic s_axis_btvalid,
    output logic s_axis_btready,

    output logic [ADW-1:0] m_axis_atdata,
    output logic [BDW-1:0] m_axis_btdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

logic all_ready;
logic both_valid;

// Skid inputs
logic [ADW-1:0] skid_atdata;
logic [BDW-1:0] skid_btdata;
logic skid_atvalid, skid_btvalid;

Skid #(.DW(ADW)) skid_a_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axis_atdata),
    .s_axis_tvalid(s_axis_atvalid),
    .s_axis_tready(s_axis_atready),
    .m_axis_tdata(skid_atdata),
    .m_axis_tvalid(skid_atvalid),
    .m_axis_tready(all_ready)
);
Skid #(.DW(BDW)) skid_b_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axis_btdata),
    .s_axis_tvalid(s_axis_btvalid),
    .s_axis_tready(s_axis_btready),
    .m_axis_tdata(skid_btdata),
    .m_axis_tvalid(skid_btvalid),
    .m_axis_tready(all_ready)
);

assign both_valid = skid_atvalid && skid_btvalid;
// We can accept new data when the output is not stalled and both inputs have data.
assign all_ready = both_valid && (!m_axis_tvalid || m_axis_tready);

always @(posedge clk) begin
    // Accepted output transaction.
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // New data.
    if (both_valid && all_ready) begin
        m_axis_atdata <= skid_atdata;
        m_axis_btdata <= skid_btdata;
        m_axis_tvalid <= 1;
    end

    if (rst) begin
        m_axis_tvalid <= 0;
    end
end

endmodule
