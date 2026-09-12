#include "framework/configfile.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "framework/uicadence.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

using namespace OpenApoc;

// At the reference 60 FPS, uiCosmeticFramesPerSecond() must reproduce exactly what the
// old hardcoded-60 formulas did, e.g. tileview.h's SELECTION_FRAME_ANIMATION_DELAY = 60 / 5.
static bool test_60fps_matches_old_hardcoded_formulas()
{
	Options::targetFPS.set(60);
	bool ok = true;
	struct Case
	{
		int desiredPerSecond;
		int expectedDelay;
	};
	// Mirrors tileview.h/battletileview.h's "FPS / DESIRED_ANIMATIONS_PER_SECOND" constants.
	static const Case cases[] = {
	    {5, 12}, // SELECTION_FRAME_ANIMATION_DELAY
	    {15, 4}, // PORTAL_FRAME_ANIMATION_DELAY
	    {4, 15}, // TARGET/HEALING/PSI_ICON_ANIMATION_DELAY
	    {2, 30}, // LOWMORALE_ICON_ANIMATION_DELAY
	    {12, 5}, // TEXTEDITOR_CARET_TOGGLE_TIME (textedit.h)
	};
	for (const auto &c : cases)
	{
		int delay = std::max(1, uiCosmeticFramesPerSecond() / c.desiredPerSecond);
		if (delay != c.expectedDelay)
		{
			LogError("test_60fps_matches_old_hardcoded_formulas: 60/{0} gave {1}, expected {2}",
			         c.desiredPerSecond, delay, c.expectedDelay);
			ok = false;
		}
	}
	return ok;
}

// The acceptance criterion here is "unchanged at 60 FPS, correct at other FPS values" - a
// cosmetic cadence tuned for N animations/second must scale proportionally with the frame
// rate, not stay pinned to the old 60 FPS assumption (the defect this branch fixes).
static bool test_scales_with_target_fps()
{
	bool ok = true;
	Options::targetFPS.set(144);
	int delayAt144 = std::max(1, uiCosmeticFramesPerSecond() / 5);
	if (delayAt144 != 28) // 144 / 5, truncated - same rounding convention the old code used
	{
		LogError("test_scales_with_target_fps: 144 FPS gave delay {0}, expected 28", delayAt144);
		ok = false;
	}
	Options::targetFPS.set(30);
	int delayAt30 = std::max(1, uiCosmeticFramesPerSecond() / 5);
	if (delayAt30 != 6)
	{
		LogError("test_scales_with_target_fps: 30 FPS gave delay {0}, expected 6", delayAt30);
		ok = false;
	}
	Options::targetFPS.set(60);
	return ok;
}

// "0" is the More Options UI's "auto"/unlimited value (Options::targetFPS's own convention,
// see framework/frametiming.h) and must not reach a divide as 0; a frame-counted cosmetic
// cadence substitutes the reference 60 Hz it was tuned at instead.
static bool test_zero_target_fps_falls_back_to_60()
{
	Options::targetFPS.set(0);
	bool ok = uiCosmeticFramesPerSecond() == 60;
	if (!ok)
	{
		LogError("test_zero_target_fps_falls_back_to_60: got {0}, expected 60",
		         uiCosmeticFramesPerSecond());
	}
	Options::targetFPS.set(60);
	return ok;
}

// Real-duration UI timers (StageFrame::elapsedRealUs) express themselves as
// "US_PER_SECOND * oldFrameCount / 60" - check that derivation is exact for the values used
// across the branch (battleview.cpp's *_US constants), so a reader can trust the comment
// documenting each one's provenance.
static bool test_named_duration_constants_match_old_frame_counts_at_60fps()
{
	bool ok = true;
	struct Case
	{
		uint64_t oldFrameCountAt60Fps;
		uint64_t expectedUs;
	};
	static const Case cases[] = {
	    {5, 83333},   // THROW_RETRY_DELAY_US / ATTACK_COST_CALC_DELAY_US
	    {30, 500000}, // PATH_PREVIEW_HOVER_DELAY_US (also HIGHLIGHT_UPDATE_DELAY_US's 0.5s)
	    {40, 666666}, // ACTION_IMPOSSIBLE_CURSOR_DELAY_US
	    {10, 166666}, // HIDDEN_BAR_REFRESH_DELAY_US
	};
	for (const auto &c : cases)
	{
		uint64_t us = US_PER_SECOND * c.oldFrameCountAt60Fps / 60;
		if (us != c.expectedUs)
		{
			LogError("test_named_duration_constants_match_old_frame_counts_at_60fps: {0} frames "
			         "gave {1}us, expected {2}us",
			         c.oldFrameCountAt60Fps, us, c.expectedUs);
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
	allPassed &= test_60fps_matches_old_hardcoded_formulas();
	allPassed &= test_scales_with_target_fps();
	allPassed &= test_zero_target_fps_falls_back_to_60();
	allPassed &= test_named_duration_constants_match_old_frame_counts_at_60fps();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
