module Combiner #(
    parameter DW = 24,
    parameter N = 2
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata[N],
    input  logic s_axis_tvalid[N],
    output logic s_axis_tready[N],

    output logic [DW-1:0] m_axis_tdata[N],
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

logic all_ready, all_valid;

logic [DW-1:0] skid_tdata[N];
logic [N-1:0] skid_tvalid;

assign all_valid = &skid_tvalid;
assign all_ready = all_valid && (!m_axis_tvalid || m_axis_tready);

generate
    for (genvar i = 0; i < N; i = i + 1) begin : skid_inputs
        Skid #(.DW(DW)) skid_i (
            .clk,
            .rst,
            .s_axis_tdata(s_axis_tdata[i]),
            .s_axis_tvalid(s_axis_tvalid[i]),
            .s_axis_tready(s_axis_tready[i]),
            .m_axis_tdata(skid_tdata[i]),
            .m_axis_tvalid(skid_tvalid[i]),
            .m_axis_tready(all_ready)
        );
    end
endgenerate

always @(posedge clk) begin
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    if (all_valid && all_ready) begin
        for (int i = 0; i < N; i = i + 1) begin
            m_axis_tdata[i] <= skid_tdata[i];
        end
        m_axis_tvalid <= 1;
    end

    if (rst) begin
        m_axis_tvalid <= 0;
    end
end
    
endmodule
