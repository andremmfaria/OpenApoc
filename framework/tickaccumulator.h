#pragma once

#include <cstdint>

namespace OpenApoc
{

/*
    Class: TickAccumulator
    Converts elapsed real (wall-clock) time into whole game ticks.

    Pure and view-independent: it knows nothing about Framework, Stage, or GameState,
    and takes no dependency on any of them. It is driven purely by `advance(elapsedRealUs)`
    calls, which is what makes it unit-testable with synthetic elapsedRealUs sequences
    (arbitrary FPS cadences, throttled delivery, stall spikes) without a live Framework.

    Representation: a 64-bit unsigned integer of "microticks", where one game tick equals
    1,000,000 microticks. Every operation is integer arithmetic; nothing here uses float
    or double. This matters because it guarantees that identical elapsedRealUs sequences
    produce bit-identical tick sequences on every platform, which floating point
    accumulation does not: see plans/997-implementation-plan.md SS3.1 and SS8.

    Two independent safety nets, both expressed in this class so they are covered by the
    same unit tests:
      - `elapsedClampUs` bounds a single advance() call's *input*. It mirrors the clamp
        Framework itself applies to elapsedRealUs before constructing a StageFrame,
        kept here too so the accumulator is fully self-contained: it does not rely on
        being fed already-clamped input to behave correctly under a stall.
      - `maxTicksPerAdvance` bounds a single advance() call's *output*. Ticks beyond the
        ceiling are discarded, not carried, and the fractional remainder is zeroed: see
        SS3.3 for why carrying a backlog forward is wrong (it turns one stall into a
        sustained catch-up that keeps re-tripping the ceiling).
*/
class TickAccumulator
{
  public:
	static constexpr uint64_t MICROTICKS_PER_TICK = 1000000ull;

	// rateNumerator / rateDenominator is the exact ticks-per-real-second rate, e.g.
	// speed_value * TICKS_MULTIPLIER * 1193182 / 65536 (SS3.1/SS3.2). A rate of 0/x
	// (Pause) accrues nothing.
	TickAccumulator(uint64_t rateNumerator, uint64_t rateDenominator, uint64_t elapsedClampUs,
	                uint64_t maxTicksPerAdvance);

	// Feeds elapsedRealUs of real time; returns the whole ticks now due. The sub-tick
	// remainder carries into the next call (unless the ceiling trips, see above).
	uint64_t advance(uint64_t elapsedRealUs);

	// Zeroes the fractional remainder without producing ticks. Call this on Pause and on
	// Stage::resume() (SS3.4): it stops a stale fraction accrued at a previous speed from
	// surfacing as an unexpected tick once play resumes. A stage that is merely covered by
	// another stage needs no such call: Framework only updates the top stage, so a covered
	// view already accrues nothing and its remainder is simply frozen.
	void reset();

	void setRate(uint64_t rateNumerator, uint64_t rateDenominator);
	void setMaxTicksPerAdvance(uint64_t maxTicksPerAdvance);

	// Ticks produced by one elapsedClampUs-wide burst at the given rate: the SS3.3
	// suggested per-frame ceiling default ("the ticks one clamp-width of real time
	// produces at the current speed"). Exposed statically so callers can compute a
	// default ceiling when constructing an accumulator, and so tests can check the
	// derivation directly.
	static uint64_t ticksInOneClamp(uint64_t rateNumerator, uint64_t rateDenominator,
	                                uint64_t elapsedClampUs);

  private:
	uint64_t rateNumerator;
	uint64_t rateDenominator;
	uint64_t elapsedClampUs;
	uint64_t maxTicksPerAdvance;
	uint64_t microticks = 0;
};

} // namespace OpenApoc
