// Struct ports. dspsim presents each field of a struct port as a port of its own.
package struct_model_pkg;

  typedef struct {
    logic signed [7:0] re;
    logic signed [7:0] im;
  } iq_t;

  typedef struct packed {
    logic [2:0] id;
    logic       last;
  } flags_t;

  // Unpacked: Verilator keeps one variable per field.
  typedef struct {
    logic signed [11:0] data;
    logic               valid;
    logic [5:0]         taps [4];
    iq_t                iq;
    logic [99:0]        wide;
    flags_t             flags;
  } bus_t;

  // Packed: one vector of 17 bits, with the first field in the most significant bits.
  typedef struct packed {
    logic signed [11:0] data;
    logic               valid;
    flags_t             flags;
  } packed_bus_t;

  // Packed and more than 64 bits: 147 bits in 32-bit words. `mid` is in bits [46:7], across two words, and
  // `wide` in bits [146:47].
  typedef struct packed {
    logic [99:0]        wide;
    logic signed [39:0] mid;
    flags_t             flags;
    logic [2:0]         lo;
  } packed_wide_t;

endpackage

module StructModel
  import struct_model_pkg::*;
(
    input  logic         clk,
    input  bus_t         u_in,
    output bus_t         u_out,
    input  packed_bus_t  p_in,
    output packed_bus_t  p_out,
    input  packed_wide_t w_in,
    output packed_wide_t w_out
);

  always_ff @(posedge clk) begin
    // Every field changes in a way of its own, so that a field that is mapped to the wrong bits shows.
    u_out.data       <= -u_in.data;
    u_out.valid      <= u_in.valid;
    for (int i = 0; i < 4; i++) begin
      u_out.taps[i] <= u_in.taps[3-i];
    end
    u_out.iq.re      <= u_in.iq.im;
    u_out.iq.im      <= u_in.iq.re;
    u_out.wide       <= ~u_in.wide;
    u_out.flags.id   <= u_in.flags.id + 3'd1;
    u_out.flags.last <= ~u_in.flags.last;

    p_out.data       <= -p_in.data;
    p_out.valid      <= p_in.valid;
    p_out.flags.id   <= p_in.flags.id + 3'd2;
    p_out.flags.last <= ~p_in.flags.last;

    w_out.wide       <= ~w_in.wide;
    w_out.mid        <= -w_in.mid;
    w_out.flags.id   <= w_in.flags.id + 3'd3;
    w_out.flags.last <= w_in.flags.last;
    w_out.lo         <= w_in.lo + 3'd1;
  end

endmodule
