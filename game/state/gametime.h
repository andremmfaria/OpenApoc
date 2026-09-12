#pragma once

#include "library/strings.h"
#include <cstdint>

namespace OpenApoc
{

static constexpr unsigned VANILLA_TICKS_PER_SECOND = 36;
static constexpr unsigned TICKS_MULTIPLIER = 4;
static constexpr unsigned TICKS_PER_SECOND = VANILLA_TICKS_PER_SECOND * TICKS_MULTIPLIER;
static constexpr unsigned TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
static constexpr unsigned TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
static constexpr unsigned TICKS_PER_DAY = TICKS_PER_HOUR * 24;
static constexpr unsigned TURBO_TICKS = 5 * 60 * TICKS_PER_SECOND;

/*
    The original game's speed constant, at OpenApoc's tick resolution.

    The original accumulates `speed_value` once per its own real-time frame (a busy-wait
    pinned to 1193182 / 65536 Hz, ~18.206512 Hz) into a fixed-point counter where 36 units
    make one game second (see plans/997-original-pacing.md). Folding that /36 into
    TICKS_MULTIPLIER's ticks-per-vanilla-unit conversion gives an exact ticks-per-real-
    second rational:

        ticksPerSecond = (ratioNumerator / ratioDenominator) * TICKS_MULTIPLIER
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
	return VanillaTickRate{ratioNumerator * static_cast<uint64_t>(TICKS_MULTIPLIER) * 1193182ull,
	                       ratioDenominator * 65536ull};
}

class GameTime
{
  private:
	bool secondPassedFlag = false;
	bool fiveMinutesPassedFlag = false;
	bool hourPassedFlag = false;
	bool dayPassedFlag = false;
	bool weekPassedFlag = false;

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

	void clearFlags();

	static GameTime midday();
};
} // namespace OpenApoc
