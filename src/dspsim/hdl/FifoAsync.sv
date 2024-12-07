/*
    Asynchronous FIFO. Write and Read streams have independent clocks. Useful for clock-domain-crossing.
    Uses gray code addressing with 2ff synchronizers.
*/
module FifoAsync #(
    parameter DW = 24,
    parameter N = 8,
    parameter TIDW = 8,
    parameter INPUT_BUFFER = 1,
    parameter SYNC_STAGES = 2,
    parameter PIPELINE_STAGES = 0
) (
    // Write port.
    input  logic clka,
    input  logic rsta,

    input  logic [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    input  logic [TIDW-1:0] s_axis_tid,

    // Read port
    input  logic clkb,
    input  logic rstb,

    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,
    output logic [TIDW-1:0] m_axis_tid
);

// clka == wr_clk, clkb == rd_clk;
localparam MW = DW + TIDW;
localparam AW = $clog2(N);

// Memory
logic [MW-1:0] mem [N];

// Fifo wr/rd addresses. The counters have an extra bit to check the full/empty status.
logic [AW:0] rd_ctr = 0, wr_ctr = 0;
logic [AW:0] rd_gray, wr_gray;
logic [AW:0] rd_gray_sync, wr_gray_sync;
// logic [AW:0] rd_ctr_sync, wr_ctr_sync;

logic [AW-1:0] rd_addr, wr_addr;
logic full, empty;

assign rd_addr = rd_ctr[AW-1:0];
assign wr_addr = wr_ctr[AW-1:0];

// Full is on clka domain with wr_gray
assign full = (wr_gray[AW:AW-1] == ~rd_gray_sync[AW:AW-1]) && (wr_gray[AW-2:0] == rd_gray_sync[AW-2:0]);

// Empty is on clkb domain with rd_gray
assign empty = rd_gray == wr_gray_sync;

/*
(wgray[AW:AW-1] == ~wq2_rgray[AW:AW-1])
                                && (wgray[AW-2:0]==wq2_rgray[AW-2:0]);
*/

// assign full = {~rd_ctr[AW], rd_ctr[AW-1:0]} == wr_ctr_sync;

assign rd_gray = (rd_ctr >> 1) ^ rd_ctr;
assign wr_gray = (wr_ctr >> 1) ^ wr_ctr;

// Syncronize rd_gray to clka
AsyncSync #(
    .DW(AW+1),
    .SYNC_STAGES(SYNC_STAGES),
    .PIPELINE_STAGES(PIPELINE_STAGES)
) rd_gray_sync_i (
    .clk(clka),
    .d(rd_gray),
    .q(rd_gray_sync)
);
// Synchronize wr_gray to clkb
AsyncSync #(
    .DW(AW+1),
    .SYNC_STAGES(SYNC_STAGES),
    .PIPELINE_STAGES(PIPELINE_STAGES)
) wr_gray_sync_i (
    .clk(clkb),
    .d(wr_gray),
    .q(wr_gray_sync)
);

// Input buffer
logic [MW-1:0] skid_tdata;
logic skid_tvalid, skid_tready;
generate
if (INPUT_BUFFER != 0) begin : use_input_buffer
    Skid #(.DW(MW)) skid_i (
        .clk(clka),
        .rst(rsta),
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

// Write port
assign skid_tready = !full;
always @(posedge clka) begin

    if (skid_tvalid && skid_tready) begin
        mem[wr_addr] <= skid_tdata;
        wr_ctr <= wr_ctr + 1;
    end

    if (rsta) begin
        wr_ctr <= 0;
    end
end

// Read port
always @(posedge clkb) begin
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // The fifo has data, and the output is not stalled.
    if (!empty && (!m_axis_tvalid || m_axis_tready)) begin
        {m_axis_tdata, m_axis_tid} <= mem[rd_addr];
        m_axis_tvalid <= 1;

        rd_ctr <= rd_ctr + 1;
    end

    if (rstb) begin
        m_axis_tdata <= 0;
        m_axis_tvalid <= 0;
        rd_ctr <= 0;
    end
end
endmodule
