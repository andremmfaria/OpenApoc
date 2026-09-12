// Headless GameState-level determinism/trace harness (issue #997).
//
// Framework::run refuses to run without a window, and GameState seeds its RNG from
// std::time(nullptr) whenever OpenApoc.NewFeature.SeedRng is set (the default), so neither can
// be used as-is to produce a repeatable trace. This harness drives GameState directly, the way
// test_serialize.cpp and test_lab_assignment.cpp already do, and injects a fixed seed through
// the existing SeedRng option and the already-public GameState::rng member, so no engine code
// changes were needed to make a run repeatable.
//
// It then proves that repeatability concretely: run the same scripted simulation twice from
// identical inputs and compare the two checkpoint traces line-for-line.

#include "determinism_trace.h"
#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/gamestate.h"
#include <iostream>

using namespace OpenApoc;

namespace
{

ConfigOptionInt optionSeed("Harness", "Seed", "Fixed RNG seed for the determinism harness", 424242);
ConfigOptionInt
    optionOtherSeed("Harness", "SeedForDivergenceCheck",
                    "A second seed used only to prove the harness is actually seed-sensitive",
                    424243);
ConfigOptionInt optionTicks("Harness", "Ticks", "Total game ticks to simulate per run", 720);
ConfigOptionInt optionCheckpointInterval("Harness", "CheckpointInterval",
                                         "Game ticks between checkpoints", 144);
ConfigOptionString optionTraceOut(
    "Harness", "TraceOut",
    "Optional path to also write the first run's trace to, for baseline comparisons by other "
    "tools",
    "");

// Loads and starts a fresh GameState the same way test_serialize.cpp / test_lab_assignment.cpp
// do, then forces a fixed RNG seed before any gameplay-affecting code can draw from it.
//
// Least-invasive seed injection: OpenApoc.NewFeature.SeedRng already exists purely to gate
// GameState::startGame()'s time(nullptr) reseed (gamestate.cpp), and GameState::rng is already
// a public member with a public seed() method. Turning the option off and calling rng.seed()
// ourselves before startGame() reuses both existing entry points exactly as designed, with zero
// new conditional logic anywhere in GameState and no change to what a normal (non-harness) run
// does, since the option's default stays true.
sp<GameState> loadAndSeed(const UString &commonPath, const UString &gamestatePath, uint64_t seed)
{
	auto state = mksp<GameState>();
	if (!state->loadGame(commonPath))
	{
		LogError("Failed to load common gamestate \"{0}\"", commonPath);
		return nullptr;
	}
	if (!state->loadGame(gamestatePath))
	{
		LogError("Failed to load gamestate \"{0}\"", gamestatePath);
		return nullptr;
	}

	config().set("OpenApoc.NewFeature.SeedRng", false);
	state->rng.seed(seed);

	state->startGame();
	state->initState();
	state->fillPlayerStartingProperty();
	state->fillOrgStartingProperty();

	return state;
}

// Drives GameState::update() one tick at a time (chunk=1, matching UPDATE_EVERY_TICK's forced
// per-tick delivery in game/state/gamestate.h) for `totalTicks`, capturing a checkpoint every
// `checkpointInterval` ticks plus one at tick 0 and one at the final tick. Per-tick delivery is
// what the plan requires for exact-trace comparability: any larger chunk size can reorder float
// accumulation and collision segment lengths, which is a real behavior difference, not a
// harness artefact.
TraceLines runSimulation(const UString &commonPath, const UString &gamestatePath, uint64_t seed,
                         unsigned totalTicks, unsigned checkpointInterval)
{
	TraceLines trace;

	auto state = loadAndSeed(commonPath, gamestatePath, seed);
	if (!state)
	{
		return trace;
	}

	captureCheckpoint(*state, 0, trace);

	bool finalTickCaptured = false;
	for (unsigned tick = 1; tick <= totalTicks; tick++)
	{
		state->update(1);
		if (checkpointInterval > 0 && tick % checkpointInterval == 0)
		{
			captureCheckpoint(*state, tick, trace);
			finalTickCaptured = (tick == totalTicks);
		}
	}

	if (totalTicks > 0 && !finalTickCaptured)
	{
		captureCheckpoint(*state, totalTicks, trace);
	}

	return trace;
}

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

	const uint64_t seed = static_cast<uint64_t>(optionSeed.get());
	const uint64_t otherSeed = static_cast<uint64_t>(optionOtherSeed.get());
	const unsigned totalTicks = static_cast<unsigned>(optionTicks.get());
	const unsigned checkpointInterval = static_cast<unsigned>(optionCheckpointInterval.get());

	LogInfo("Running determinism harness: seed={0} ticks={1} checkpointInterval={2}", seed,
	        totalTicks, checkpointInterval);

	LogInfo("Run A (seed={0})", seed);
	auto traceA = runSimulation(commonName, gamestateName, seed, totalTicks, checkpointInterval);
	if (traceA.empty())
	{
		LogError("Run A produced no trace (failed to load gamestate?)");
		return EXIT_FAILURE;
	}

	LogInfo("Run B (seed={0}, independent GameState instance)", seed);
	auto traceB = runSimulation(commonName, gamestateName, seed, totalTicks, checkpointInterval);
	if (traceB.empty())
	{
		LogError("Run B produced no trace (failed to load gamestate?)");
		return EXIT_FAILURE;
	}

	UString diff;
	if (!compareTraces(traceA, traceB, diff))
	{
		LogError("Determinism check FAILED: traces for two identically-seeded runs diverged: {0}",
		         diff);
		return EXIT_FAILURE;
	}
	LogInfo("Determinism check passed: {0} trace lines byte-identical across two runs",
	        traceA.size());

	// Sanity check on the harness itself: a different seed must produce a different trace,
	// otherwise the seed injection could be silently inert and the identical-trace check above
	// would be vacuously true.
	LogInfo("Run C (seed={0}, divergence check)", otherSeed);
	auto traceC =
	    runSimulation(commonName, gamestateName, otherSeed, totalTicks, checkpointInterval);
	if (traceC.empty())
	{
		LogError("Run C produced no trace (failed to load gamestate?)");
		return EXIT_FAILURE;
	}
	UString sameDiff;
	if (compareTraces(traceA, traceC, sameDiff))
	{
		LogError("Divergence check FAILED: seed={0} and seed={1} produced identical traces; the "
		         "seed injection appears to have no effect",
		         seed, otherSeed);
		return EXIT_FAILURE;
	}
	LogInfo("Divergence check passed: different seeds produce different traces");

	auto traceOutPath = optionTraceOut.get();
	if (!traceOutPath.empty())
	{
		if (!writeTraceFile(traceOutPath, traceA))
		{
			LogError("Failed to write trace to \"{0}\"", traceOutPath);
			return EXIT_FAILURE;
		}
		LogInfo("Wrote reference trace to \"{0}\"", traceOutPath);
	}

	LogInfo("test_determinism_harness success");
	return EXIT_SUCCESS;
}
