/*
    Matrix Mixer.

    performs the matrix operation y = Ax where
    x is an Nx1 matrix of input stream data.
    A is an MxN matrix of gain coefficients.
    y is an Mx1 matrix of the output stream data.

    Each output channel can have independent gains for all input sources.

    Pseudo-implementation

    for (int i = 0; i < M; i++)
    {
        output[i] = 0;
        for (int j = 0; j < N; j++)
        {
            output[i] += input[j] * coefs[i*N + j];
        }
    }

    This is implemented with a single Macc module. So the total time to compute all outputs will be MxN clock cycles.

    Coefficients are stored in row-major or c-contiguous format.
    For example, a 3 input, 2 output matrix would look like this.
    [
      [y0a0, y0a1, y0a2],
      [y1g0, y1g1, y1g2],
    ]
    And coefficients, in contiguous memory would look like this
    [y0a0, y0a1, y0a2, y1g0, y1g1, y1g2]

    Usage

    Combine basic streams into a serialized stream.
    Matrix operates on a serialized stream and outputs a serialized stream.
    Deserialize the matrix output back to basic streams.

    i2s_in0_l ->                                            -> i2s_out0_l
    i2s_in0_r ->                                            -> i2s_out0_r
    i2s_in1_l -> Serializer -> Matrix Mixer -> Deserializer -> i2s_out1_l
    i2s_in1_r ->                                            -> i2s_out1_r

*/
module Mixer #(
    parameter DW = 24,
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter COEFW = 18,
    parameter COEFQ = 16,
    parameter M = 2,        // Number of outputs
    parameter N = 4,        // Number of inputs
    parameter TIDW = 8
) (
    input  logic clk,
    input  logic rst,

    // Input stream
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [TIDW-1:0] s_axis_tid,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic s_axis_tlast,

    // Output stream.
    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,
    output logic [TIDW-1:0] m_axis_tid,
    output logic m_axis_tlast,

    // Coefficients through config bus.
    input  logic cyc_i,
    input  logic stb_i,
    input  logic we_i,
    output logic ack_o,
    output logic stall_o,
    input  logic [CFGAW-1:0] addr_i,
    input  logic signed [CFGDW-1:0] data_i,
    output logic signed [CFGDW-1:0] data_o
);

localparam NIDW = $clog2(N);
localparam MIDW = $clog2(M);

localparam NLAST = NIDW'(N-1);
localparam MLAST = MIDW'(M-1);

localparam NCOEF = M * N;


localparam IDW = $clog2(NCOEF);

// Wishbone registers for coefficients.
logic signed [COEFW-1:0] coefs [NCOEF];
WbRegs #(
    .CFGAW(CFGAW),
    .CFGDW(CFGDW),
    .REGW(COEFW),
    .N_CTL(NCOEF),
    .N_STS(0),
    .SIGN_EXTEND(1)
) wb_regs_i (
    .clk,
    .rst,
    .cyc_i,
    .stb_i,
    .we_i,
    .ack_o,
    .stall_o,
    .addr_i,
    .data_i,
    .data_o,
    .ctl_regs(coefs),
    /* verilator lint_off PINCONNECTEMPTY */
    .sts_regs()
    /* verilator lint_on PINCONNECTEMPTY */
);

localparam ACCUMW = DW + COEFW + $clog2(N);
logic signed [ACCUMW-1:0] accum_tdata;
logic accum_tvalid, accum_tready;

logic signed [DW-1:0] macc_atdata;
logic signed [COEFW-1:0] macc_btdata;
logic macc_tvalid = 0, macc_tready, macc_tlast = 0;

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
    .m_axis_tvalid(accum_tvalid),
    .m_axis_tready(accum_tready)
);

logic signed [DW-1:0] input_regs [N];
logic [N-1:0] input_mask = 0;

logic [MIDW-1:0] output_ctr = 0;
logic [NIDW-1:0] input_ctr = 0;
logic [IDW-1:0] coef_ctr = 0;

// logic [NIDW-1:0] input_tid = 0;

logic signed [DW-1:0] skid_tdata;
logic [NIDW-1:0] skid_tid;
logic skid_tvalid, skid_tready = 1, skid_tlast;
logic idle = 1;

Skid #(
    .DW(DW+NIDW+1)
) skid_i (
    .clk,
    .rst,
    .s_axis_tdata({s_axis_tdata, s_axis_tid[NIDW-1:0], s_axis_tlast}),
    .s_axis_tvalid,
    .s_axis_tready,
    .m_axis_tdata({skid_tdata, skid_tid, skid_tlast}),
    .m_axis_tvalid(skid_tvalid),
    .m_axis_tready(skid_tready)
);

// Receive input data
always @(posedge clk) begin

    // 
    if (macc_tvalid && macc_tready) begin
        macc_tvalid <= 0;
        macc_tlast <= 0;
    end

    // Process the data.
    if (!idle) begin
        // Macc is not busy.
        if (!macc_tvalid || macc_tready) begin
            // Input is valid.
            if (input_mask[input_ctr]) begin
                // Load data into the macc.
                macc_atdata <= input_regs[input_ctr];
                // Load gain coefficient.
                macc_btdata <= coefs[coef_ctr];
                macc_tvalid <= 1;
                
                // Increment counters.
                input_ctr <= input_ctr + 1;
                coef_ctr <= coef_ctr + 1;

                // Last input in the computation.
                if (input_ctr == NLAST) begin
                    macc_tlast <= 1;

                    input_ctr <= 0;
                    output_ctr <= output_ctr + 1;

                    // Finished the last output
                    if (output_ctr == MLAST) begin
                        output_ctr <= 0;

                        idle <= 1;
                        skid_tready <= 1;
                    end
                end
            end
        end
    end

    // Receiving input data stream from serializer.
    if (skid_tvalid && skid_tready) begin
        // Load data into input regs.
        input_regs[skid_tid] <= skid_tdata;

        // Expect stream to be serialized, so there will always be N samples per frame with tid=0...N-1 for each input.

        // If this is the very first sample, reset counters and start computation.
        if (idle) begin
            idle <= 0;
            input_ctr <= 0;
            output_ctr <= 0;
            coef_ctr <= 0;

            input_mask <= 0; // Clear the mask.
        end

        // Set the bit in the mask to show that this register has been filled.
        input_mask[skid_tid] <= 1;
        

        // Last sample, stall the skid until computation is done.
        if (skid_tlast) begin
            skid_tready <= 0;
        end
    end

    //
    if (rst) begin
        macc_tvalid <= 0;
        macc_tlast <= 0;

        skid_tready <= 1;
        idle <= 1;
        input_mask <= 0;
        input_ctr <= 0;
        output_ctr <= 0;
        coef_ctr <= 0;
    end
end

assign accum_tready = !m_axis_tvalid || m_axis_tready;

logic [MIDW-1:0] m_id_ctr = 0;

// Output sequencing
always @(posedge clk) begin
    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
        m_axis_tlast <= 0;
    end

    if (accum_tvalid && accum_tready) begin
        /* verilator lint_off WIDTHTRUNC */
        m_axis_tdata <= $signed(accum_tdata >>> COEFQ);
        /* verilator lint_on WIDTHTRUNC */
        m_axis_tvalid <= 1;
        m_axis_tid <= TIDW'(m_id_ctr);
        m_id_ctr <= m_id_ctr + 1;

        if (m_id_ctr == MLAST) begin
            m_axis_tlast <= 1;
            m_id_ctr <= 0;
        end
    end

    if (rst) begin
        m_id_ctr <= 0;
        m_axis_tvalid <= 0;
        m_axis_tlast <= 0;
        // m_axis_tid <= 0;
    end
end

endmodule
