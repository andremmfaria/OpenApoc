#include "framework/configfile.h"
#include "framework/logger.h"
#include "framework/tickaccumulator.h"
#include "game/state/gametime.h"

#include <cmath>
#include <cstdint>

using namespace OpenApoc;

namespace
{

bool nearlyEqual(double a, double b, double relTolerance)
{
	double diff = std::fabs(a - b);
	double scale = std::max(std::fabs(a), std::fabs(b));
	if (scale == 0.0)
	{
		return diff < 1e-9;
	}
	return (diff / scale) < relTolerance;
}

// Feeds `accumulator` `frameCount` frames of `frameUs` microseconds each, returning the
// total whole ticks produced.
uint64_t runFrames(TickAccumulator &accumulator, uint64_t frameUs, uint64_t frameCount)
{
	uint64_t total = 0;
	for (uint64_t i = 0; i < frameCount; i++)
	{
		total += accumulator.advance(frameUs);
	}
	return total;
}

TickAccumulator makeUnceilinged(uint64_t rateNumerator, uint64_t rateDenominator,
                                uint64_t elapsedClampUs)
{
	auto ceiling = TickAccumulator::ticksInOneClamp(rateNumerator, rateDenominator, elapsedClampUs);
	return TickAccumulator(rateNumerator, rateDenominator, elapsedClampUs, ceiling);
}

} // anonymous namespace

// SS3.8: changing TargetFPS must change smoothness only - the delivered rate (and so
// game-second duration) has to be the same no matter which frame cadence is used to
// deliver it. Exercised at 30/60/120/144 FPS plus a throttled 50 Hz cadence, on the
// city's Speed1 rate.
static bool test_fps_invariance()
{
	auto rate = vanillaTickRate(1); // city Speed1
	const uint64_t simulatedSeconds = 20;
	const uint64_t totalUs = simulatedSeconds * 1000000ull;

	double referenceRate = -1.0;
	bool ok = true;
	for (uint64_t fps : {30ull, 50ull, 60ull, 120ull, 144ull})
	{
		uint64_t frameUs = 1000000ull / fps;
		uint64_t frames = totalUs / frameUs;
		auto accumulator = makeUnceilinged(rate.numerator, rate.denominator, 250000);
		uint64_t ticks = runFrames(accumulator, frameUs, frames);
		double simulatedUs = static_cast<double>(frames * frameUs);
		double ticksPerSecond = static_cast<double>(ticks) / (simulatedUs / 1e6);
		if (referenceRate < 0.0)
		{
			referenceRate = ticksPerSecond;
			continue;
		}
		if (!nearlyEqual(ticksPerSecond, referenceRate, 0.001))
		{
			LogError("test_fps_invariance: {0} FPS gave {1} ticks/s, expected ~{2}", fps,
			         ticksPerSecond, referenceRate);
			ok = false;
		}
	}
	return ok;
}

// SS3.8: city Speed1..Speed4 must match the original's measured rates to tighter than 1%.
static bool test_city_tiers_match_measured_rates()
{
	struct Target
	{
		uint64_t ratioNumerator;
		double gameSecondsPerRealSecond;
		const char *name;
	};
	static const Target targets[] = {
	    {1, 0.505736, "Speed1"},
	    {2, 1.011473, "Speed2"},
	    {4, 2.022946, "Speed3"},
	    {6, 3.034419, "Speed4"},
	};
	bool ok = true;
	for (const auto &t : targets)
	{
		auto rate = vanillaTickRate(t.ratioNumerator);
		double actualTicksPerSecond =
		    static_cast<double>(rate.numerator) / static_cast<double>(rate.denominator);
		double expectedTicksPerSecond = t.gameSecondsPerRealSecond * TICKS_PER_SECOND;
		if (!nearlyEqual(actualTicksPerSecond, expectedTicksPerSecond, 0.01))
		{
			LogError("test_city_tiers_match_measured_rates: {0} gave {1} ticks/s, expected "
			         "{2} (tolerance 1%)",
			         t.name, actualTicksPerSecond, expectedTicksPerSecond);
			ok = false;
		}
	}
	return ok;
}

// battle's provisional {0, 0.5, 1, 2} ratios, at the corrected absolute scale.
static bool test_battle_tiers_match_provisional_ratios()
{
	struct Target
	{
		uint64_t ratioNumerator;
		uint64_t ratioDenominator;
		double expectedRatio;
		const char *name;
	};
	static const Target targets[] = {
	    {1, 2, 0.5, "Speed1"},
	    {1, 1, 1.0, "Speed2"},
	    {2, 1, 2.0, "Speed3"},
	};
	auto speed2 = vanillaTickRate(1, 1);
	double speed2Rate =
	    static_cast<double>(speed2.numerator) / static_cast<double>(speed2.denominator);

	bool ok = true;
	for (const auto &t : targets)
	{
		auto rate = vanillaTickRate(t.ratioNumerator, t.ratioDenominator);
		double actualTicksPerSecond =
		    static_cast<double>(rate.numerator) / static_cast<double>(rate.denominator);
		double expectedTicksPerSecond = t.expectedRatio * speed2Rate;
		if (!nearlyEqual(actualTicksPerSecond, expectedTicksPerSecond, 1e-9))
		{
			LogError("test_battle_tiers_match_provisional_ratios: {0} gave {1} ticks/s, "
			         "expected {2}",
			         t.name, actualTicksPerSecond, expectedTicksPerSecond);
			ok = false;
		}
	}
	return ok;
}

// A stall (load screen, alt-tab) must not hand the accumulator's *input* an unbounded
// delta: elapsedClampUs bounds it independently of the tick ceiling.
static bool test_elapsed_input_clamp()
{
	// A contrived 1 tick/us rate keeps the arithmetic easy to hand-check.
	uint64_t rateNumerator = 1000000;
	uint64_t rateDenominator = 1;
	uint64_t elapsedClampUs = 1000;
	// A ceiling far above what the clamp could ever produce, so only the input clamp is
	// under test here.
	TickAccumulator accumulator(rateNumerator, rateDenominator, elapsedClampUs, 1000000);

	// At 1 tick/us, elapsedClampUs of (clamped) input yields exactly elapsedClampUs ticks -
	// not the ~5,000,000 the unclamped 5-second stall would otherwise produce.
	uint64_t ticks = accumulator.advance(5000000);
	if (ticks != elapsedClampUs)
	{
		LogError("test_elapsed_input_clamp: stall produced {0} ticks, expected the clamped "
		         "{1}",
		         ticks, elapsedClampUs);
		return false;
	}
	return true;
}

// The per-frame tick ceiling must discard excess ticks rather than carry them, and must
// zero the fractional remainder when it trips (SS3.3): otherwise one stall becomes a
// sustained catch-up that keeps re-tripping the ceiling on later frames.
static bool test_ceiling_discards_and_resets_remainder()
{
	// rate = 3.5 ticks/second: a one-second input produces 3 whole ticks (over the
	// ceiling below) and leaves a half-tick remainder to check is really zeroed.
	uint64_t rateNumerator = 7;
	uint64_t rateDenominator = 2;
	uint64_t elapsedClampUs = 1000000; // 1 second
	uint64_t maxTicksPerAdvance = 1;   // below the 3 raw ticks the first call produces
	TickAccumulator accumulator(rateNumerator, rateDenominator, elapsedClampUs, maxTicksPerAdvance);

	uint64_t ticks = accumulator.advance(1000000); // raw: 3 ticks + a 0.5-tick remainder
	if (ticks != maxTicksPerAdvance)
	{
		LogError("test_ceiling_discards_and_resets_remainder: first call returned {0}, "
		         "expected the ceiling of {1}",
		         ticks, maxTicksPerAdvance);
		return false;
	}

	// If the 0.5-tick remainder had survived the ceiling trip, adding another ~0.5 tick's
	// worth here would round up to a whole tick, which is still within maxTicksPerAdvance
	// and so would NOT be caught by the ceiling - it must not happen: the remainder was
	// zeroed when the ceiling tripped.
	ticks = accumulator.advance(285714);
	if (ticks != 0)
	{
		LogError("test_ceiling_discards_and_resets_remainder: second call returned {0}, "
		         "expected 0 - the ceiling trip did not zero the remainder",
		         ticks);
		return false;
	}
	return true;
}

// Pause (and Stage::resume(), SS3.4) must zero the fractional remainder so a stale
// fraction from a previous speed never surfaces as a tick after resuming.
static bool test_reset_zeroes_remainder()
{
	uint64_t rateNumerator = 3;
	uint64_t rateDenominator = 2; // 1.5 ticks/second
	TickAccumulator accumulator(rateNumerator, rateDenominator, 1000000, 1000000);

	accumulator.advance(1000000); // leaves a 0.5-tick remainder
	accumulator.reset();

	// Without the reset, adding another ~0.5 tick's worth would round up to a whole tick.
	uint64_t ticks = accumulator.advance(333334);
	if (ticks != 0)
	{
		LogError("test_reset_zeroes_remainder: got {0} ticks after reset(), expected 0", ticks);
		return false;
	}
	return true;
}

// A covered view accrues zero ticks simply by not being advance()d (Framework only
// updates the top stage - SS3.4); the first post-resume frame must then behave like an
// ordinary frame, not a burst.
static bool test_covered_view_then_resume_is_one_ordinary_frame()
{
	auto rate = vanillaTickRate(1); // city Speed1
	uint64_t frameUs = 1000000ull / 60ull;

	TickAccumulator fresh = makeUnceilinged(rate.numerator, rate.denominator, 250000);
	uint64_t freshTicks = fresh.advance(frameUs);

	TickAccumulator covered = makeUnceilinged(rate.numerator, rate.denominator, 250000);
	covered.advance(frameUs); // some play before the stage is covered
	// The view is covered for a long stretch: nothing calls advance() during that time.
	// On resume(), the remainder is zeroed (SS3.4) and the next frame is fed normally.
	covered.reset();
	uint64_t resumedTicks = covered.advance(frameUs);

	if (resumedTicks != freshTicks)
	{
		LogError("test_covered_view_then_resume_is_one_ordinary_frame: resumed frame gave "
		         "{0} ticks, expected {1} (one ordinary frame, no burst)",
		         resumedTicks, freshTicks);
		return false;
	}
	return true;
}

// ticksInOneClamp is the SS3.3 suggested per-frame ceiling default; check it against
// hand-computed values for the city and battle tiers this branch actually ships.
static bool test_ticks_in_one_clamp_matches_hand_computed_values()
{
	struct Case
	{
		uint64_t ratioNumerator;
		uint64_t ratioDenominator;
		uint64_t expectedCeiling;
		const char *name;
	};
	static const Case cases[] = {
	    {1, 1, 18, "city Speed1"},   {2, 1, 36, "city Speed2"},  {4, 1, 72, "city Speed3"},
	    {6, 1, 109, "city Speed4"},  {1, 2, 9, "battle Speed1"}, {1, 1, 18, "battle Speed2"},
	    {2, 1, 36, "battle Speed3"},
	};
	bool ok = true;
	for (const auto &c : cases)
	{
		auto rate = vanillaTickRate(c.ratioNumerator, c.ratioDenominator);
		uint64_t ceiling =
		    TickAccumulator::ticksInOneClamp(rate.numerator, rate.denominator, 250000);
		if (ceiling != c.expectedCeiling)
		{
			LogError("test_ticks_in_one_clamp_matches_hand_computed_values: {0} gave "
			         "ceiling {1}, expected {2}",
			         c.name, ceiling, c.expectedCeiling);
			ok = false;
		}
	}
	return ok;
}

// hideDisplay's own fixed 960 ticks/s rate (16 ticks/frame at the old assumed 60 FPS),
// unrelated to TICKS_MULTIPLIER.
static bool test_hide_display_rate_ceiling()
{
	uint64_t ceiling = TickAccumulator::ticksInOneClamp(960, 1, 250000);
	if (ceiling != 240)
	{
		LogError("test_hide_display_rate_ceiling: got {0}, expected 240", ceiling);
		return false;
	}
	return true;
}

// SS3.5/Q9: turbo is retuned to the original's Ultra rate, 303.441874 game-seconds per
// real second (600 * TICKS_MULTIPLIER * 1193182 / 65536 = 43,695.6 ticks/s), and like
// every other tier this must hold regardless of the rendered frame rate.
static bool test_turbo_rate_matches_original()
{
	auto rate = vanillaTickRate(600); // city Speed5 (turbo)
	double actualTicksPerSecond =
	    static_cast<double>(rate.numerator) / static_cast<double>(rate.denominator);
	double expectedTicksPerSecond = 303.441874 * TICKS_PER_SECOND;
	if (!nearlyEqual(actualTicksPerSecond, expectedTicksPerSecond, 0.01))
	{
		LogError("test_turbo_rate_matches_original: turbo gave {0} ticks/s, expected {1} "
		         "(tolerance 1%)",
		         actualTicksPerSecond, expectedTicksPerSecond);
		return false;
	}
	return true;
}

static bool test_turbo_rate_fps_independent()
{
	auto rate = vanillaTickRate(600); // city Speed5 (turbo)
	const uint64_t simulatedSeconds = 20;
	const uint64_t totalUs = simulatedSeconds * 1000000ull;

	double referenceRate = -1.0;
	bool ok = true;
	for (uint64_t fps : {30ull, 50ull, 60ull, 120ull, 144ull})
	{
		uint64_t frameUs = 1000000ull / fps;
		uint64_t frames = totalUs / frameUs;
		auto accumulator = makeUnceilinged(rate.numerator, rate.denominator, 250000);
		uint64_t ticks = runFrames(accumulator, frameUs, frames);
		double simulatedUs = static_cast<double>(frames * frameUs);
		double ticksPerSecond = static_cast<double>(ticks) / (simulatedUs / 1e6);
		if (referenceRate < 0.0)
		{
			referenceRate = ticksPerSecond;
			continue;
		}
		if (!nearlyEqual(ticksPerSecond, referenceRate, 0.001))
		{
			LogError("test_turbo_rate_fps_independent: {0} FPS gave {1} ticks/s, expected "
			         "~{2}",
			         fps, ticksPerSecond, referenceRate);
			ok = false;
		}
	}
	return ok;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool allPassed = true;
	allPassed &= test_fps_invariance();
	allPassed &= test_city_tiers_match_measured_rates();
	allPassed &= test_battle_tiers_match_provisional_ratios();
	allPassed &= test_elapsed_input_clamp();
	allPassed &= test_ceiling_discards_and_resets_remainder();
	allPassed &= test_reset_zeroes_remainder();
	allPassed &= test_covered_view_then_resume_is_one_ordinary_frame();
	allPassed &= test_ticks_in_one_clamp_matches_hand_computed_values();
	allPassed &= test_hide_display_rate_ceiling();
	allPassed &= test_turbo_rate_matches_original();
	allPassed &= test_turbo_rate_fps_independent();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
