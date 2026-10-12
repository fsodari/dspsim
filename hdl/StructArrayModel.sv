// Arrays of structs, packed unions and packed arrays of structs. dspsim presents an array of structs as an
// array whose elements are structs, and a packed union or packed array as the vector of its bits.
package struct_array_pkg;

  typedef struct packed {
    logic [2:0] id;
    logic       last;
  } flags_t;

  typedef struct {
    logic signed [7:0] re;
    logic signed [7:0] im;
  } iq_t;

  // An element of the unpacked arrays.
  typedef struct {
    logic signed [11:0] data;
    logic [5:0]         taps [2];
    iq_t                iq;
    logic [99:0]        wide;
    flags_t             flags;
  } elem_t;

  // A packed element of more than 64 bits: 86 bits in three 32-bit words.
  typedef struct packed {
    logic [69:0]        wide;
    logic signed [11:0] data;
    flags_t             flags;
  } packed_elem_t;

  // Arrays of structs as fields of a struct.
  typedef struct {
    iq_t    iqs [3];
    flags_t flags [2];
    logic   valid;
  } outer_t;

  // The members of a packed union are views of the same 16 bits.
  typedef union packed {
    logic [15:0]     raw;
    logic [1:0][7:0] bytes;
    struct packed {
      logic [3:0]         op;
      logic signed [11:0] imm;
    } fields;
  } word_t;

endpackage

module StructArrayModel
  import struct_array_pkg::*;
(
    input  logic         clk,
    input  elem_t        a_in  [2][3],
    output elem_t        a_out [2][3],
    input  flags_t       f_in  [4],
    output flags_t       f_out [4],
    input  packed_elem_t w_in  [2],
    output packed_elem_t w_out [2],
    input  outer_t       o_in,
    output outer_t       o_out,
    input  word_t        u_in,
    output word_t        u_out,
    input  flags_t [2:0] pk_in,
    output flags_t [2:0] pk_out
);

  always_ff @(posedge clk) begin
    // Every element changes in a way of its own, so that an element that is mapped to the wrong place shows.
    for (int i = 0; i < 2; i++) begin
      for (int j = 0; j < 3; j++) begin
        a_out[i][j].data       <= a_in[i][j].data + 12'(3 * i + j);
        a_out[i][j].taps[0]    <= a_in[i][j].taps[1];
        a_out[i][j].taps[1]    <= a_in[i][j].taps[0];
        a_out[i][j].iq.re      <= -a_in[i][j].iq.re;
        a_out[i][j].iq.im      <= a_in[i][j].iq.im + 8'(i);
        a_out[i][j].wide       <= ~a_in[i][j].wide;
        a_out[i][j].flags.id   <= a_in[i][j].flags.id + 3'(j);
        a_out[i][j].flags.last <= ~a_in[i][j].flags.last;
      end
    end
    for (int i = 0; i < 4; i++) begin
      f_out[i].id   <= f_in[i].id + 3'(i);
      f_out[i].last <= f_in[i].last;
    end
    for (int i = 0; i < 2; i++) begin
      w_out[i].wide     <= ~w_in[i].wide;
      w_out[i].data     <= w_in[i].data - 12'(i);
      w_out[i].flags.id <= w_in[i].flags.id + 3'(i + 1);
      w_out[i].flags.last <= w_in[i].flags.last;
    end
    for (int i = 0; i < 3; i++) begin
      o_out.iqs[i].re <= o_in.iqs[i].im;
      o_out.iqs[i].im <= o_in.iqs[i].re + 8'(i);
    end
    for (int i = 0; i < 2; i++) begin
      o_out.flags[i].id   <= o_in.flags[1-i].id;
      o_out.flags[i].last <= o_in.flags[1-i].last;
    end
    o_out.valid <= o_in.valid;

    // Written through one view of the union, read through another.
    u_out.fields.op  <= u_in.bytes[0][3:0];
    u_out.fields.imm <= -u_in.fields.imm;
    for (int i = 0; i < 3; i++) begin
      pk_out[i] <= pk_in[2-i];
    end
  end

endmodule
