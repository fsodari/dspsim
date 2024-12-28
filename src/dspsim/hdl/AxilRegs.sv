module AxilRegs #(
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter REGW = 32,
    parameter N_CTL = 32,
    parameter N_STS = 32
    // parameter SIGN_EXTEND = 1,
    // parameter string INIT_FILE = "",
    // localparam STBW = CFGAW >> 2
) (
    input  logic clk,
    input  logic rst,

    // Write address
    input  logic [CFGAW-1:0] s_axil_awaddr,
    input  logic s_axil_awvalid,
    output logic s_axil_awready,

    // Write data
    input  logic signed [CFGDW-1:0] s_axil_wdata,
    // /* verilator lint_off UNUSED */
    // input  logic [STBW-1:0] s_axil_wstb,
    // /* verilator lint_on UNUSED */
    input  logic s_axil_wvalid,
    output logic s_axil_wready,

    // Write response
    output logic [1:0] s_axil_bresp,
    output logic s_axil_bvalid,
    input  logic s_axil_bready,

    // Read address
    input  logic [CFGAW-1:0] s_axil_araddr,
    input  logic s_axil_arvalid,
    output logic s_axil_arready,

    // Read response
    output logic signed [CFGDW-1:0] s_axil_rdata,
    output logic [1:0] s_axil_rresp,
    output logic s_axil_rvalid,
    input  logic s_axil_rready,

    output logic signed [REGW-1:0] ctl_regs[N_CTL],
    input  logic signed [REGW-1:0] sts_regs[N_STS]
);

// Combine/sync write address and write data streams.
logic [CFGAW-1:0] awaddr;
logic [CFGDW-1:0] wdata;
logic awvalid, wvalid, wready;

// combine2 #(.ADW(CFGAW), .BDW(CFGDW)) combine_write_i (
//     .clk,
//     .rst,

//     .s_axis_atdata(s_axil_awaddr),
//     .s_axis_atvalid(s_axil_awvalid),
//     .s_axis_atready(s_axil_awready),

//     .s_axis_btdata(s_axil_wdata),
//     .s_axis_btvalid(s_axil_wvalid),
//     .s_axis_btready(s_axil_wready),

//     .m_axis_atdata(waddr),
//     .m_axis_btdata(wdata),
//     .m_axis_tvalid(wvalid),
//     .m_axis_tready(wready)
// );
Skid #(.DW(CFGAW)) write_addr_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axil_awaddr),
    .s_axis_tvalid(s_axil_awvalid),
    .s_axis_tready(s_axil_awready),
    .m_axis_tdata(awaddr),
    .m_axis_tvalid(awvalid),
    .m_axis_tready(wready)
);
Skid #(.DW(CFGDW)) write_data_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axil_wdata),
    .s_axis_tvalid(s_axil_wvalid),
    .s_axis_tready(s_axil_wready),
    .m_axis_tdata(wdata),
    .m_axis_tvalid(wvalid),
    .m_axis_tready(wready)
);
logic write_all_valid;
assign write_all_valid = awvalid && wvalid;
assign wready = write_all_valid && (!s_axil_bvalid || s_axil_bready);

always @(posedge clk) begin
    if (s_axil_bvalid && s_axil_bready) begin
        s_axil_bvalid <= 0;
    end

    if (wvalid && wready) begin
        if (awaddr < N_CTL) begin
            ctl_regs[awaddr] <= wdata[REGW-1:0];
        end

        s_axil_bresp <= 0;
        s_axil_bvalid <= 1;
    end

    if (rst) begin
        s_axil_bvalid <= 0;
    end
end

// Read channel
logic [CFGAW-1:0] araddr;
logic arvalid, arready;

Skid #(.DW(CFGAW)) read_i (
    .clk,
    .rst,
    .s_axis_tdata(s_axil_araddr),
    .s_axis_tvalid(s_axil_arvalid),
    .s_axis_tready(s_axil_arready),
    .m_axis_tdata(araddr),
    .m_axis_tvalid(arvalid),
    .m_axis_tready(arready)
);

assign arready = !s_axil_rvalid || s_axil_rready;

logic signed [REGW-1:0] cdata_r, sdata_r;
/* verilator lint_off WIDTHEXPAND */
assign cdata_r = ctl_regs[araddr];
assign sdata_r = sts_regs[araddr-N_CTL];
// assign cdata_r = SIGN_EXTEND != 0 ? $signed(ctl_regs[araddr]) : {{(CFGDW-REGW){1'b0}}, ctl_regs[araddr]};
// assign sdata_r = SIGN_EXTEND != 0 ? $signed(sts_regs[araddr - N_CTL]) : {{(CFGDW-REGW){1'b0}}, sts_regs[araddr - N_CTL]};
/* verilator lint_on WIDTHEXPAND */

always @(posedge clk) begin

    if (s_axil_rvalid && s_axil_rready) begin
        s_axil_rvalid <= 0;
    end

    if (arvalid && arready) begin
        if (araddr < N_CTL) begin
            s_axil_rdata <= cdata_r;
        end else begin
            s_axil_rdata <= sdata_r;
        end
        s_axil_rresp <= 0;
        s_axil_rvalid <= 1;
    end

    if (rst) begin
        s_axil_rvalid <= 0;
    end
end
endmodule
