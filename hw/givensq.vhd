-- GIVENSQ -- custom instruction, hardware implementation.
--
-- Scenario step 8: an automaton (Moore FSM, D flip-flops) driving a datapath,
-- with a testbench of known inputs and outputs, simulated to obtain the
-- latency that the software cannot measure.
--
-- Computes, for Rn = opposite and Rm = adjacent:
--     result(31 downto 16) = sin(theta), Q14
--     result(15 downto  0) = cos(theta), Q14
--     theta = atan2(opposite, adjacent)
--
-- Bit-identical to src/common/givensq_fw.c and src/common/givensq_fw.S: all
-- three take their coefficients from one generator (givensq_pkg.vhd), and one
-- vector file validates all three.
--
-- Structure, and why:
--   * The DIVIDER is sequential -- one restoring step per clock, 14 clocks.
--     It is the whole reason this unit is multi-cycle. See docs/HARDWARE.md
--     for the reciprocal-LUT alternative and what it costs.
--   * sin and cos are evaluated BY TWO SEPARATE PWL BLOCKS IN THE SAME CYCLE.
--     This is the one thing hardware buys that firmware cannot: the 2-issue
--     microcode still alternates, the hardware genuinely overlaps.
--   * No branch anywhere in the datapath. Segment selection is a ROM address,
--     and the |x| > pi/4 fold is a mux, so there is nothing to mispredict.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.givensq_pkg.all;

entity givensq is
  port (
    clk    : in  std_logic;
    rst    : in  std_logic;
    start  : in  std_logic;
    opp    : in  signed(31 downto 0);
    adj    : in  signed(31 downto 0);
    done   : out std_logic;
    result : out signed(31 downto 0)
  );
end entity givensq;

architecture rtl of givensq is

  type state_t is (S_IDLE, S_PREP, S_DIV, S_ARCTAN, S_FIX, S_TRIG, S_DONE);
  signal state : state_t := S_IDLE;

  -- divider registers
  signal rem_r   : unsigned(31 downto 0) := (others => '0');
  signal quo_r   : unsigned(31 downto 0) := (others => '0');
  signal div_d   : unsigned(31 downto 0) := (others => '0');
  signal iter    : integer range 0 to TRIG_Q := 0;

  -- control flags captured in S_PREP
  signal neg_r   : boolean := false;   -- ratio is negative
  signal swp_r   : boolean := false;   -- operands were swapped
  signal angle_r : signed(31 downto 0) := (others => '0');

  -- One PWL segment: index is a shift, coefficients come from ROM, and the
  -- product is a single multiply because the CSD-constrained slope makes the
  -- multiplier optional -- see docs/HARDWARE.md, where the shift-add
  -- alternative turns out to cost about the same area.
  function pwl (rom : coeff_array; x : signed(31 downto 0))
    return signed is
    variable idx  : integer;
    variable prod : signed(63 downto 0);
    variable acc  : signed(63 downto 0);
  begin
    idx := to_integer(shift_right(x, INDEX_SHIFT));
    if idx < 0 then
      idx := 0;
    elsif idx > rom'high then
      idx := rom'high;
    end if;
    prod := to_signed(rom(idx).m, 32) * x;
    acc  := shift_right(prod, TRIG_Q) + to_signed(rom(idx).b, 64);
    return resize(acc, 32);
  end function pwl;

  function pwl_arctan (x : signed(31 downto 0)) return signed is
    variable r : signed(31 downto 0);
  begin
    r := pwl(ROM_ARCTAN, abs(x));
    if x < 0 then
      r := -r;                          -- arctan is odd
    end if;
    return r;
  end function pwl_arctan;

  -- cos(x) = sin(pi/2 - x) beyond pi/4. That argument GOES NEGATIVE for
  -- |x| > pi/2, and sine is odd, so the sign has to be carried across the
  -- fold rather than dropped by the abs.
  function pwl_cos (a : signed(31 downto 0)) return signed is
    variable ax, arg, r : signed(31 downto 0);
  begin
    ax := abs(a);                       -- cosine is even
    if ax > to_signed(PI_4_FIXED, 32) then
      arg := to_signed(PI_2_FIXED, 32) - ax;
      r   := pwl(ROM_SIN, abs(arg));
      if arg < 0 then
        r := -r;
      end if;
      return r;
    end if;
    return pwl(ROM_COS, ax);
  end function pwl_cos;

  -- sin(x) = cos(pi/2 - x) beyond pi/4. Cosine is even, so abs is safe here.
  function pwl_sin (a : signed(31 downto 0)) return signed is
    variable ax, arg, r : signed(31 downto 0);
  begin
    ax := abs(a);
    if ax > to_signed(PI_4_FIXED, 32) then
      arg := to_signed(PI_2_FIXED, 32) - ax;
      r   := pwl(ROM_COS, abs(arg));
    else
      r   := pwl(ROM_SIN, ax);
    end if;
    if a < 0 then
      r := -r;                          -- sine is odd
    end if;
    return r;
  end function pwl_sin;

begin

  process (clk)
    variable ao, ad : unsigned(31 downto 0);
    variable n, d   : unsigned(31 downto 0);
    variable r2     : unsigned(31 downto 0);
    variable ratio  : signed(31 downto 0);
    variable ang    : signed(31 downto 0);
    variable cv, sv : signed(31 downto 0);
  begin
    if rising_edge(clk) then
      if rst = '1' then
        state  <= S_IDLE;
        done   <= '0';
        result <= (others => '0');
      else
        case state is

          when S_IDLE =>
            done <= '0';
            if start = '1' then
              state <= S_PREP;
            end if;

          -- |o|, |a|, sign of the ratio, and the swap that guarantees n <= d
          when S_PREP =>
            ao := unsigned(abs(opp));
            ad := unsigned(abs(adj));
            neg_r <= ((opp < 0) xor (adj < 0));
            if adj = 0 then
              -- vertical: theta = +/- pi/2, no divide and no arctan at all
              if opp >= 0 then
                angle_r <= to_signed(PI_2_FIXED, 32);
              else
                angle_r <= to_signed(-PI_2_FIXED, 32);
              end if;
              state <= S_TRIG;
            else
              if ao > ad then
                swp_r <= true;  n := ad; d := ao;
              else
                swp_r <= false; n := ao; d := ad;
              end if;
              if n >= d then
                -- exactly 1.0; a 14-step binary loop cannot produce a 15-bit
                -- quotient, so it is a separate case (as in the C model)
                quo_r <= to_unsigned(2 ** TRIG_Q, 32);
                state <= S_ARCTAN;
              else
                rem_r <= n;
                quo_r <= (others => '0');
                div_d <= d;
                iter  <= 0;
                state <= S_DIV;
              end if;
            end if;

          -- one restoring step per clock: shift, compare, conditional subtract
          when S_DIV =>
            r2 := shift_left(rem_r, 1);
            if r2 >= div_d then
              rem_r <= r2 - div_d;
              quo_r <= shift_left(quo_r, 1) or to_unsigned(1, 32);
            else
              rem_r <= r2;
              quo_r <= shift_left(quo_r, 1);
            end if;
            if iter = TRIG_Q - 1 then
              state <= S_ARCTAN;
            else
              iter <= iter + 1;
            end if;

          when S_ARCTAN =>
            ratio := signed(quo_r);
            if neg_r then
              ratio := -ratio;
            end if;
            angle_r <= pwl_arctan(ratio);
            state   <= S_FIX;

          -- quadrant fix-up for the swapped case
          when S_FIX =>
            if swp_r then
              if neg_r then
                angle_r <= to_signed(-PI_2_FIXED, 32) - angle_r;
              else
                angle_r <= to_signed(PI_2_FIXED, 32) - angle_r;
              end if;
            end if;
            state <= S_TRIG;

          -- BOTH PWL blocks evaluate here, in the same cycle
          when S_TRIG =>
            cv := pwl_cos(angle_r);
            sv := pwl_sin(angle_r);
            result <= signed(std_logic_vector(sv(15 downto 0)) &
                             std_logic_vector(cv(15 downto 0)));
            state  <= S_DONE;

          when S_DONE =>
            done  <= '1';
            state <= S_IDLE;

        end case;
      end if;
    end if;
  end process;

end architecture rtl;
