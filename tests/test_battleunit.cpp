#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battleunit.h"
#include "game/state/gamestate.h"

using namespace OpenApoc;

namespace
{

// Regression test for the residual-aiming-timer underflow: when
// 0 < residual_aiming_ticks_remaining < ticks, BattleUnit::updateAttacking() used to compute
// `residual_aiming_ticks_remaining -= ticks` directly on the unsigned counter. That wraps to a
// huge value instead of reaching zero, so the unit's weapon never lowers (canHandStateChange()
// is never reached because the counter never reads as expired). This is reported to happen at
// Speed3 and during hideDisplay, both of which can feed a single large `ticks` step.
bool test_residual_aiming_does_not_underflow_when_ticks_exceed_remaining()
{
	GameState state;

	auto unit = mksp<BattleUnit>();
	// Default weaponStatus is NotFiring, so isAttacking() is false and updateAttacking() takes
	// the "not attacking" branch that only touches the residual aiming timer - no battle,
	// agent or equipment setup is needed to exercise this path.
	unit->residual_aiming_ticks_remaining = 5;

	unit->updateAttacking(state, 10);

	if (unit->residual_aiming_ticks_remaining != 0)
	{
		LogError("test_residual_aiming_does_not_underflow_when_ticks_exceed_remaining: "
		         "remaining was {0} after a bigger tick step, expected 0 (clamped, not wrapped)",
		         unit->residual_aiming_ticks_remaining);
		return false;
	}
	return true;
}

// Companion case: when ticks is strictly less than the remaining timer, the normal
// subtraction path must still behave as before (no off-by-one from the new clamp).
bool test_residual_aiming_decrements_normally_when_ticks_below_remaining()
{
	GameState state;

	auto unit = mksp<BattleUnit>();
	unit->residual_aiming_ticks_remaining = 10;

	unit->updateAttacking(state, 4);

	if (unit->residual_aiming_ticks_remaining != 6)
	{
		LogError("test_residual_aiming_decrements_normally_when_ticks_below_remaining: "
		         "remaining was {0}, expected 6",
		         unit->residual_aiming_ticks_remaining);
		return false;
	}
	return true;
}

} // anonymous namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool allPassed = true;
	allPassed &= test_residual_aiming_does_not_underflow_when_ticks_exceed_remaining();
	allPassed &= test_residual_aiming_decrements_normally_when_ticks_below_remaining();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
