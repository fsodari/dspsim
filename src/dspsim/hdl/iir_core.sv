module iir_core #(
    parameter DW = 24,
    parameter COEFW = 18,
    parameter COEFQ = 16,
    parameter ORDER = 2,
    localparam N = (ORDER+1)*2
) (
    input  logic clk,
    input  logic rst,

    // Input stream
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    // Output stream.
    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // Coefficients. Normalized, fixed-point.
    input  logic signed [COEFW-1:0] coefs [N]
);

localparam NX = ORDER + 1;

localparam IDW = $clog2(N + 1);
localparam MW = DW + COEFW;
localparam ACCUMW = MW + $clog2(N);

logic signed [DW-1:0] skid_tdata;
logic skid_tvalid, skid_tready;

Skid #(.DW(DW)) skid_i (
    .clk,
    .rst,
    .s_axis_tdata,
    .s_axis_tvalid,
    .s_axis_tready,
    .m_axis_tdata(skid_tdata),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);

logic signed [DW-1:0] macc_atdata;
logic macc_tvalid, macc_tready, macc_tlast;
logic signed [COEFW-1:0] macc_btdata;
logic signed [ACCUMW-1:0] accum_tdata;


// Fraction saving and output. The fraction is always positive, so add 0 at the beginning so it's not negative.
logic signed [COEFW-1:0] fraction_save;
assign fraction_save = {{(COEFW-COEFQ){1'b0}}, accum_tdata[COEFQ-1:0]};

// Signed fraction. This is wrong?
/* verilator lint_off WIDTHEXPAND */
// assign fraction_save = $signed(accum_tdata[COEFQ-1:0]);
/* verilator lint_on WIDTHEXPAND */

// No fraction saving
// assign fraction_save = 0;

/*
    Store the saved fraction in the coefficient register.
    Set state_vars[NX] to 1.
*/
/* verilator lint_off WIDTHTRUNC */
assign m_axis_tdata = $signed(accum_tdata >> COEFQ);
/* verilator lint_on WIDTHTRUNC */

Macc #(
    .ADW(DW),
    .BDW(COEFW),
    .ODW(ACCUMW)
) macc_i (
    .clk,
    .rst,

    .s_axis_atdata(macc_atdata),
    .s_axis_btdata(macc_btdata),
    .s_axis_tvalid(macc_tvalid),
    .s_axis_tready(macc_tready),
    .s_axis_tlast(macc_tlast),

    .m_axis_tdata(accum_tdata),
    .m_axis_tvalid(m_axis_tvalid),
    .m_axis_tready(m_axis_tready)
);

logic signed [DW-1:0] state_vars [N];
logic signed [COEFW-1:0] coefs_adj [N];
generate
    for (genvar i = 0; i < N; i = i + 1) begin : adjusted_coefficients
        if (i < NX) begin : x_coefs
            assign coefs_adj[i] = coefs[i];
        end else if (i == NX) begin : fraction_save_coefficient
            assign coefs_adj[i] = fraction_save;
        end else begin : y_coefs
            // Negate these coefficients.
            assign coefs_adj[i] = -coefs[i];
        end
    end
endgenerate


logic [IDW-1:0] state_id = 0;
logic busy = 0;
assign skid_tready = !busy && (!m_axis_tvalid || m_axis_tready);

always @(posedge clk) begin

    // Output was accepted, we're not busy anymore
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;

        busy <= 0;
        state_vars[NX] <= m_axis_tdata; // Save the output into the state vars.
    end

    // Send data through macc
    if (macc_tvalid && macc_tready) begin
        macc_tvalid <= 0;
        macc_tlast <= 0;

        if (state_id < IDW'(N)) begin
            state_id <= state_id + 1;

            macc_atdata <= state_vars[state_id];
            macc_btdata <= coefs_adj[state_id];
            macc_tvalid <= 1;

            if (state_id == IDW'(N-1)) begin
                macc_tlast <= 1;
            end
        end
    end

    // New data
    if (skid_tvalid && skid_tready) begin
        busy <= 1; // Set busy until we're done.
        
        // Load and shift state variables.
        state_vars[0] <= skid_tdata;
        for (int i = 1; i < N; i = i + 1) begin
            state_vars[i] <= state_vars[i-1];
            if (i == NX) begin
                state_vars[i] <= 1; // fraction saving.
            end
        end

        // Send the first computation.
        macc_atdata <= skid_tdata;
        macc_btdata <= coefs_adj[0];
        macc_tvalid <= 1;
        macc_tlast <= 0;

        // Set the next state_id
        state_id <= 1;
    end

    if (rst) begin
        state_id <= 0;
        m_axis_tvalid <= 0;
    end
end

endmodule
