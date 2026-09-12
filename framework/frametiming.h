#pragma once

#include <cstdint>

namespace OpenApoc
{

/*
    Converts a validated Options::targetFPS value into the frame pacer's target duration
    between frames.

    Pure and Framework-independent so it can be unit-tested directly, which is what makes the
    "a value of 0 never reaches the divide" and "0 means unlimited" requirements verifiable
    without a live window (see plans/997-implementation-plan.md SS11.2/SS11.4).

    Precondition: targetFPS >= 0. A negative value is a config-validation concern for the
    caller (Framework::run falls back to 60 and logs a warning), not something this function
    interprets.
*/
struct FrameDuration
{
	// True when targetFPS was 0 ("auto" in the More Options UI, matching the "0 = unlimited"
	// convention already used by Options::frameLimit). durationUs is unspecified when true;
	// callers must branch on this flag rather than treating a 0 duration as "sleep 0us every
	// frame", which would busy-loop instead of removing the cap.
	bool unlimited;
	uint64_t durationUs;
};

FrameDuration frameDurationForTargetFPS(int targetFPS);

} // namespace OpenApoc
