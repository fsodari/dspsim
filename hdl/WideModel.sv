// Ports of more than 64 bits. Verilator stores them as 32-bit words, and dspsim presents them as arrays of words.
module WideModel #(
    parameter int W = 100,
    parameter int N = 3
) (
    input  logic                clk,
    input  logic [W-1:0]        a,
    input  logic [W-1:0]        b,
    // W + 1 bits: the carry shows input bits that should have been truncated.
    output logic [W:0]          sum,
    input  logic signed [W-1:0] s,
    output logic signed [W-1:0] neg,
    input  logic [127:0]        arr_in [N],
    output logic [127:0]        arr_out[N]
);

  always_ff @(posedge clk) begin
    sum <= a + b;
    neg <= -s;
    for (int i = 0; i < N; i++) begin
      arr_out[i] <= ~arr_in[i];
    end
  end

endmodule
