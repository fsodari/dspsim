module SimpleModel #(
    parameter int DW = 8
) (
    input logic clk,
    input logic rst,
    input logic [DW-1:0] i,
    output logic [DW-1:0] o
);

  always @(posedge clk) begin
    if (rst) begin
      o <= {DW{1'b0}};
    end else begin
      o <= i;
    end
  end
endmodule
