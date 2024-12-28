module Spis #(
    parameter CPOL = 0,
    parameter CPHA = 0
) (
    input  logic clk,
    input  logic rst,

    // Miso data stream
    input  logic [7:0] s_axis_tdata,
    input  logic s_axis_tvalid, 
    output logic s_axis_tready,

    // Mosi data stream
    output logic [7:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // SPI signals
    input  logic cs,
    input  logic sclk,
    input  logic mosi,
    output logic miso
);

localparam IDW = $clog2(8);
localparam IDLAST = IDW'(7);

logic cs_prev = 0;
logic sclk_prev = 0;
logic sclk_rise, sclk_fall;
logic update_data, sample_data;

logic [IDW-1:0] mosi_ctr = 0, miso_ctr = 0;
logic [7:0] mosi_reg = 0, miso_reg = 0;
logic update_output = 0;

assign sclk_rise = sclk && !sclk_prev;
assign sclk_fall = !sclk && sclk_prev;

assign sample_data = (CPOL == CPHA) ? sclk_rise : sclk_fall;
assign update_data = (CPOL != CPHA) ? sclk_rise : sclk_fall;

always @(posedge clk) begin
    cs_prev <= cs;
    sclk_prev <= sclk;

    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // If CPHA==0, data updates on falling cs
    if (!cs && ( ((CPHA == 0) && cs_prev) || update_data )) begin
        miso_ctr <= miso_ctr + 1;
        {miso, miso_reg} <= {miso_reg, 1'b0};

        if (miso_ctr == IDLAST) begin
            miso_ctr <= 0;
            s_axis_tready <= 1;
        end
    end

    if (s_axis_tvalid && s_axis_tready) begin
        miso_reg <= s_axis_tdata;
        s_axis_tready <= 0;
    end

    // sample mosi data.
    if (!cs && sample_data) begin
        mosi_ctr <= mosi_ctr + 1;
        // {1'b0, mosi_reg} <= {mosi_reg, mosi};
        mosi_reg <= {mosi_reg[6:0], mosi};
        if (mosi_ctr == IDLAST) begin
            mosi_ctr <= 0;
            update_output <= 1;
        end
    end
    if (update_output) begin
        m_axis_tdata <= mosi_reg;
        m_axis_tvalid <= 1;
        update_output <= 0;
    end

    //
    if (cs) begin
        mosi_ctr <= 0;
        miso_ctr <= 0;
        mosi_reg <= 0;
        miso_reg <= 0;
    end

    if (rst) begin
        s_axis_tready <= 1;
        m_axis_tvalid <= 0;
        update_output <= 0;
    end
end

endmodule
