/*
    Synchronous FIFO. Write and read streams share a clock.
*/
module FifoSync #(
    parameter DW = 24,
    parameter N = 8,
    parameter TIDW = 8,
    parameter USE_INPUT_BUFFER = 0
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    input  logic [TIDW-1:0] s_axis_tid,

    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,
    output logic [TIDW-1:0] m_axis_tid
);

localparam AW = $clog2(N);
localparam MW = DW + TIDW;
// Memory
logic [MW-1:0] mem [N];

// Fifo wr/rd addresses. The counters have an extra bit to check the full/empty status.
logic [AW:0] rd_ctr = 0, wr_ctr = 0;
logic [AW-1:0] rd_addr, wr_addr;
logic full, empty;

assign rd_addr = rd_ctr[AW-1:0];
assign wr_addr = wr_ctr[AW-1:0];

assign empty = rd_ctr == wr_ctr;
assign full = {~rd_ctr[AW], rd_ctr[AW-1:0]} == wr_ctr;

// Input Buffer.
logic [MW-1:0] skid_tdata;
logic skid_tvalid, skid_tready;

/*
 Optionally use a skid buffer on the input, otherwise use the input stream directly.
 There won't be a combinational path from output to input either way, so it should be fine to not use.
 */
generate
if (USE_INPUT_BUFFER) begin : use_input_buffer
Skid #(.DW(MW)) skid_i (
    .clk(clk),
    .rst(rst),
    .s_axis_tdata({s_axis_tdata, s_axis_tid}),
    .s_axis_tvalid(s_axis_tvalid),
    .s_axis_tready(s_axis_tready),
    .m_axis_tdata(skid_tdata),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);
end else begin : no_use_input_buffer
    assign skid_tdata = {s_axis_tdata, s_axis_tid};
    assign skid_tvalid = s_axis_tvalid;
    assign s_axis_tready = skid_tready;
end
endgenerate

// Write
assign skid_tready = !full;
always @(posedge clk) begin
    if (skid_tvalid && skid_tready) begin
        mem[wr_addr] <= skid_tdata;
        wr_ctr <= wr_ctr + 1;
    end

    if (rst) begin
        wr_ctr <= 0;
    end
end

// Read
always @(posedge clk) begin
    // Output data was accepted.
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // There is data in the fifo, and the output is not stalled.
    if (!empty && (!m_axis_tvalid || m_axis_tready)) begin
        {m_axis_tdata, m_axis_tid} <= mem[rd_addr];
        m_axis_tvalid <= 1;

        rd_ctr <= rd_ctr + 1;
    end

    if (rst) begin
        rd_ctr <= 0;
    end
end

endmodule
