module SimpleModel (
    input logic clk,
    input logic rst,
    input logic [7:0] i,
    output logic [7:0] o
);

  always @(posedge clk) begin
    if (rst) begin
      o <= 8'b0;
    end else begin
      o <= i;
    end
  end
endmodule
