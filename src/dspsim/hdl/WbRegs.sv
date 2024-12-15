/*
    Wishbone interface to an array of registers
*/

module WbRegs #(
    parameter CFGAW = 32,
    parameter CFGDW = 32,
    parameter N_CTL = 32,
    // status regs are after ctl regs
    parameter N_STS = 32
) (
    input  logic clk,
    /* verilator lint_off UNUSED */
    input  logic rst,
    /* verilator lint_on UNUSED */

    input  logic cyc_i,
    input  logic stb_i,
    input  logic we_i,
    output logic ack_o,
    output logic stall_o,

    /* verilator lint_off UNUSED */
    input  logic [CFGAW-1:0] addr_i,
    /* verilator lint_on UNUSED */
    input  logic [CFGDW-1:0] data_i,
    output logic [CFGDW-1:0] data_o,

    // Registers
    output logic [CFGDW-1:0] ctl_regs [N_CTL],
    input  logic [CFGDW-1:0] sts_regs [N_STS]
);

// This interface never stalls.
assign stall_o = 0;

always @(posedge clk) begin
    ack_o <= cyc_i & stb_i;

    if (cyc_i && stb_i) begin
        if (we_i && addr_i < N_CTL) begin
            ctl_regs[addr_i] <= data_i;
        end else begin
            data_o <= addr_i < N_CTL ? ctl_regs[addr_i] : sts_regs[addr_i - N_CTL];
        end
    end
end

endmodule
