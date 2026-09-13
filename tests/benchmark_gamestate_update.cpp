// Standalone GameState::update() cost benchmark: measures the per-wall-second cost of the
// tick-rate change (144 vs 180 TPS) so a regression can be caught before merge.
//
// This is not a pass/fail correctness test: it measures wall-clock CPU cost of simulating one
// real-time game-second's worth of ticks (TICKS_PER_SECOND calls to GameState::update(1), per-tick
// delivery to match UPDATE_EVERY_TICK's production chunking policy) and reports it, so a build at
// one tick rate can be compared against a build at another. It always exits successfully as long
// as the simulation runs without crashing; the number it prints is the actual result.
//
// Unlike tests/test_determinism_harness.cpp, this deliberately DOES call
// GameState::fillOrgStartingProperty() to populate every organisation's full vehicle fleet (~750
// vehicles), because this benchmark specifically wants a "max vehicles" scenario and does not need
// run-to-run determinism the way the trace harness does.

#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/gamestate.h"
#include <chrono>
#include <iostream>

using namespace OpenApoc;

namespace
{

ConfigOptionInt optionSeed("Benchmark", "Seed", "Fixed RNG seed for the benchmark", 424242);
ConfigOptionInt optionWarmupSeconds("Benchmark", "WarmupSeconds",
                                    "Simulated game-seconds to run before timing starts", 5);
ConfigOptionInt optionMeasureSeconds("Benchmark", "MeasureSeconds",
                                     "Simulated game-seconds to measure", 30);

} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");

	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	auto gamestateName = config().getString("gamestate");
	auto commonName = config().getString("common");
	if (gamestateName.empty() || commonName.empty())
	{
		std::cerr << "Must provide common and gamestate\n";
		config().showHelp();
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);

	auto state = mksp<GameState>();
	if (!state->loadGame(commonName))
	{
		LogError("Failed to load common gamestate \"{0}\"", commonName);
		return EXIT_FAILURE;
	}
	if (!state->loadGame(gamestateName))
	{
		LogError("Failed to load gamestate \"{0}\"", gamestateName);
		return EXIT_FAILURE;
	}

	config().set("OpenApoc.NewFeature.SeedRng", false);
	state->rng.seed(static_cast<uint64_t>(optionSeed.get()));

	state->startGame();
	state->initState();
	state->fillPlayerStartingProperty();
	// Maximal-vehicle-count scenario (see the file comment above for why determinism does not
	// matter here the way it does for the trace harness).
	state->fillOrgStartingProperty();

	const unsigned warmupTicks =
	    static_cast<unsigned>(optionWarmupSeconds.get()) * TICKS_PER_SECOND;
	const unsigned measureTicks =
	    static_cast<unsigned>(optionMeasureSeconds.get()) * TICKS_PER_SECOND;

	LogInfo("Benchmark: TICKS_PER_SECOND={0}, warming up for {1} ticks ({2} game-seconds)",
	        TICKS_PER_SECOND, warmupTicks, optionWarmupSeconds.get());
	for (unsigned i = 0; i < warmupTicks; i++)
	{
		state->update(1);
	}

	LogInfo("Benchmark: measuring {0} ticks ({1} game-seconds)", measureTicks,
	        optionMeasureSeconds.get());
	auto start = std::chrono::steady_clock::now();
	for (unsigned i = 0; i < measureTicks; i++)
	{
		state->update(1);
	}
	auto end = std::chrono::steady_clock::now();

	double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
	double gameSecondsSimulated = static_cast<double>(measureTicks) / TICKS_PER_SECOND;
	double msPerGameSecond = totalMs / gameSecondsSimulated;
	double msPerUpdateCall = totalMs / measureTicks;

	LogInfo("RESULT ticks_per_second={0} total_ms={1} game_seconds={2} ms_per_game_second={3} "
	        "ms_per_update_call={4}",
	        TICKS_PER_SECOND, totalMs, gameSecondsSimulated, msPerGameSecond, msPerUpdateCall);
	std::cout << "TICKS_PER_SECOND=" << TICKS_PER_SECOND
	          << " ms_per_game_second=" << msPerGameSecond
	          << " ms_per_update_call=" << msPerUpdateCall << std::endl;

	return EXIT_SUCCESS;
}
