module Skid #(
    parameter DW = 24
) (
    input  logic clk,
    input  logic rst,

    input  logic [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    output logic [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready
);

logic [DW-1:0] skid_tdata = 0;
logic skid_tvalid = 0;

// Can accept data whenever the skid buffer is not full.
assign s_axis_tready = !skid_tvalid;

// Skid buffer
always @(posedge clk) begin
    // Skid data was read out.
    if (skid_tvalid && (!m_axis_tvalid || m_axis_tready)) begin
        skid_tvalid <= 0;
    end

    // Incoming data is valid and the output is stalled. Load the skid buffer.
    if ((s_axis_tvalid && s_axis_tready) && (m_axis_tvalid && !m_axis_tready)) begin
        // Incoming data is valid but the output is stalled.
        skid_tdata <= s_axis_tdata;
        skid_tvalid <= 1;
    end

    if (rst) begin
        skid_tdata <= 0;
        skid_tvalid <= 0;
    end
end

// Output buffer. Select from skid buffer or input stream.
always @(posedge clk) begin
    // Output data was accepted.
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    // If we have data and the output stream is ready.
    if ((skid_tvalid || s_axis_tvalid) && (!m_axis_tvalid || m_axis_tready)) begin
        // If the skid has data, read from it, otherwise the input stream data.
        m_axis_tdata <= skid_tvalid ? skid_tdata : s_axis_tdata;
        m_axis_tvalid <= 1;
    end

    if (rst) begin
        m_axis_tdata <= 0;
        m_axis_tvalid <= 0;
    end
end

endmodule
