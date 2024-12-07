module I2SRx #(
    parameter DW = 24,
    localparam TIDW = 8
) (
    input  logic clk, // mclk
    input  logic rst,

    // Stream output
    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,
    output logic [TIDW-1:0] m_axis_tid,

    // I2S input
    input  logic lrclk,
    input  logic sclk,
    input  logic sdi
);

localparam CTRW = $clog2(DW);

logic sclk_prev = 0, lrclk_prev = 0;
logic lrclk_rise_r = 0, lrclk_fall_r = 0;

logic [DW-1:0] sdi_reg = 0;
logic [CTRW-1:0] sdi_ctr = 0;
logic channel_waiting = 0;

always @(posedge clk) begin
    sclk_prev <= sclk;

    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    if (sclk && !sclk_prev) begin
        lrclk_prev <= lrclk;
        lrclk_rise_r <= lrclk && !lrclk_prev;
        lrclk_fall_r <= !lrclk && lrclk_prev;

        sdi_reg <= {sdi_reg[DW-2:0], sdi};
        sdi_ctr <= sdi_ctr + 1;

        if (lrclk_fall_r || lrclk_rise_r) begin
            channel_waiting <= lrclk_fall_r ? 0 : 1;
            sdi_ctr <= 0;
        end

        // Received full sample.
        if (sdi_ctr == CTRW'(DW-1)) begin
            // m_axis_tdata <= {sdi_reg[DW-2:0], sdi};
            m_axis_tdata <= sdi_reg;
            m_axis_tvalid <= 1;
            m_axis_tid <= {7'd0, channel_waiting};
        end
    end

    if (rst) begin
        m_axis_tdata <= 0;
        m_axis_tvalid <= 0;
        m_axis_tid <= 0;

        sdi_ctr <= 0;
    end
end
endmodule
