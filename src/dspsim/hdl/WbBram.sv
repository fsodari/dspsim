`include "bram_pkg.svh"
import bram_pkg::LOW_LATENCY;
import bram_pkg::HIGH_PERFORMANCE;
import bram_pkg::NC;
import bram_pkg::RF;
import bram_pkg::WF;

// Wishbone Bram Controller. Does not instantiate memory.
module WbBram #(
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter DW = 18,
    parameter DEPTH = 1024,
    parameter SIGN_EXTEND = 1,
    parameter OREG = HIGH_PERFORMANCE,

    localparam BRAMAW = $clog2(DEPTH)
) (
    input  logic clk,
    input  logic rst,

    // Wishbone interface
    input  logic cyc_i,
    input  logic stb_i,
    input  logic we_i,
    output logic ack_o,
    output logic stall_o,
    input  logic [CFGAW-1:0] addr_i,
    input  logic signed [CFGDW-1:0] data_i,
    output logic signed [CFGDW-1:0] data_o,


    // BRAM Interface
    output logic bram_en,
    output logic bram_we,
    output logic [BRAMAW-1:0] bram_addr,
    output logic [DW-1:0] bram_dout,
    input  logic [DW-1:0] bram_din,
    output logic bram_regce
);

// BRAM can accept requests every clock cycle, so we don't need to stall.
assign stall_o = 0;

assign bram_regce = 1;

// Translate wishbone signals to BRAM interface.
assign bram_addr = addr_i[BRAMAW-1:0];
assign bram_en = cyc_i & stb_i;
assign bram_we = cyc_i & stb_i & we_i;
assign bram_dout = data_i[DW-1:0];
/* verilator lint_off WIDTHEXPAND */
assign data_o = SIGN_EXTEND != 0 ? $signed(bram_din) : {{(CFGDW - DW){1'b0}}, bram_din};
/* verilator lint_on WIDTHEXPAND */

// Handle ack timing. High performance mode will have an extra clock cycle of latency on the ack signal.
logic [1:0] ack_pipe;
assign ack_o = OREG == LOW_LATENCY ? ack_pipe[0] : ack_pipe[1];

always @(posedge clk) begin
    ack_pipe <= {ack_pipe[0], cyc_i & stb_i};
end

endmodule
