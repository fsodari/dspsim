module SomeModel #(
    parameter DW = 32
) (
    input  logic          clk,
    input  logic          rst,
    input  logic [DW-1:0] i,
    output logic [DW-1:0] o
);

  always @(posedge clk) begin
    if (rst) begin
      o <= '0;
    end else begin
      o <= i;
    end
  end
endmodule
