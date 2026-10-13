// The stage of the bus design of the verilated benchmarks, with its ports declared in three ways. The three
// modules register the same function of the bus: every field changes, and depends on other fields.
package bus_pkg;

  typedef struct packed {
    logic signed [11:0] re;
    logic signed [11:0] im;
  } iq_t;

  // Packed: one vector of 64 bits, with the first field in the most significant bits.
  typedef struct packed {
    logic [31:0] data;
    logic [7:0]  tag;
    iq_t         iq;
  } bus_t;

  typedef struct {
    logic signed [11:0] re;
    logic signed [11:0] im;
  } unpacked_iq_t;

  // Unpacked: Verilator keeps one variable per field.
  typedef struct {
    logic [31:0]  data;
    logic [7:0]   tag;
    unpacked_iq_t iq;
  } unpacked_bus_t;

endpackage

// A port per field.
module BusPorts (
    input  logic               clk,
    input  logic        [31:0] s_data,
    input  logic        [7:0]  s_tag,
    input  logic signed [11:0] s_re,
    input  logic signed [11:0] s_im,
    output logic        [31:0] m_data,
    output logic        [7:0]  m_tag,
    output logic signed [11:0] m_re,
    output logic signed [11:0] m_im
);

  always_ff @(posedge clk) begin
    m_data <= s_data + 32'(s_tag);
    m_tag  <= s_tag + 8'd1;
    m_re   <= s_im;
    m_im   <= s_re + 12'(s_tag);
  end

endmodule

// A packed struct.
module BusPacked
  import bus_pkg::*;
(
    input  logic clk,
    input  bus_t s,
    output bus_t m
);

  always_ff @(posedge clk) begin
    m.data  <= s.data + 32'(s.tag);
    m.tag   <= s.tag + 8'd1;
    m.iq.re <= s.iq.im;
    m.iq.im <= s.iq.re + 12'(s.tag);
  end

endmodule

// An unpacked struct.
module BusUnpacked
  import bus_pkg::*;
(
    input  logic          clk,
    input  unpacked_bus_t s,
    output unpacked_bus_t m
);

  always_ff @(posedge clk) begin
    m.data  <= s.data + 32'(s.tag);
    m.tag   <= s.tag + 8'd1;
    m.iq.re <= s.iq.im;
    m.iq.im <= s.iq.re + 12'(s.tag);
  end

endmodule
