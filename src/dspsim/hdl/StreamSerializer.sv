module StreamSerializer #(
    parameter DW = 24,
    parameter N = 4,
    parameter TIDW = 8
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata[N],
    input  logic s_axis_tvalid[N],
    output logic s_axis_tready[N],

    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,
    output logic [TIDW-1:0] m_axis_tid,
    output logic m_axis_tlast
);

localparam IDW = $clog2(N);

logic [IDW-1:0] stream_id = 0;

logic [DW-1:0] combine_tdata [N];
logic combine_tvalid, combine_tready = 1;
Combiner #(.DW(DW), .N(N)) combine_i (
    .clk,
    .rst,
    .s_axis_tdata,
    .s_axis_tvalid,
    .s_axis_tready,
    .m_axis_tdata(combine_tdata),
    .m_axis_tvalid(combine_tvalid),
    .m_axis_tready(combine_tready)
);

always @(posedge clk) begin
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
        m_axis_tlast <= 0;
        m_axis_tid <= 0;

        if (!combine_tready) begin
            // Last
            m_axis_tdata <= combine_tdata[stream_id];
            m_axis_tvalid <= 1;
            m_axis_tid <= TIDW'(stream_id);
            if (stream_id == IDW'(N-1)) begin
                m_axis_tlast <= 1;
                combine_tready <= 1;
            end
        end
    end

    

    if (combine_tvalid && combine_tready) begin
        combine_tready <= 0;

        m_axis_tdata <= combine_tdata[0];
        m_axis_tvalid <= 1;
        m_axis_tid <= 0;
        m_axis_tlast <= 0;
        stream_id <= 1;
    end
end

endmodule
