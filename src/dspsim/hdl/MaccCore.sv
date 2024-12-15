/*
    Multiply-Accumulate. Based on the Vivado language template for implementing with a DSP slice.
*/
module MaccCore #(
    parameter ADW = 24, // Input A data width
    parameter BDW = 18, // Input B data width
    parameter ODW = 48  // Output data width
) (
    input  logic clk,
    input  logic ce,
    input  logic sload,
    input  logic signed [ADW-1:0] a,
    input  logic signed [BDW-1:0] b,
    output logic signed [ODW-1:0] accum_out
);

// Multiplier width
localparam MW = ADW + BDW;

// Sign-extend the multiplier output when adding to the accumulator.
localparam MPAD = ODW - MW;

// Declare registers for intermediate values
logic signed [ADW-1:0] a_reg;
logic signed [BDW-1:0] b_reg;
logic sload_reg;
logic signed [MW-1:0] mult_reg;
logic signed [ODW-1:0] mult_sign_extend;
logic signed [ODW-1:0] adder_out, old_result;

assign mult_sign_extend = {{MPAD{mult_reg[MW-1]}}, mult_reg};

always_comb begin
    if (sload_reg) begin
        old_result = 0;
    end else begin
        // 'sload' is now and opens the accumulation loop.
        // The accumulator takes the next multiplier output
        // in the same cycle.
        old_result = adder_out;
    end
end

always @(posedge clk) begin
    if (ce) begin
        a_reg <= a;
        b_reg <= b;
        mult_reg <= a_reg * b_reg;
        sload_reg <= sload;
        // Store accumulation result into a register
        adder_out <= old_result + mult_sign_extend;
    end
end

// Output accumulation result
assign accum_out = adder_out;


endmodule
