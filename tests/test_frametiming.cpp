#include "framework/configfile.h"
#include "framework/frametiming.h"
#include "framework/logger.h"

#include <cstdint>
#include <cstdlib>

using namespace OpenApoc;

// SS11.3/SS11.4: 0 ("auto" in the More Options UI) must mean unlimited, and must never reach
// the divide that used to compute frame duration directly from targetFPS.
static bool test_zero_is_unlimited()
{
	FrameDuration d = frameDurationForTargetFPS(0);
	if (!d.unlimited)
	{
		LogError("test_zero_is_unlimited: targetFPS=0 did not report unlimited");
		return false;
	}
	return true;
}

// SS11.4: the presets plus a couple of representative typed values produce the exact
// microsecond duration a 1000000/targetFPS pacer expects.
static bool test_presets_match_expected_duration()
{
	struct Case
	{
		int targetFPS;
		uint64_t expectedUs;
	};
	static const Case cases[] = {
	    {30, 33333}, {60, 16666}, {75, 13333}, {144, 6944}, {240, 4166}, {1, 1000000},
	};
	bool ok = true;
	for (const auto &c : cases)
	{
		FrameDuration d = frameDurationForTargetFPS(c.targetFPS);
		if (d.unlimited)
		{
			LogError("test_presets_match_expected_duration: targetFPS={0} reported unlimited",
			         c.targetFPS);
			ok = false;
			continue;
		}
		if (d.durationUs != c.expectedUs)
		{
			LogError("test_presets_match_expected_duration: targetFPS={0} gave {1}us, expected "
			         "{2}us",
			         c.targetFPS, d.durationUs, c.expectedUs);
			ok = false;
		}
	}
	return ok;
}

// A negative targetFPS is a config-validation concern for the caller (Framework::run falls
// back to 60 and logs a warning), but this function must still be total and never divide by a
// non-positive value even if that validation is skipped.
static bool test_negative_is_treated_as_unlimited_not_a_crash()
{
	FrameDuration d = frameDurationForTargetFPS(-1);
	if (!d.unlimited)
	{
		LogError("test_negative_is_treated_as_unlimited_not_a_crash: targetFPS=-1 did not "
		         "report unlimited");
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
	allPassed &= test_zero_is_unlimited();
	allPassed &= test_presets_match_expected_duration();
	allPassed &= test_negative_is_treated_as_unlimited_not_a_crash();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
