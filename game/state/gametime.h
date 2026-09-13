#pragma once

#include "library/strings.h"
#include <cstdint>

namespace OpenApoc
{

static constexpr unsigned VANILLA_TICKS_PER_SECOND = 36;
static constexpr unsigned TICKS_MULTIPLIER = 5;
static constexpr unsigned TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER;
static constexpr unsigned TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
static constexpr unsigned TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
static constexpr unsigned TICKS_PER_DAY = TICKS_PER_HOUR * 24;

/*
    Tick constant vocabulary.

    There is exactly one game clock, TICKS_PER_SECOND, and two named converters that must
    not be confused with each other or used interchangeably:

    - VANILLA_TO_TICKS converts a *vanilla data unit* (a value stored in, or derived from,
      the original's 36-per-second fixed-point base) into OpenApoc ticks. Use it for
      doodad lifetimes/frame timings, hazard/explosion timers, and extractor-baked fields
      such as fire_delay - anything whose source value is expressed in vanilla units.
    - TICKS_PER_VANILLA_FRAME names the *duration of one vanilla frame*, in ticks. It is
      numerically identical to VANILLA_TO_TICKS today (both are presently 4), but the two
      answer different questions - "how many ticks is this vanilla-unit value worth?" vs.
      "how many ticks is one vanilla frame long?" - and could diverge if either side of
      that relationship is ever retuned independently. Use it for one-vanilla-frame delays
      (map part / item collapse, explosion expansion ticks).

    The old undifferentiated multiplier survives only as the base ratio TICKS_PER_SECOND is
    built from, above (see its own definition); nothing else should reference it directly.

    TICK_SCALE, defined further down, is a separate physics-only divisor. It is presently
    36 as well, but that is a coincidence of TICKS_PER_SECOND's current value, not a
    relationship to preserve - see its own doc comment.
*/
static constexpr unsigned VANILLA_TO_TICKS = TICKS_PER_SECOND / VANILLA_TICKS_PER_SECOND;
static constexpr unsigned TICKS_PER_VANILLA_FRAME = TICKS_PER_SECOND / VANILLA_TICKS_PER_SECOND;

/*
    The original game's speed constant, at OpenApoc's tick resolution.

    The original accumulates `speed_value` once per its own real-time frame (a busy-wait
    pinned to 1193182 / 65536 Hz, ~18.206512 Hz) into a fixed-point counter where 36 units
    make one game second. Folding that /36 into
    VANILLA_TO_TICKS's ticks-per-vanilla-unit conversion gives an exact ticks-per-real-
    second rational:

        ticksPerSecond = (ratioNumerator / ratioDenominator) * VANILLA_TO_TICKS
                         * 1193182 / 65536

    `ratioDenominator` exists so a non-integer ratio (battle's provisional 0.5x tier)
    stays an exact fraction instead of being truncated. The result is meant to drive a
    TickAccumulator (framework/tickaccumulator.h) directly; no floating point is used.
*/
struct VanillaTickRate
{
	uint64_t numerator;
	uint64_t denominator;
};

constexpr VanillaTickRate vanillaTickRate(uint64_t ratioNumerator, uint64_t ratioDenominator = 1)
{
	return VanillaTickRate{ratioNumerator * static_cast<uint64_t>(VANILLA_TO_TICKS) * 1193182ull,
	                       ratioDenominator * 65536ull};
}

/*
    TICK_SCALE - the physics-only divisor used by per-tick velocity/movement math.

    Vanilla velocity data was calibrated such that real-world speed = 4 * v / velocityScale
    per second. Units check (game/state/shared/projectile.cpp): displacement per tick is
    (ticks / TICK_SCALE) * v / velocityScale, and ticks per real second is TICKS_PER_SECOND,
    so real speed = (TICKS_PER_SECOND / TICK_SCALE) * v / velocityScale. The ratio
    TICKS_PER_SECOND / TICK_SCALE = 4 is what must stay invariant - not TICK_SCALE's
    absolute value - so TICK_SCALE must scale with TICKS_PER_SECOND (144 -> 36, 180 -> 45).
    Freezing it at VANILLA_TICKS_PER_SECOND (36) would silently speed up all physics 25% at
    180 TPS.

    The /4 is a frozen vanilla speed-calibration factor. It is not VANILLA_TO_TICKS/
    TICKS_PER_VANILLA_FRAME above (those convert vanilla *data* units; this calibrates
    vanilla *velocity* data specifically), and its equality with VANILLA_TICKS_PER_SECOND's
    factor-of-36 relationship to TICKS_PER_SECOND=144 is coincidental, not causal. The
    original binary's decompilation established its clock rate (18.206512 Hz, 36 sub-second
    units per game second), not its velocity calibration - no vanilla projectile or movement
    code has been decompiled - so that work neither confirms nor refutes this /4 factor.
*/
static constexpr unsigned TICK_SCALE = TICKS_PER_SECOND / 4;
static_assert(TICKS_PER_SECOND / TICK_SCALE == 4,
              "TICK_SCALE must stay proportional to TICKS_PER_SECOND (ratio of 4) - see the "
              "doc comment above");

/*
    A full sweep of the codebase's compile-time `TICKS_PER_SECOND / N` and
    `TICKS_PER_SECOND * a / b` expressions found divisors 2, 3, 4, 6, 9, 12, 18 and the
    combined form `* 3 / 2` in use elsewhere, all of which
    must divide TICKS_PER_SECOND exactly or the corresponding cadence (PSI checks, enzyme ticks,
    turns, cloak timers, brainsuck, snooze delays...) silently drifts off its intended
    real-seconds duration. 144 and 180 both satisfy every one of these; a future change to
    TICKS_MULTIPLIER must keep doing so, or update the affected constant's own expression to a
    divisor-safe fraction (see WEAPON_MISFIRE_DELAY_TICKS, TICKS_PER_SCANNER_UPDATE,
    UNIT_AI_THINK_INTERVAL and VEHICLE_ANIMATION_TICKS_PER_FRAME's doc comments for the four
    known exceptions, which use /8, /16 and /72 and are deliberately left truncating).
*/
static_assert(TICKS_PER_SECOND % 2 == 0, "TICKS_PER_SECOND must stay divisible by 2");
static_assert(TICKS_PER_SECOND % 3 == 0, "TICKS_PER_SECOND must stay divisible by 3");
static_assert(TICKS_PER_SECOND % 4 == 0, "TICKS_PER_SECOND must stay divisible by 4");
static_assert(TICKS_PER_SECOND % 6 == 0, "TICKS_PER_SECOND must stay divisible by 6");
static_assert(TICKS_PER_SECOND % 9 == 0, "TICKS_PER_SECOND must stay divisible by 9");
static_assert(TICKS_PER_SECOND % 12 == 0, "TICKS_PER_SECOND must stay divisible by 12");
static_assert(TICKS_PER_SECOND % 18 == 0, "TICKS_PER_SECOND must stay divisible by 18");

class GameTime
{
  private:
	bool secondPassedFlag = false;
	bool fiveMinutesPassedFlag = false;
	bool hourPassedFlag = false;
	bool dayPassedFlag = false;
	bool weekPassedFlag = false;

	// Boundary counts accrued since the last clearFlags(), mirroring the *PassedFlag
	// booleans above but counting every crossing instead of collapsing them to one bit.
	// A single addTicks() call can span many boundaries (turbo, cheat-menu time skips);
	// without this, callers that only see a bool run their once-per-boundary logic once
	// no matter how many boundaries actually elapsed.
	uint64_t secondsElapsedCount = 0;
	uint64_t fiveMinutesElapsedCount = 0;
	uint64_t hoursElapsedCount = 0;
	uint64_t daysElapsedCount = 0;
	uint64_t weeksElapsedCount = 0;

  public:
	uint64_t ticks = 0;
	GameTime() = default;
	GameTime(uint64_t ticks);

	void addTicks(uint64_t ticks);

	unsigned int getHours() const;

	unsigned int getMinutes() const;

	unsigned int getSeconds() const;

	unsigned int getMonthDay() const;

	unsigned int getDay() const;

	unsigned int getWeek() const;

	unsigned int getMonth() const;

	unsigned int getFirstDayOfCurrentWeek() const;

	unsigned int getLastDayOfCurrentWeek() const;

	unsigned int getLastDayOfCurrentMonth() const;

	unsigned int getTicksBetween(unsigned int fromDays, unsigned int fromHours,
	                             unsigned int fromMinutes, unsigned int fromSeconds,
	                             unsigned int toDays, unsigned int toHours, unsigned int toMinutes,
	                             unsigned int toSeconds) const;
	uint64_t getTicks() const;

	// returns week with prefix
	UString getWeekString() const;

	// returns formatted time in format hh:mm
	UString getShortTimeString() const;

	// returns formatted time in format hh:mm:ss
	UString getLongTimeString() const;

	// returns formatted date in format a, d m, y
	UString getLongDateString() const;

	// returns formatted date in format d m, y
	UString getShortDateString() const;

	// set at end of each second
	bool secondPassed() const;
	void setSecondPassed(bool newValue) { secondPassedFlag = newValue; }

	// set at end of each 5 minutes
	bool fiveMinutesPassed() const;
	void setFiveMinutesPassed(bool newValue) { fiveMinutesPassedFlag = newValue; }

	// set at end of each hour
	bool hourPassed() const;
	void setHourPassed(bool newValue) { hourPassedFlag = newValue; }

	// set at midnight
	bool dayPassed() const;
	void setDayPassed(bool newValue) { dayPassedFlag = newValue; }

	// set at sunday midnight
	bool weekPassed() const;
	void setWeekPassed(bool newValue) { weekPassedFlag = newValue; }

	// How many of each boundary elapsed since the last clearFlags(), as opposed to the
	// *Passed() bools above which only say whether at least one did. A caller that must
	// run once-per-boundary work (fuel burn, cargo expiry, per-second agent updates) at
	// the correct rate under a multi-boundary addTicks() call needs the count, not the
	// bool - see GameState::update().
	uint64_t secondsElapsed() const;
	uint64_t fiveMinutePeriodsElapsed() const;
	uint64_t hoursElapsed() const;
	uint64_t daysElapsed() const;
	uint64_t weeksElapsed() const;

	void clearFlags();

	static GameTime midday();
};
} // namespace OpenApoc
