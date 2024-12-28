`include "bram_pkg.svh"
import bram_pkg::LOW_LATENCY;
import bram_pkg::HIGH_PERFORMANCE;
import bram_pkg::NC;
import bram_pkg::RF;
import bram_pkg::WF;

module AxilBram #(
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter REGW = 32,
    parameter DEPTH = 32,
    localparam BRAMAW = $clog2(DEPTH),
    parameter OREG = HIGH_PERFORMANCE
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

    // Bram interface
    output logic bram_en,
    output logic bram_we,
    output logic [BRAMAW-1:0] bram_addr,
    output logic [REGW-1:0] bram_dout,
    input  logic [REGW-1:0] bran_din,
    output logic bram_regce
);

endmodule
