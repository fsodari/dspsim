-- The types of the ports of VhdlModel.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package vhdl_model_pkg is

  type state_t is (IDLE, LOAD, RUN, DONE, FAIL);

  type byte_array_t is array (natural range <>) of unsigned(7 downto 0);
  type matrix_t is array (natural range <>, natural range <>) of signed(7 downto 0);
  type tap_array_t is array (0 to 3) of unsigned(5 downto 0);

  type iq_t is record
    re : signed(7 downto 0);
    im : signed(7 downto 0);
  end record;

  -- A record with every kind of element: vectors, a logic value, an integer, an enumeration, a boolean,
  -- another record and an array.
  type bus_t is record
    data  : signed(15 downto 0);
    valid : std_logic;
    id    : unsigned(2 downto 0);
    level : integer range -5 to 5;
    state : state_t;
    last  : boolean;
    iq    : iq_t;
    taps  : tap_array_t;
  end record;

  type bus_array_t is array (natural range <>) of bus_t;

  -- A record of arrays of records.
  type frame_t is record
    samples : bus_array_t(1 downto 0);
    count   : natural range 0 to 100;
  end record;

end package;
