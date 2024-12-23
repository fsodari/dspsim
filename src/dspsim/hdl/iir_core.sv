module iir_core #(
    parameter DW = 24,
    parameter COEFW = 18,
    parameter COEFQ = 16,
    parameter ORDER = 2,
    localparam N = (ORDER+1)*2
    // parameter signed [COEFW-1:0] COEF_INIT [N] = '{0,0,0,0,0,0}
) (
    input  logic clk,
    input  logic rst,

    // Audio Bus
    input  logic signed [DW-1:0] s_axis_tdata,
    input  logic s_axis_tvalid,
    output logic s_axis_tready,

    output logic signed [DW-1:0] m_axis_tdata,
    output logic m_axis_tvalid,
    input  logic m_axis_tready,

    // Coefficients
    input  logic signed [COEFW-1:0] coefs [N]
);
localparam IDW = $clog2(N);
localparam MW = DW + COEFW;
localparam ACCUMW = MW + $clog2(N);

logic signed [ACCUMW-1:0] accum;

logic signed [DW-1:0] state_vars [N];
logic signed [DW-1:0] accum_o;
logic [COEFW-1:0] frac_mask;
// assign frac_mask = COEFW'(COEFQ - 1);
assign frac_mask = 0;

assign accum_o = DW'(accum >>> COEFQ);

logic [$clog2(N)-1:0] coef_index;

// Multiply accumulate
logic [N-1:0] mac_s_axis_tvalid = 0;
logic mac_ce;
assign mac_ce = |mac_s_axis_tvalid;
logic [8:0] x_valid;
logic mac_rst = 0;
macc_core #(
    .ADW(DW),
    .BDW(COEFW),
    .ODW(ACCUMW)
) macc_inst (
    .clk(clk),
    // .rst(mac_rst | rst), // Reset on first sample
    .ce(mac_ce),
    .sload(1),
    .a(state_vars[coef_index]),
    .b(coefs[coef_index]),
    .accum_o(accum)
);

logic [DW-1:0] skid_tdata;
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

// logic done;
// assign done = coef_index == IDW'(N-1);
logic busy = 0;
assign skid_tready = !busy && (!m_axis_tvalid || m_axis_tready);

always @(posedge clk) begin
    mac_s_axis_tvalid <= {mac_s_axis_tvalid[N-2:0], skid_tvalid && skid_tready};
    mac_rst <= 0;
    x_valid <= {x_valid[7:0], skid_tvalid && skid_tready};

    if (m_axis_tvalid && m_axis_tready) begin
        m_axis_tvalid <= 0;
    end

    if (x_valid[8]) begin
        m_axis_tvalid <= 1;
        m_axis_tdata <= accum_o;
        busy <= 0;
        /* verilator lint_off WIDTH */
        state_vars[ORDER+1] <= accum[COEFW-1:0] & frac_mask;
        /* verilator lint_on WIDTH */
        // frac_save <= accum[COEFW-1:0] & frac_mask;
        state_vars[ORDER+2] <= accum_o;
        for (int yz = 2; yz <= ORDER; yz++) begin
            state_vars[ORDER+1+yz] <= state_vars[ORDER+yz]; // y
        end
    end

    

    // Busy. Increment the coef index.
    if (coef_index != IDW'(N)) begin
        coef_index <= coef_index + 1;
        if(coef_index == IDW'(N-1)) begin
            // busy <= 0;
        end
    end

    // if (!)

    // New input sample.
    if (skid_tvalid && skid_tready) begin
        mac_rst <= 1;
        coef_index <= 0;
        busy <= 1;
        state_vars[0] <= skid_tdata;

        // Shift data in buffer. More efficient implementation?
        for (int z = 1; z <= ORDER; z++) begin
            state_vars[z] <= state_vars[z-1]; // X
        end
    end

    if (rst) begin
        busy <= 0;
    end
end

endmodule
