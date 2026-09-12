#include "framework/tickaccumulator.h"

#include "framework/logger.h"

namespace OpenApoc
{

TickAccumulator::TickAccumulator(uint64_t rateNumerator, uint64_t rateDenominator,
                                 uint64_t elapsedClampUs, uint64_t maxTicksPerAdvance)
    : rateNumerator(rateNumerator), rateDenominator(rateDenominator),
      elapsedClampUs(elapsedClampUs), maxTicksPerAdvance(maxTicksPerAdvance)
{
}

void TickAccumulator::reset() { microticks = 0; }

void TickAccumulator::setRate(uint64_t newRateNumerator, uint64_t newRateDenominator)
{
	rateNumerator = newRateNumerator;
	rateDenominator = newRateDenominator;
}

void TickAccumulator::setMaxTicksPerAdvance(uint64_t newMaxTicksPerAdvance)
{
	maxTicksPerAdvance = newMaxTicksPerAdvance;
}

uint64_t TickAccumulator::ticksInOneClamp(uint64_t rateNumerator, uint64_t rateDenominator,
                                          uint64_t elapsedClampUs)
{
	if (rateDenominator == 0)
	{
		return 0;
	}
	return (elapsedClampUs * rateNumerator / rateDenominator) / MICROTICKS_PER_TICK;
}

uint64_t TickAccumulator::advance(uint64_t elapsedRealUs)
{
	if (elapsedRealUs > elapsedClampUs)
	{
		elapsedRealUs = elapsedClampUs;
	}
	if (rateDenominator == 0)
	{
		// No valid rate configured - accrue nothing rather than divide by zero.
		return 0;
	}

	microticks += elapsedRealUs * rateNumerator / rateDenominator;
	uint64_t ticks = microticks / MICROTICKS_PER_TICK;
	microticks -= ticks * MICROTICKS_PER_TICK;

	if (ticks > maxTicksPerAdvance)
	{
		LogDebug("TickAccumulator: {0} ticks exceeded the {1}-tick per-frame ceiling; "
		         "discarding the excess and resetting the fractional remainder",
		         ticks, maxTicksPerAdvance);
		ticks = maxTicksPerAdvance;
		microticks = 0;
	}
	return ticks;
}

} // namespace OpenApoc
