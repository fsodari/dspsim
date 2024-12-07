module SomeModel #(
    parameter DW = 16,
    parameter int COEFW = 18,
    parameter int unsigned AW = 16,
    parameter X = -100,
    parameter BB = 3'b010,
    parameter FP = 3.14,
    parameter string SP = "SomeString",
    parameter string TRICK = "SSSS", // You must declare string, otherwise it's going to look like an int.
    parameter int IARR[3] = '{1, 2, 3},
    parameter string SARR[2] = '{"foo", "bar"},
    parameter int IMAT[2][3] = '{'{1,2,3}, '{4,5,6}},
    parameter string SMAT[2][2][2] = '{'{'{"a", "b"}, '{"c", "d"}}, '{'{"e", "f"}, {"g", "h"}}},
    parameter NC = 6,
    parameter ND = 4,
    parameter MD = 3,
    parameter NE = 1,
    parameter ME = 2
) (
    input  logic clk,
    input  logic rst,
    input  logic [DW-1:0] x,
    output logic [DW-1:0] y,
    input logic [COEFW-1:0] c[NC],
    output logic [COEFW-1:0] d[ND][MD],
    input logic [COEFW-1:0] e[NE][ME]
);

always @(posedge clk) begin
    y <= x;

    if (rst) begin
        y <= 0;
    end
end

endmodule
