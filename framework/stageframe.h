#pragma once

#include <cstdint>

namespace OpenApoc
{

/*
    Struct: StageFrame
    Carries the timing data for one iteration of the <Framework::run> loop.

    `elapsedRealUs` is the elapsed real (wall-clock) time, in microseconds, of one
    iteration of the `Framework::run` loop. It is measured globally by the framework,
    not per-stage: it is the delta since the loop last processed a frame, regardless
    of which stage was on top at the time, and is handed unchanged to whichever stage
    is on top now. It is *not* time-since-this-stage-was-last-updated, so a stage that
    was covered by another stage and has just been resumed receives an ordinary frame
    delta on its first post-resume frame rather than a backlog covering the time it
    was covered.
*/
struct StageFrame
{
	uint64_t elapsedRealUs;
};

// Ceiling Framework::run applies to elapsedRealUs before constructing a StageFrame, so a
// stall (load screen, alt-tab, breakpoint) cannot hand any stage a multi-second delta.
// Tick-consuming views apply the same bound again at their own accumulator (see
// framework/tickaccumulator.h), which keeps the accumulator fully self-contained and
// independently testable rather than trusting it is always fed pre-clamped input.
static constexpr uint64_t STAGE_FRAME_CLAMP_US = 250000;

}; // namespace OpenApoc
