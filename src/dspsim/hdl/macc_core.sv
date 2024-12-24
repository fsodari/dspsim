
module macc_core #(
    parameter ADW = 24,
    parameter BDW = 18,
    parameter ODW = 48
) (
    input  logic clk,
    input  logic ce,
    input  logic sload,
    input  logic signed [ADW-1:0] a,
    input  logic signed [BDW-1:0] b,
    output logic signed [ODW-1:0] accum_o
);

localparam MDW = ADW + BDW;

logic signed [ADW-1:0] a_reg;
logic signed [BDW-1:0] b_reg;
logic sload_reg;
logic signed [MDW-1:0] mult_reg;
logic signed [ODW-1:0] adder_out, old_result;

always_comb begin
    if (sload_reg) begin
        old_result = 0;
    end else begin
        old_result = adder_out;
    end
end

always @(posedge clk) begin
    if (ce) begin
        a_reg <= a;
        b_reg <= b;
        mult_reg <= a_reg * b_reg;
        sload_reg <= sload;

        /* verilator lint_off WIDTHEXPAND */
        adder_out <= old_result + $signed(mult_reg);
        /* verilator lint_on WIDTHEXPAND */
    end
end

assign accum_o = adder_out;

endmodule
