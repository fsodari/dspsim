-- The skid buffer of hdl/Skid.sv in VHDL. The tests check that the two behave the same.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity Skid is
  generic (
    DW : positive := 24
  );
  port (
    clk           : in  std_logic;
    rst           : in  std_logic;
    s_axis_tdata  : in  signed(DW-1 downto 0);
    s_axis_tvalid : in  std_logic;
    s_axis_tready : out std_logic;
    m_axis_tdata  : out signed(DW-1 downto 0);
    m_axis_tvalid : out std_logic;
    m_axis_tready : in  std_logic
  );
end entity;

architecture rtl of Skid is
  signal skid_tdata  : signed(DW-1 downto 0) := (others => '0');
  signal skid_tvalid : std_logic := '0';
  signal m_tvalid    : std_logic := '0';
  signal s_tready    : std_logic;
begin
  s_tready      <= not skid_tvalid;
  s_axis_tready <= s_tready;
  m_axis_tvalid <= m_tvalid;

  process (clk)
  begin
    if rising_edge(clk) then
      if skid_tvalid = '1' and (m_tvalid = '0' or m_axis_tready = '1') then
        skid_tvalid <= '0';
      end if;
      if (s_axis_tvalid = '1' and s_tready = '1') and (m_tvalid = '1' and m_axis_tready = '0') then
        skid_tdata  <= s_axis_tdata;
        skid_tvalid <= '1';
      end if;
      if rst = '1' then
        skid_tvalid <= '0';
      end if;
    end if;
  end process;

  process (clk)
  begin
    if rising_edge(clk) then
      if m_tvalid = '1' and m_axis_tready = '1' then
        m_tvalid <= '0';
      end if;
      if (skid_tvalid = '1' or s_axis_tvalid = '1') and (m_tvalid = '0' or m_axis_tready = '1') then
        if skid_tvalid = '1' then
          m_axis_tdata <= skid_tdata;
        else
          m_axis_tdata <= s_axis_tdata;
        end if;
        m_tvalid <= '1';
      end if;
      if rst = '1' then
        m_tvalid <= '0';
      end if;
    end if;
  end process;
end architecture;
