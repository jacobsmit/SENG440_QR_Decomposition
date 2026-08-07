-- Testbench for GIVENSQ: known inputs with known outputs (scenario step 8).
--
-- Vectors come from hw/vectors.txt, produced by tools/gen_hw_vectors.c from the
-- C firmware model. The same model specifies the ARM assembly, so a pass here
-- means the C, the assembly and the hardware all agree bit-for-bit.
--
-- It also MEASURES LATENCY -- start to done, in clocks -- which is the number
-- the software cannot produce and which the whole hardware-vs-software
-- comparison depends on.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

entity tb_givensq is
end entity tb_givensq;

architecture sim of tb_givensq is

  constant PERIOD : time := 10 ns;

  signal clk    : std_logic := '0';
  signal rst    : std_logic := '1';
  signal start  : std_logic := '0';
  signal opp    : signed(31 downto 0) := (others => '0');
  signal adj    : signed(31 downto 0) := (others => '0');
  signal done   : std_logic;
  signal result : signed(31 downto 0);

  signal running : boolean := true;

begin

  uut : entity work.givensq
    port map (clk => clk, rst => rst, start => start,
              opp => opp, adj => adj, done => done, result => result);

  clk <= (not clk) after PERIOD / 2 when running else '0';

  stim : process
    file     vf       : text;
    variable l        : line;
    variable ol       : line;
    variable vo       : integer;
    variable va       : integer;
    variable vexp     : integer;
    variable good     : integer := 0;
    variable bad      : integer := 0;
    variable cycles   : integer := 0;
    variable lat_min  : integer := integer'high;
    variable lat_max  : integer := 0;
    variable lat_sum  : integer := 0;
    variable status   : file_open_status;
  begin
    file_open(status, vf, "hw/vectors.txt", read_mode);
    if status /= open_ok then
      report "cannot open hw/vectors.txt -- run 'make hw-vectors' first"
        severity failure;
    end if;

    rst <= '1';
    wait for 4 * PERIOD;
    wait until rising_edge(clk);
    rst <= '0';
    wait until rising_edge(clk);

    while not endfile(vf) loop
      readline(vf, l);
      read(l, vo);
      read(l, va);
      read(l, vexp);

      opp   <= to_signed(vo, 32);
      adj   <= to_signed(va, 32);
      start <= '1';
      wait until rising_edge(clk);
      start <= '0';

      cycles := 0;
      while done /= '1' loop
        wait until rising_edge(clk);
        cycles := cycles + 1;
      end loop;

      if to_integer(result) /= vexp then
        if bad < 5 then
          write(ol, string'("  MISMATCH o="));
          write(ol, vo);
          write(ol, string'(" a="));
          write(ol, va);
          write(ol, string'(" got="));
          write(ol, to_integer(result));
          write(ol, string'(" want="));
          write(ol, vexp);
          writeline(output, ol);
        end if;
        bad := bad + 1;
      else
        good := good + 1;
      end if;

      -- latency: start pulse to done, in clocks
      if cycles < lat_min then lat_min := cycles; end if;
      if cycles > lat_max then lat_max := cycles; end if;
      lat_sum := lat_sum + cycles;

      wait until rising_edge(clk);
    end loop;

    file_close(vf);

    write(ol, string'("GIVENSQ hardware testbench"));
    writeline(output, ol);
    write(ol, string'("  vectors passed : "));
    write(ol, good);
    writeline(output, ol);
    write(ol, string'("  vectors failed : "));
    write(ol, bad);
    writeline(output, ol);
    write(ol, string'("  latency min    : "));
    write(ol, lat_min);
    write(ol, string'(" clocks"));
    writeline(output, ol);
    write(ol, string'("  latency max    : "));
    write(ol, lat_max);
    write(ol, string'(" clocks  (the divide is data independent, so"));
    write(ol, string'(" min = max unless a shortcut path was taken)"));
    writeline(output, ol);

    if bad = 0 then
      write(ol, string'("  RESULT: PASS"));
    else
      write(ol, string'("  RESULT: FAIL"));
    end if;
    writeline(output, ol);

    running <= false;
    if bad /= 0 then
      report "GIVENSQ testbench FAILED" severity failure;
    end if;
    wait;
  end process stim;

end architecture sim;
