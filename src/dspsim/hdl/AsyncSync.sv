/*

*/

module AsyncSync #(
    parameter DW = 8,
    parameter SYNC_STAGES = 2,
    parameter PIPELINE_STAGES = 0
) (
    input  logic clk,

    input  logic [DW-1:0] d,
    output logic [DW-1:0] q
);

(* ASYNC_REG="TRUE" *) logic [DW-1:0] sreg [SYNC_STAGES-1:0];

always @(posedge clk) begin
    sreg <= {sreg[SYNC_STAGES-2:0], d};
end

generate
    if (PIPELINE_STAGES==0) begin: no_pipeline

        assign q = sreg[SYNC_STAGES-1];

    end else if (PIPELINE_STAGES==1) begin: one_pipeline

        logic [DW-1:0] sreg_pipe = 0;

        always @(posedge clk) begin
            sreg_pipe <= sreg[SYNC_STAGES-1];
        end

        assign q = sreg_pipe;

    end else begin: multiple_pipeline

    (* shreg_extract = "no" *) logic [DW-1:0] sreg_pipe [PIPELINE_STAGES-1:0];

        always @(posedge clk) begin
            sreg_pipe <= {sreg_pipe[PIPELINE_STAGES-2:0], sreg[SYNC_STAGES-1]};
        end

        assign q = sreg_pipe[PIPELINE_STAGES-1];

    end
endgenerate


endmodule
