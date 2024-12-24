/*
    Simple Dual-Port, Single Clock RAM
*/
`include "bram_pkg.svh"
import bram_pkg::LOW_LATENCY;
import bram_pkg::HIGH_PERFORMANCE;

module BramSdp #(
    parameter DW = 32,                  // BRAM data width
    parameter DEPTH = 1024,             // BRAM depth
    parameter OREG = HIGH_PERFORMANCE,                 // Optional outut register for read data.
    parameter string INIT_FILE = "",    // Initialize bram from memory file.
    localparam AW = $clog2(DEPTH)
) (
    input  logic clk,
    input  logic rst,

    /* verilator lint_off UNUSED */
    input  logic ena,               // Enable command. Not used, but compatible interface?
    /* verilator lint_on UNUSED */
    input  logic wea,               // Write enable
    input  logic [AW-1:0] addra,    // Write address bus, width determined from RAM_DEPTH
    input  logic [DW-1:0] dina,     // Write data
    output logic [DW-1:0] douta,    // Unused read data, but compatible interface.

    /* verilator lint_off UNUSED */
    input  logic regcea,
    /* verilator lint_on UNUSED */

    input  logic enb,               // Read enable
    /* verilator lint_off UNUSED */
    input  logic web,               // Not used, but compatible interface?
    /* verilator lint_on UNUSED */

    input  logic [AW-1:0] addrb,    // Read address bus, width determined from RAM_DEPTH

    /* verilator lint_off UNUSED */
    input  logic [DW-1:0] dinb,     // Unused but compatible interface.
    /* verilator lint_on UNUSED */

    output logic [DW-1:0] doutb,    // Read data
    input  logic regceb             // Clock-enable for optional output register. Set this to contant 1 to always enable.
);

assign douta = 0;

logic [DW-1:0] BRAM [DEPTH];
logic [DW-1:0] ram_data = 0;

// The following code either initializes the memory values to a specified file or to all zeros to match hardware
generate
    if (INIT_FILE != "") begin: use_init_file
        initial $readmemh(INIT_FILE, BRAM, 0, DEPTH-1);
    end else begin: init_bram_to_zero
        initial
        for (int ram_index = 0; ram_index < DEPTH; ram_index = ram_index + 1) begin
            BRAM[ram_index] = 0;
        end
    end
endgenerate

always @(posedge clk) begin
    if (wea) begin
        BRAM[addra] <= dina;
    end
    if (enb) begin
        ram_data <= BRAM[addrb];
    end
end

//  The following code generates HIGH_PERFORMANCE (use output register) or LOW_LATENCY (no output register)
generate
    if (OREG == LOW_LATENCY) begin: no_output_register
        // The following is a 1 clock cycle read latency at the cost of a longer clock-to-out timing
        assign doutb = ram_data;

    end else begin: output_register
        // The following is a 2 clock cycle read latency with improve clock-to-out timing
        logic [DW-1:0] doutb_reg = 0;

        always @(posedge clk) begin
            if (regceb) begin
                doutb_reg <= ram_data;
            end
            if (rst) begin
                doutb_reg <= 0;
            end
        end

        assign doutb = doutb_reg;
    end
endgenerate

endmodule
						
/*
bram_sdp #(
    .DW(DW),
    .BRAM_DEPTH(BRAM_DEPTH),
    .OREG(OREG),
    .INIT_FILE("")
) bram_sdp_i (
    .clk(clk),
    .rst(rst),
    .addra(addra),
    .wea(wea),
    .dina(dina),
    .addrb(addrb),
    .enb(enb),
    .doutb(doutb),
    .regceb(regceb)
);
*/
