/*
    Generate 
*/

module I2SClkGen #(
    parameter MCLK_LRCLK = 384, // 384 = 18.432e6 / 48e3
    parameter MCLK_SCLK = 8, // 8 = 384 / (2*24)
    parameter LRCLK_INIT = 0,
    parameter SCLK_INIT = 0
) (
    //
    input  logic clk,
    input  logic rst,

    // output logic mclk,
    output logic lrclk,
    output logic sclk
);

// Generate I2S Clocks. lrclk, sclk.
localparam LRCLK_CTRW = $clog2(MCLK_LRCLK);
localparam logic [LRCLK_CTRW-1:0] LRCLK_MAX = LRCLK_CTRW'(MCLK_LRCLK - 1);

localparam SCLK_CTRW = $clog2(MCLK_SCLK);
localparam SCLK_MAX = (MCLK_SCLK - 1);

initial lrclk = LRCLK_INIT;
initial sclk = SCLK_INIT;

logic [LRCLK_CTRW-1:0] lrclk_ctr = 0;
logic [SCLK_CTRW-1:0] sclk_ctr = 0;

// assign mclk = clk;

always @(posedge clk) begin

    lrclk_ctr <= lrclk_ctr + 1;
    sclk_ctr <= sclk_ctr + 1;

    // lrclk.
    if (lrclk_ctr == LRCLK_MAX) begin
        lrclk_ctr <= 0;
        lrclk <= ~lrclk;
    end

    // sclk.
    if (sclk_ctr == SCLK_MAX[SCLK_CTRW-1:0]) begin
        sclk_ctr <= 0;
        sclk <= ~sclk;
    end

    if (rst) begin
        lrclk_ctr <= 0;
        sclk_ctr <= 0;
        lrclk <= LRCLK_INIT;
        sclk <= SCLK_INIT;
    end
end


endmodule
