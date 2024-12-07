/*

    Data changes on falling edge, and is read on rising.
*/
module I2STx #(
    parameter DW = 24,
    parameter USE_INPUT_BUFFER = 1,
    localparam TIDW = 8
) (
    input  logic clk, // mclk
    input  logic rst,

    // Stream input
    input  logic [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    /* verilator lint_off unused */
    input  logic [TIDW-1:0] s_axis_tid, // 0: L, 1: R. Both L+R should be in one stream.
    /* verilator lint_on unused */

    // I2S Output
    input  logic lrclk,
    input  logic sclk,
    output logic sdo
);

logic sclk_prev = 0;
logic lrclk_prev = 0;

logic [DW-1:0] sdo_reg;
logic [DW-1:0] tx_data[2];
logic [1:0] tx_data_valid = 0;

logic [DW-1:0] skid_tdata;
logic skid_tid;
logic skid_tvalid, skid_tready = 0;
generate
if (USE_INPUT_BUFFER != 0) begin : use_input_buffer
    Skid #(.DW(DW+1)) skid_i (
        .clk(clk),
        .rst(rst),
        .s_axis_tdata({s_axis_tdata,s_axis_tid[0]}),
        .s_axis_tvalid(s_axis_tvalid),
        .s_axis_tready(s_axis_tready),
        .m_axis_tdata({skid_tdata, skid_tid}),
        .m_axis_tvalid(skid_tvalid),
        .m_axis_tready(skid_tready)
    );
end else begin : no_use_input_buffer
    assign skid_tdata = s_axis_tdata;
    assign skid_tvalid = s_axis_tvalid;
    assign s_axis_tready = skid_tready;
    assign skid_tid = s_axis_tid[0];
end
endgenerate

// I2S.
logic read_l, read_r;

always @(posedge clk) begin
    sclk_prev <= sclk;

    // Rising edge.
    if (sclk && !sclk_prev) begin
        lrclk_prev <= lrclk;

        // Falling edge of lrclk. Left channel
        if (!lrclk && lrclk_prev) begin
            sdo_reg <= tx_data[0];
        end else if (lrclk && !lrclk_prev) begin
            sdo_reg <= tx_data[1];
        end

    end

    // Falling edge
    if (!sclk && sclk_prev) begin
        {sdo, sdo_reg} <= {sdo_reg, 1'b0};
    end

    if (rst) begin
        sdo_reg <= 0;
        sdo <= 0;
    end
end

assign read_l = (sclk && !sclk_prev) && (!lrclk && lrclk_prev);
assign read_r = (sclk && !sclk_prev) && (lrclk && !lrclk_prev);

// Stream
assign skid_tready = !tx_data_valid[skid_tid];
always @(posedge clk) begin

    if (tx_data_valid[0] && read_l) begin
        tx_data_valid[0] <= 0;
    end
    if (tx_data_valid[1] && read_r) begin
        tx_data_valid[1] <= 0;
    end

    if (skid_tvalid && skid_tready) begin
        tx_data[skid_tid] <= skid_tdata;
        tx_data_valid[skid_tid] <= 1;
    end

    if (rst) begin
        tx_data <= '{0, 0};
        tx_data_valid <= '{0, 0};
    end
end
endmodule
