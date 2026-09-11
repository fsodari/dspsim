module HellModel #(
    parameter int DW = 24,
    parameter unsigned UW = 8,
    parameter real F = 3.14,
    parameter string STR_DEF = "default",
    parameter string USTR = "some string",
    parameter bit B = 1'b1,
    parameter logic signed [3:0] L = 4'b0111,
    parameter M = 2,
    parameter N = 3,
    parameter O = 4,
    parameter int ONEARR[1] = '{9},
    parameter int V[M] = '{1, 2},
    parameter int W[M][N] = '{'{1, 2, 3}, '{4, 5, 6}},
    parameter int X[M][N][O] = '{
        '{'{1, 2, 3, 4}, '{5, 6, 7, 8}, '{9, 10, 11, 12}},
        '{'{13, 14, 15, 16}, '{17, 18, 19, 20}, '{21, 22, 23, 24}}
    }
) (
    input logic clk,
    input logic rst,

    input logic [DW-1:0] a,
    output logic [UW-1:0] b,
    input logic signed [B-1:0] c,
    output logic signed [L-1:0] d,
    output logic signed [DW-1:0] e[M],
    output logic signed [DW-1:0] f[M][N],
    output logic [DW-1:0] g[M][N][O]
);

  localparam SOME_LOCAL_PARAM = 99;

  generate
    // Add your generate blocks here if needed
    for (genvar i = 0; i < M; i++) begin : gen_e
      assign e[i] = DW'(V[i]);
      for (genvar j = 0; j < N; j++) begin : gen_f
        assign f[i][j] = DW'(W[i][j]);

        for (genvar k = 0; k < O; k++) begin : gen_g
          assign g[i][j][k] = DW'(X[i][j][k]);
        end
      end
    end
  endgenerate

endmodule
