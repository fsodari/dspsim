module priority_encoder #(
    parameter N = 4,
    parameter LSB_PRIORITY = 0,
    parameter IW = $clog2(N)
) (
    input  logic [N-1:0] d,
    output logic [N-1:0] q,
    output logic [IW-1:0] id,
    output logic valid
);

generate
always_comb begin
    id = 0;
    valid = 0;
    for (int i = 0; i < N; i = i + 1) begin
        q[i] = 0;
    end
    if (LSB_PRIORITY) begin
        for (int i = 0; i < N; i = i + 1) begin
            if (d[i]) begin
                q[i] = 1;
                id = IW'(i);
                valid = 1;
                break;
            end
        end
    end else begin
        for (int i = N-1; i >= 0; i = i - 1) begin
            if (d[i]) begin
                q[i] = 1;
                id = IW'(i);
                valid = 1;
                break;
            end
        end
    end
end
endgenerate

endmodule
