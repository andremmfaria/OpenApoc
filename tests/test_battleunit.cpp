#include "framework/configfile.h"
#include "framework/logger.h"
#include "framework/sound.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battledoor.h"
#include "game/state/battle/battlescanner.h"
#include "game/state/battle/battleunit.h"
#include "game/state/city/agentmission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/shared/agent.h"

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

// Regression test for the cry-timer skipping its expiry when a single update() step covers
// more than one tick: BattleUnit::updateCrying() used to do `ticksUntillNextCry -= 1;` and test
// `== 0`, matching its per-frame origin. Converted to `-= ticks`, a step where
// `ticks > ticksUntillNextCry` can jump straight past zero into negative territory, so the
// comparison must be `<= 0` or the cry (and its timer reset) is skipped entirely, silently
// disabling enemy cries whenever the frame/tick ratio exceeds 1 (any speed above Speed1, or any
// FPS below 144).
//
// To reach the counter update without a real audio backend, the test steers around the actual
// cry: with mode TurnBased and owner != currentActiveOrganisation, a 1-in-8 roll skips playing
// the sample entirely while still exercising the decrement-and-reset path. resetCryTimer() draws
// from the same RNG first (its own randomised reset interval), so the RNG must be seeded so that
// the *second* draw - the 1-in-8 roll - lands on the skip outcome; seed 10 was found by brute
// force to satisfy that (first draw is resetCryTimer()'s, second is the cry-chance roll).
bool test_cry_timer_does_not_skip_expiry_when_ticks_exceed_remaining()
{
	GameState state;
	state.rng.seed(10);

	auto battle = mksp<Battle>();
	battle->mode = Battle::Mode::TurnBased;
	battle->currentPlayer = {&state, UString("ORG_PLAYER")};
	battle->currentActiveOrganisation = {&state, UString("ORG_ACTIVE")};
	state.current_battle = battle;

	auto agentType = mksp<AgentType>();
	agentType->crySfx.push_back(mksp<Sample>());
	state.agent_types[UString("AGENTTYPE_TEST")] = agentType;

	auto agent = mksp<Agent>();
	agent->type = {&state, UString("AGENTTYPE_TEST")};
	agent->modified_stats.health = 10;
	state.agents[UString("AGENT_TEST")] = agent;

	auto unit = mksp<BattleUnit>();
	unit->agent = {&state, UString("AGENT_TEST")};
	unit->owner = {&state, UString("ORG_ENEMY")};
	unit->ticksUntillNextCry = 5;

	unit->updateCrying(state, 10);

	bool expired = unit->ticksUntillNextCry <= 0;
	// GameState::~GameState() runs Battle::finishBattle()/exitBattle() when current_battle is
	// still set, which assumes a fully-formed battle (city, player org, etc.) that this test
	// never builds. Detach it before the stub Battle goes out of scope.
	state.current_battle = nullptr;

	if (expired)
	{
		LogError("test_cry_timer_does_not_skip_expiry_when_ticks_exceed_remaining: "
		         "ticksUntillNextCry was {0} after expiry, expected resetCryTimer() to have run "
		         "and left it positive",
		         unit->ticksUntillNextCry);
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
	allPassed &= test_cry_timer_does_not_skip_expiry_when_ticks_exceed_remaining();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
