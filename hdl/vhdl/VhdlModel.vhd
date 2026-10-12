-- Ports and generics of every kind that dspsim supports for a VHDL entity. Every output is its input changed
-- in a way of its own, element by element, so that an element that dspsim connects to the wrong bits shows.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.vhdl_model_pkg.all;

entity VhdlModel is
  generic (
    DW     : positive := 12;
    N      : positive := 3;
    OFFSET : integer  := -2;
    MODE   : string   := "add";
    INVERT : boolean  := false;
    SCALE  : real     := 0.5
  );
  port (
    clk       : in  std_logic;
    rst       : in  std_logic;
    -- Vectors, whose width is a generic.
    data_in   : in  signed(DW-1 downto 0);
    data_out  : out signed(DW-1 downto 0);
    acc_out   : out signed(DW+3 downto 0);
    wide_in   : in  std_logic_vector(99 downto 0);
    wide_out  : out std_logic_vector(99 downto 0);
    -- Integers, an enumeration and a boolean.
    count_in  : in  natural range 0 to 100;
    count_out : out integer range -100 to 100;
    state_in  : in  state_t;
    state_out : out state_t;
    flag_in   : in  boolean;
    flag_out  : out boolean;
    -- Arrays: with ascending and descending ranges, a range that does not start at 0, two dimensions.
    vec_in    : in  byte_array_t(1 to N);
    vec_out   : out byte_array_t(N-1 downto 0);
    mat_in    : in  matrix_t(0 to 1, 0 to 2);
    mat_out   : out signed(11 downto 0);
    -- A record, an array of records and a record of arrays of records.
    bus_in    : in  bus_t;
    bus_out   : out bus_t;
    buses_in  : in  bus_array_t(0 to 1);
    buses_out : out bus_array_t(0 to 1);
    frame_in  : in  frame_t;
    frame_out : out frame_t
  );
end entity;

architecture rtl of VhdlModel is

  -- The same change for a bus wherever it is, with a number that tells the buses apart.
  function changed(b : bus_t; n : natural) return bus_t is
    variable r : bus_t;
  begin
    r.data  := b.data + to_signed(n, 16);
    r.valid := b.valid;
    r.id    := b.id + 1;
    r.level := -b.level;
    if b.state = FAIL then
      r.state := IDLE;
    else
      r.state := state_t'succ(b.state);
    end if;
    r.last  := not b.last;
    r.iq.re := b.iq.im;
    r.iq.im := b.iq.re + 1;
    for i in 0 to 3 loop
      r.taps(i) := b.taps(3 - i) + to_unsigned(i, 6);
    end loop;
    return r;
  end function;

begin

  assert SCALE > 0.0 report "SCALE must be positive" severity failure;

  u_acc : entity work.Accumulator
    generic map (W => DW)
    port map (clk => clk, rst => rst, d => data_in, q => acc_out);

  process (clk)
    variable sum : signed(11 downto 0);
  begin
    if rising_edge(clk) then
      if MODE = "add" then
        data_out <= data_in + to_signed(OFFSET, DW);
      else
        data_out <= data_in - to_signed(OFFSET, DW);
      end if;
      wide_out  <= not wide_in;
      count_out <= -count_in;
      if state_in = FAIL then
        state_out <= IDLE;
      else
        state_out <= state_t'succ(state_in);
      end if;
      flag_out <= flag_in xor INVERT;

      -- vec_out(i) is vec_in(i + 1) plus i.
      for i in 0 to N-1 loop
        vec_out(i) <= vec_in(i + 1) + to_unsigned(i, 8);
      end loop;
      -- Every element of the matrix has a weight of its own: 1 to 6, row by row.
      sum := (others => '0');
      for i in 0 to 1 loop
        for j in 0 to 2 loop
          sum := sum + resize(mat_in(i, j) * to_signed(3 * i + j + 1, 4), 12);
        end loop;
      end loop;
      mat_out <= sum;

      bus_out <= changed(bus_in, 0);
      for i in 0 to 1 loop
        buses_out(i) <= changed(buses_in(i), 10 * (i + 1));
        frame_out.samples(i) <= changed(frame_in.samples(i), 100 * (i + 1));
      end loop;
      frame_out.count <= 100 - frame_in.count;
    end if;
  end process;

end architecture;
