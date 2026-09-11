module HellModel #(
    parameter int DW = 24,
    parameter unsigned UW = 8,
    parameter real F = 3.14,
    parameter string STR_DEF = "default",
    parameter string S = "some_string",
    parameter M = 2,
    parameter N = 3,
    parameter O = 4,
    parameter int V[M] = '{1, 2},
    parameter W[M][N] = '{default: '{default: 0}},
    parameter X[M][N][O] = '{default: '{default: '{default: 0}}}
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] a,
    output logic [DW-1:0] b,
    input  logic signed [DW-1:0] c,
    output logic signed [DW-1:0] d,
    input  logic signed [DW-1:0] e[M],
    output logic signed [DW-1:0] f[M][N],
    input  logic signed [DW-1:0] g[M][N][O]

);

endmodule