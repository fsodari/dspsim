//  Xilinx True Dual Port RAM, No Change, Single Clock
//  This code implements a parameterizable true dual port memory (both ports can read and write).
//  This is a no change RAM which retains the last read value on the output during writes
//  which is the most power efficient mode.
//  If a reset or enable is not necessary, it may be tied off or removed from the code.
`include "bram_pkg.svh"
import bram_pkg::LOW_LATENCY;
import bram_pkg::HIGH_PERFORMANCE;
import bram_pkg::NC;
import bram_pkg::RF;
import bram_pkg::WF;

module BramTdp #(
    parameter DW = 32,                              // Specify RAM data width
    parameter DEPTH = 1024,                             // Specify RAM depth (number of entries)
    parameter OREG = HIGH_PERFORMANCE, // Select "HIGH_PERFORMANCE" or "LOW_LATENCY" 
    parameter string INIT_FILE = "",                       // Specify name/location of RAM initialization file if using one (leave blank if not)
    parameter MODE = RF,                          // NC: No Change, RF: Read First, WF: Write First
    localparam AW = $clog2(DEPTH)
) (
    input  logic clka,
    input  logic rsta,              // Port A output reset (does not affect memory contents)
    input  logic ena,               // Port A RAM Enable, for additional power savings, disable port when not in use
    input  logic wea,               // Port A write enable
    input  logic [AW-1:0] addra,    // Port A address bus, width determined from DEPTH
    input  logic [DW-1:0] dina,     // Port A RAM input data
    output logic [DW-1:0] douta,    // Port A RAM output data
    input  logic regcea,         // Port A output register enable. Set to 1 to always enable.

    input  logic clkb,
    input  logic rstb,              // Port B output reset (does not affect memory contents)
    input  logic enb,               // Port B RAM Enable, for additional power savings, disable port when not in use
    input  logic web,               // Port B write enable
    input  logic [AW-1:0] addrb,    // Port B address bus, width determined from DEPTH
    input  logic [DW-1:0] dinb,     // Port B RAM input data
    output logic [DW-1:0] doutb,    // Port B RAM output data
    input  logic regceb          // Port B output register enable. Set to 1 to always enable.

    /*
    Alternative interface with port arrays.

    input  logic en[2],
    input  logic we[2],
    input  logic [AW-1:0] addr[2],
    input  logic [DW-1:0] din[2],
    output logic [DW-1:0] dout[2]
    */
);

// TDP can support multiple clocks. Useful with an asynchronous fifo.
/* verilator lint_off MULTIDRIVEN */
logic [DW-1:0] BRAM [DEPTH];
/* verilator lint_on MULTIDRIVEN */

logic [DW-1:0] ram_data_a = 0;
logic [DW-1:0] ram_data_b = 0;

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

generate
    case (MODE)
        // Read First
        RF: begin : mode_read_first
            always @(posedge clka) begin
                if (ena) begin
                    if (wea) begin
                        BRAM[addra] <= dina;
                    end
                    ram_data_a <= BRAM[addra];
                end
            end
            always @(posedge clkb) begin
                if (enb) begin
                    if (web) begin
                        BRAM[addrb] <= dinb;
                    end
                    ram_data_b <= BRAM[addrb];
                end
            end
        end
        // Write First
        WF: begin : mode_write_first
            always @(posedge clka) begin
                if (ena) begin
                    if (wea) begin
                        BRAM[addra] <= dina;
                        ram_data_a <= dina;
                    end else begin
                        ram_data_a <= BRAM[addra];
                    end
                end
            end
            always @(posedge clkb) begin
                if (enb) begin
                    if (web) begin
                        BRAM[addrb] <= dinb;
                        ram_data_b <= dinb;
                    end else begin
                        ram_data_b <= BRAM[addrb];
                    end
                end
            end
        end
        
        // No Change Mode
        default: begin : mode_no_change
            always @(posedge clka) begin
                if (ena) begin
                    if (wea) begin
                        BRAM[addra] <= dina;
                    end else begin
                        ram_data_a <= BRAM[addra];
                    end
                end
            end
            always @(posedge clkb) begin
                if (enb) begin
                    if (web) begin
                        BRAM[addrb] <= dinb;
                    end else begin
                        ram_data_b <= BRAM[addrb];
                    end
                end
            end
        end
    endcase
    
endgenerate

//  The following code generates HIGH_PERFORMANCE (use output register) or LOW_LATENCY (no output register)
generate
    if (OREG == LOW_LATENCY) begin: no_output_register
        // The following is a 1 clock cycle read latency at the cost of a longer clock-to-out timing
        assign douta = ram_data_a;
        assign doutb = ram_data_b;

    end else begin: output_register
        // The following is a 2 clock cycle read latency with improve clock-to-out timing
        logic [DW-1:0] douta_reg = 0;
        logic [DW-1:0] doutb_reg = 0;

        always @(posedge clka) begin
            if (regcea) begin
                douta_reg <= ram_data_a;
            end
            if (rsta) begin
                douta_reg <= 0;
            end
        end

        always @(posedge clkb) begin
            if (regceb) begin
                doutb_reg <= ram_data_b;
            end
            if (rstb) begin
                doutb_reg <= 0;
            end
        end

        assign douta = douta_reg;
        assign doutb = doutb_reg;
    end
endgenerate

endmodule
