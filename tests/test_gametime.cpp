#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/gametime.h"

using namespace OpenApoc;

static bool checkFlags(const GameTime &gt, const char *testName, bool expectSecond,
                       bool expectFiveMinutes, bool expectHour, bool expectDay, bool expectWeek)
{
	bool ok = true;
	if (gt.secondPassed() != expectSecond)
	{
		LogError("{0}: secondPassed() was {1}, expected {2}", testName, gt.secondPassed(),
		         expectSecond);
		ok = false;
	}
	if (gt.fiveMinutesPassed() != expectFiveMinutes)
	{
		LogError("{0}: fiveMinutesPassed() was {1}, expected {2}", testName, gt.fiveMinutesPassed(),
		         expectFiveMinutes);
		ok = false;
	}
	if (gt.hourPassed() != expectHour)
	{
		LogError("{0}: hourPassed() was {1}, expected {2}", testName, gt.hourPassed(), expectHour);
		ok = false;
	}
	if (gt.dayPassed() != expectDay)
	{
		LogError("{0}: dayPassed() was {1}, expected {2}", testName, gt.dayPassed(), expectDay);
		ok = false;
	}
	if (gt.weekPassed() != expectWeek)
	{
		LogError("{0}: weekPassed() was {1}, expected {2}", testName, gt.weekPassed(), expectWeek);
		ok = false;
	}
	return ok;
}

static bool test_within_second_no_flag()
{
	GameTime gt(100);
	gt.addTicks(10);
	return checkFlags(gt, "test_within_second_no_flag", false, false, false, false, false);
}

static bool test_second_boundary()
{
	GameTime gt(TICKS_PER_SECOND - 1);
	gt.addTicks(2);
	return checkFlags(gt, "test_second_boundary", true, false, false, false, false);
}

static bool test_five_minute_boundary()
{
	GameTime gt(5 * TICKS_PER_MINUTE - 1);
	gt.addTicks(2);
	return checkFlags(gt, "test_five_minute_boundary", true, true, false, false, false);
}

static bool test_hour_boundary()
{
	GameTime gt(TICKS_PER_HOUR - 1);
	gt.addTicks(2);
	return checkFlags(gt, "test_hour_boundary", true, true, true, false, false);
}

static bool test_day_boundary()
{
	GameTime gt(TICKS_PER_DAY - 1);
	gt.addTicks(2);
	if (!checkFlags(gt, "test_day_boundary", true, true, true, true, false))
	{
		return false;
	}
	if (gt.getDay() != 2)
	{
		LogError("test_day_boundary: getDay() was {0}, expected 2", gt.getDay());
		return false;
	}
	return true;
}

static bool test_week_boundary_tuesday_start()
{
	GameTime gt(6 * TICKS_PER_DAY - 1);
	gt.addTicks(2);
	if (!checkFlags(gt, "test_week_boundary_tuesday_start", true, true, true, true, true))
	{
		return false;
	}
	if (gt.getDay() != 7)
	{
		LogError("test_week_boundary_tuesday_start: getDay() was {0}, expected 7", gt.getDay());
		return false;
	}
	return true;
}

static bool test_flags_persist_until_cleared()
{
	GameTime gt(TICKS_PER_SECOND - 1);
	gt.addTicks(2);
	if (!checkFlags(gt, "test_flags_persist_until_cleared (before clear)", true, false, false,
	                false, false))
	{
		return false;
	}
	if (!gt.secondPassed())
	{
		LogError("test_flags_persist_until_cleared: secondPassed() did not persist across a "
		         "repeated call before clearFlags()");
		return false;
	}
	gt.clearFlags();
	return checkFlags(gt, "test_flags_persist_until_cleared (after clear)", false, false, false,
	                  false, false);
}

// A call spanning many seconds (a turbo tick, a cheat-menu time skip) must report every
// boundary it crossed, not just whether at least one was crossed - this is the SS3.5 fix:
// the old code fired each *PassedFlag at most once per addTicks() call.
static bool test_multi_second_call_counts_every_second()
{
	GameTime gt(0);
	// 300 whole seconds plus a partial second, matching a turbo-scale call.
	gt.addTicks(300 * TICKS_PER_SECOND + 1);
	if (gt.secondsElapsed() != 300)
	{
		LogError("test_multi_second_call_counts_every_second: secondsElapsed() was {0}, "
		         "expected 300",
		         gt.secondsElapsed());
		return false;
	}
	return true;
}

// A single call spanning a week must correctly count every cadence within it, not just the
// coarsest one: this is what the cheat menu's "fast forward a week" button does today.
static bool test_week_long_call_counts_every_cadence()
{
	GameTime gt(0);
	gt.addTicks(TICKS_PER_DAY * 7);
	if (gt.secondsElapsed() != TICKS_PER_DAY * 7 / TICKS_PER_SECOND)
	{
		LogError("test_week_long_call_counts_every_cadence: secondsElapsed() was {0}, "
		         "expected {1}",
		         gt.secondsElapsed(), TICKS_PER_DAY * 7 / TICKS_PER_SECOND);
		return false;
	}
	if (gt.fiveMinutePeriodsElapsed() != TICKS_PER_DAY * 7 / (5 * TICKS_PER_MINUTE))
	{
		LogError("test_week_long_call_counts_every_cadence: fiveMinutePeriodsElapsed() was "
		         "{0}, expected {1}",
		         gt.fiveMinutePeriodsElapsed(), TICKS_PER_DAY * 7 / (5 * TICKS_PER_MINUTE));
		return false;
	}
	if (gt.hoursElapsed() != TICKS_PER_DAY * 7 / TICKS_PER_HOUR)
	{
		LogError("test_week_long_call_counts_every_cadence: hoursElapsed() was {0}, "
		         "expected {1}",
		         gt.hoursElapsed(), TICKS_PER_DAY * 7 / TICKS_PER_HOUR);
		return false;
	}
	if (gt.daysElapsed() != 7)
	{
		LogError("test_week_long_call_counts_every_cadence: daysElapsed() was {0}, expected 7",
		         gt.daysElapsed());
		return false;
	}
	// Starting at tick 0 (day index 0, a Tuesday), day indices 6 and 13 both satisfy the
	// week-rollover congruence, so a 7-day span from day 0 crosses exactly one week
	// boundary (day 6); day 13 falls exactly on the far edge and is not < the 7-day span.
	if (gt.weeksElapsed() != 1)
	{
		LogError("test_week_long_call_counts_every_cadence: weeksElapsed() was {0}, "
		         "expected 1",
		         gt.weeksElapsed());
		return false;
	}
	return true;
}

// Counts accumulate across multiple addTicks() calls the same way the *PassedFlag bools
// do, and both are zeroed together by clearFlags(): GameState::update() relies on this to
// see a cheat-menu jump's counts on whichever later addTicks() call actually checks them.
static bool test_counts_accumulate_like_flags_until_cleared()
{
	GameTime gt(TICKS_PER_SECOND - 1);
	gt.addTicks(2);                // crosses one second boundary
	gt.addTicks(TICKS_PER_SECOND); // crosses one more
	if (gt.secondsElapsed() != 2)
	{
		LogError("test_counts_accumulate_like_flags_until_cleared: secondsElapsed() was "
		         "{0}, expected 2",
		         gt.secondsElapsed());
		return false;
	}
	if (!gt.secondPassed())
	{
		LogError("test_counts_accumulate_like_flags_until_cleared: secondPassed() was "
		         "false after two boundary-crossing calls");
		return false;
	}
	gt.clearFlags();
	if (gt.secondsElapsed() != 0 || gt.secondPassed())
	{
		LogError("test_counts_accumulate_like_flags_until_cleared: clearFlags() did not "
		         "zero both the count and the flag");
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool allPassed = true;
	allPassed &= test_within_second_no_flag();
	allPassed &= test_second_boundary();
	allPassed &= test_five_minute_boundary();
	allPassed &= test_hour_boundary();
	allPassed &= test_day_boundary();
	allPassed &= test_week_boundary_tuesday_start();
	allPassed &= test_flags_persist_until_cleared();
	allPassed &= test_multi_second_call_counts_every_second();
	allPassed &= test_week_long_call_counts_every_cadence();
	allPassed &= test_counts_accumulate_like_flags_until_cleared();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
