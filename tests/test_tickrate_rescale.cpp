// Verifies the 144->180 TPS tick-rate flip (TICKS_MULTIPLIER 4 -> 5) preserves
// real-seconds behavior for every hand-rescaled constant.
//
// Comparing behavior across a tick-rate change requires per-second summaries (projectile
// range, fall time, timer durations in game-seconds) rather than raw tick traces, since the
// traces themselves necessarily change when TICKS_PER_SECOND does. Rather
// than re-deriving those summaries from a live simulation (which would need a scripted scenario
// with actual vehicle/unit motion - the shipped starting gamestate has none), this test encodes
// the same claim algebraically against the actual compiled constants: a rate constant R is
// correct iff R / TICKS_PER_SECOND still equals its known value at TICKS_PER_SECOND==144, and a
// per-tick acceleration A is correct iff A * TICKS_PER_SECOND (the resulting per-real-second
// acceleration) still equals its value at 144. Every comparison below is done with integer
// cross-multiplication (rates) or a small float epsilon (accelerations) against the historical
// 144-TPS constant, so this test would fail if TICKS_MULTIPLIER changed again without the
// corresponding hand-rescale.

#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battleitem.h"
#include "game/state/battle/battlemappart.h"
#include "game/state/battle/battlescanner.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/vehicle.h"
#include "game/state/gametime.h"
#include "game/state/rules/supportedmappart.h"
#include "game/state/shared/agent.h"
#include <cmath>

using namespace OpenApoc;

namespace
{

constexpr unsigned OLD_TICKS_PER_SECOND = 144;

bool test_flip()
{
	bool ok = true;
	if (TICKS_MULTIPLIER != 5)
	{
		LogError("test_flip: TICKS_MULTIPLIER = {0}, expected 5", TICKS_MULTIPLIER);
		ok = false;
	}
	if (TICKS_PER_SECOND != 180)
	{
		LogError("test_flip: TICKS_PER_SECOND = {0}, expected 180", TICKS_PER_SECOND);
		ok = false;
	}
	if (VANILLA_TO_TICKS != 5)
	{
		LogError("test_flip: VANILLA_TO_TICKS = {0}, expected 5", VANILLA_TO_TICKS);
		ok = false;
	}
	if (TICK_SCALE != 45)
	{
		LogError("test_flip: TICK_SCALE = {0}, expected 45", TICK_SCALE);
		ok = false;
	}
	return ok;
}

// value/TICKS_PER_SECOND (a real-seconds rate) must equal oldValue/144, checked as
// value*144 == oldValue*TICKS_PER_SECOND to stay in integer arithmetic.
bool checkRateInvariant(const char *name, unsigned value, unsigned oldValue)
{
	uint64_t lhs = static_cast<uint64_t>(value) * OLD_TICKS_PER_SECOND;
	uint64_t rhs = static_cast<uint64_t>(oldValue) * TICKS_PER_SECOND;
	if (lhs != rhs)
	{
		LogError("checkRateInvariant({0}): {1}/{2} != {3}/{4} (the two tick rates disagree on "
		         "the real-seconds rate this constant represents)",
		         name, value, TICKS_PER_SECOND, oldValue, OLD_TICKS_PER_SECOND);
		return false;
	}
	return true;
}

// value*TICKS_PER_SECOND (a real-per-second-squared acceleration) must equal
// oldValue*144, within float rounding.
bool checkAccelerationInvariant(const char *name, float value, float oldValue)
{
	float newPerSecond = value * static_cast<float>(TICKS_PER_SECOND);
	float oldPerSecond = oldValue * static_cast<float>(OLD_TICKS_PER_SECOND);
	if (std::fabs(newPerSecond - oldPerSecond) > 0.01f)
	{
		LogError("checkAccelerationInvariant({0}): {1} != {2} (per-real-second acceleration "
		         "drifted across the tick-rate flip)",
		         name, newPerSecond, oldPerSecond);
		return false;
	}
	return true;
}

bool test_rate_constants()
{
	bool ok = true;
	ok &= checkRateInvariant("TICKS_PER_UNIT_TRAVELLED_BATTLEUNIT",
	                         TICKS_PER_UNIT_TRAVELLED_BATTLEUNIT, 32);
	ok &= checkRateInvariant("TICKS_PER_FRAME_UNIT", TICKS_PER_FRAME_UNIT, 8);
	ok &= checkRateInvariant("TICKS_PER_FRAME_MAP_PART", TICKS_PER_FRAME_MAP_PART, 8);
	ok &= checkRateInvariant("TICKS_PER_UNIT_TRAVELLED_AGENT", TICKS_PER_UNIT_TRAVELLED_AGENT, 8);
	return ok;
}

bool test_acceleration_constants()
{
	bool ok = true;
	ok &= checkAccelerationInvariant("FALLING_ACCELERATION_UNIT", FALLING_ACCELERATION_UNIT,
	                                 0.16666667f);
	ok &= checkAccelerationInvariant("FALLING_ACCELERATION_ITEM", FALLING_ACCELERATION_ITEM,
	                                 0.14285714f);
	ok &= checkAccelerationInvariant("FALLING_ACCELERATION_MAP_PART", FALLING_ACCELERATION_MAP_PART,
	                                 0.16666667f);
	ok &= checkAccelerationInvariant("FV_ACCELERATION", FV_ACCELERATION, 0.16666667f);
	return ok;
}

// TURNING_SLOW_DOWN_CORRECTION is already expressed as k * TICK_SCALE / 36 (vehicle.h), so it
// needs no hand-edit for the flip - this just pins the resulting number so a future refactor that
// accidentally re-hardcodes it gets caught.
bool test_turning_slow_down_correction()
{
	float expected = 38.893f * static_cast<float>(TICK_SCALE) / 36.0f;
	if (std::fabs(TURNING_SLOW_DOWN_CORRECTION - expected) > 0.001f)
	{
		LogError("test_turning_slow_down_correction: {0} != {1}", TURNING_SLOW_DOWN_CORRECTION,
		         expected);
		return false;
	}
	// At TICKS_PER_SECOND==180 (TICK_SCALE==45) this should be ~48.6162, not the 144-TPS value
	// of 38.893.
	if (std::fabs(TURNING_SLOW_DOWN_CORRECTION - 48.6162f) > 0.01f)
	{
		LogError("test_turning_slow_down_correction: value {0} does not match the expected "
		         "~48.6162 at TICK_SCALE==45",
		         TURNING_SLOW_DOWN_CORRECTION);
		return false;
	}
	return true;
}

// The three named integer-division landmines that remain reachable from outside their own
// translation unit (UNIT_AI_THINK_INTERVAL in unitai.cpp and VEHICLE_ANIMATION_TICKS_PER_FRAME
// in vehicle.cpp are function-local/internal-linkage and are not checkable here; see their own
// doc comments). 180 is not divisible by 8 or 16, and the decision was to accept the
// truncation rather than re-express these as divisor-safe fractions - this test pins the
// accepted, truncated values.
bool test_accepted_truncation()
{
	bool ok = true;
	if (WEAPON_MISFIRE_DELAY_TICKS != 11)
	{
		LogError("test_accepted_truncation: WEAPON_MISFIRE_DELAY_TICKS = {0}, expected 11 "
		         "(180/16 truncated)",
		         WEAPON_MISFIRE_DELAY_TICKS);
		ok = false;
	}
	if (TICKS_PER_SCANNER_UPDATE != 22)
	{
		LogError("test_accepted_truncation: TICKS_PER_SCANNER_UPDATE = {0}, expected 22 "
		         "(180/8 truncated)",
		         TICKS_PER_SCANNER_UPDATE);
		ok = false;
	}
	return ok;
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool allPassed = true;
	allPassed &= test_flip();
	allPassed &= test_rate_constants();
	allPassed &= test_acceleration_constants();
	allPassed &= test_turning_slow_down_correction();
	allPassed &= test_accepted_truncation();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
