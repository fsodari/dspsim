-- Adds its input to a sum that is four bits wider. An entity that VhdlModel instantiates.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity Accumulator is
  generic (
    W : positive := 8
  );
  port (
    clk : in  std_logic;
    rst : in  std_logic;
    d   : in  signed(W-1 downto 0);
    q   : out signed(W+3 downto 0)
  );
end entity;

architecture rtl of Accumulator is
  signal sum : signed(W+3 downto 0) := (others => '0');
begin
  process (clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        sum <= (others => '0');
      else
        sum <= sum + resize(d, sum'length);
      end if;
    end if;
  end process;
  q <= sum;
end architecture;
