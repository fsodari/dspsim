module NDArrayModel #(
    parameter int WIDTH = 8,
    parameter int M = 2,
    parameter int N = 3,
    parameter int O = 4
)
(
    input logic clk,
    input logic rst,
    input logic [WIDTH-1:0] a [M][N][O],
    output logic [WIDTH-1:0] b [M][N][O]
);

always @(posedge clk) begin
    for (int i = 0; i < M; i = i + 1) begin
        for (int j = 0; j < N; j = j + 1) begin
            for (int k = 0; k < O; k = k + 1) begin
                b[i][j][k] <= a[i][j][k];
            end
        end
    end
    if (rst) begin
        for (int i = 0; i < M; i = i + 1) begin
            for (int j = 0; j < N; j = j + 1) begin
                for (int k = 0; k < O; k = k + 1) begin
                    b[i][j][k] <= '0;
                end
            end
        end
    end
end

endmodule
