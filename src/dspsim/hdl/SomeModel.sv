module SomeModel #(
    parameter DW = 24
) (
    input  logic clk,
    input  logic rst,
    input  logic [DW-1:0] x,
    output logic [DW-1:0] y
);

always @(posedge clk) begin
    y <= x;

    if (rst) begin
        y <= 0;
    end
end

endmodule
