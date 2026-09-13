#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/logger.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/gamestate_serialize.h"
#include "game/state/message.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "library/strings_format.h"
#include <fstream>
#include <sstream>
#include <thread>

using namespace OpenApoc;

namespace
{

UString tempDirPath(const char *suffix)
{
	std::stringstream ss;
	ss << "openapoc_test_save_version-" << suffix << "-" << std::this_thread::get_id();
	return UString((fs::temp_directory_path() / ss.str()).string());
}

UString readTextFile(const UString &path)
{
	std::ifstream in(path, std::ios::binary);
	std::ostringstream oss;
	oss << in.rdbuf();
	return oss.str();
}

void writeTextFile(const UString &path, const UString &contents)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << contents;
}

// A freshly written save must carry an explicit, numeric save-format version, matching whatever
// GameState::deserialize() expects on read. This test inspects the raw XML rather than going
// through GameState again, so it fails loudly if the field is ever silently dropped (eg by
// accidentally routing it through the per-member diff mechanism the rest of GameState's
// serialization uses, which omits any member equal to its default value).
bool test_fresh_save_writes_numeric_version()
{
	GameState state;
	UString dir = tempDirPath("fresh");
	// pack=false writes a plain directory of XML files, so the saved data can be inspected
	// directly without going back through the archive reader.
	if (!state.saveGame(dir, false, true))
	{
		LogError("test_fresh_save_writes_numeric_version: saveGame() failed");
		return false;
	}

	UString gamestateXmlPath = (fs::path(dir) / "gamestate.xml").string();
	UString contents = readTextFile(gamestateXmlPath);
	std::error_code ec;
	fs::remove_all(dir, ec);

	UString openTag = "<save_format_version>";
	UString closeTag = "</save_format_version>";
	auto openPos = contents.find(openTag);
	auto closePos = contents.find(closeTag);
	if (openPos == UString::npos || closePos == UString::npos || closePos < openPos)
	{
		LogError("test_fresh_save_writes_numeric_version: no <save_format_version> element "
		         "found in a freshly written save");
		return false;
	}
	UString valueStr =
	    contents.substr(openPos + openTag.size(), closePos - (openPos + openTag.size()));
	unsigned int writtenVersion = 0;
	try
	{
		writtenVersion = static_cast<unsigned int>(std::stoul(valueStr));
	}
	catch (const std::exception &e)
	{
		LogError("test_fresh_save_writes_numeric_version: version value \"{0}\" is not numeric "
		         "({1})",
		         valueStr, e.what());
		return false;
	}
	if (writtenVersion != CURRENT_SAVE_FORMAT_VERSION)
	{
		LogError("test_fresh_save_writes_numeric_version: wrote version {0}, expected {1}",
		         writtenVersion, CURRENT_SAVE_FORMAT_VERSION);
		return false;
	}
	return true;
}

// A save written by this build must load in this build, version field intact.
bool test_round_trip_preserves_version()
{
	GameState state;
	UString path = tempDirPath("roundtrip");
	if (!state.saveGame(path, true, true))
	{
		LogError("test_round_trip_preserves_version: saveGame() failed");
		return false;
	}

	GameState reloaded;
	bool loaded = reloaded.loadGame(path);
	std::error_code ec;
	fs::remove_all(path, ec);
	if (!loaded)
	{
		LogError("test_round_trip_preserves_version: loadGame() failed on a save this build "
		         "just wrote");
		return false;
	}
	return true;
}

// Saves that predate this change carry no <save_format_version> element at all. Loading one must
// still succeed (defaulting to version 0), not fail or throw - this is the whole point of the
// migration hook: version 0 to CURRENT_SAVE_FORMAT_VERSION must be a safe no-op today.
bool test_unstamped_save_still_loads()
{
	GameState state;
	UString dir = tempDirPath("unstamped");
	if (!state.saveGame(dir, false, true))
	{
		LogError("test_unstamped_save_still_loads: saveGame() failed");
		return false;
	}

	UString gamestateXmlPath = (fs::path(dir) / "gamestate.xml").string();
	UString contents = readTextFile(gamestateXmlPath);

	UString openTag = "<save_format_version>";
	UString closeTag = "</save_format_version>";
	auto openPos = contents.find(openTag);
	auto closePos = contents.find(closeTag);
	if (openPos == UString::npos || closePos == UString::npos || closePos < openPos)
	{
		LogError("test_unstamped_save_still_loads: could not find <save_format_version> element "
		         "to strip");
		std::error_code ec;
		fs::remove_all(dir, ec);
		return false;
	}
	// Simulate a pre-existing save by removing the whole element, the way every save written
	// before this change looks on disk.
	contents.erase(openPos, closePos + closeTag.size() - openPos);
	writeTextFile(gamestateXmlPath, contents);

	GameState reloaded;
	bool loaded = reloaded.loadGame(dir);
	std::error_code ec;
	fs::remove_all(dir, ec);
	if (!loaded)
	{
		LogError("test_unstamped_save_still_loads: loadGame() failed on a save with no "
		         "save_format_version element");
		return false;
	}
	return true;
}

// A save written before the 180 TPS tick-rate change (save_format_version 1, the version the
// previous branch shipped) must load at 180 TPS with every tick-denominated field rescaled by
// 5/4, per GameState::migrateSaveFormat()'s full field inventory (see its comment in
// gamestate.cpp) - existing player saves must keep working, not be rejected. This covers a
// representative field from each of the inventory's reachable-without-a-full-ruleset categories:
// a GameState-level absolute tick count (gameTime), a GameState-level absolute timestamp
// (nextInvasion), an EventMessage's embedded GameTime, and one live accumulator each on a
// default-constructed Agent and Vehicle (both are default-constructible with no StateRef
// dependencies, so this does not need a full loaded ruleset the way an in-battle or in-mission
// fixture would).
bool test_v1_save_migrates_tick_rate()
{
	GameState state;
	state.gameTime = GameTime(400);
	state.nextInvasion = 100;
	state.messages.push_back(EventMessage(GameTime(200), "test message"));

	auto agent = mksp<Agent>();
	agent->trainingPhysicalTicksAccumulated = 40;
	state.agents["AGENT_TEST_MIGRATION"] = agent;

	auto vehicle = mksp<Vehicle>();
	vehicle->ticksToTurn = 20;
	state.vehicles["VEHICLE_TEST_MIGRATION"] = vehicle;

	UString dir = tempDirPath("migration");
	if (!state.saveGame(dir, false, true))
	{
		LogError("test_v1_save_migrates_tick_rate: saveGame() failed");
		return false;
	}

	// Simulate a save written by the previous branch (save_format_version 1, still 144 TPS) by
	// rewriting the version this build just wrote down to 1.
	UString gamestateXmlPath = (fs::path(dir) / "gamestate.xml").string();
	UString contents = readTextFile(gamestateXmlPath);
	UString tagOpen = "<save_format_version>";
	UString tagClose = "</save_format_version>";
	UString from = tagOpen + format("{0}", CURRENT_SAVE_FORMAT_VERSION) + tagClose;
	UString to = tagOpen + "1" + tagClose;
	auto pos = contents.find(from);
	if (pos == UString::npos)
	{
		LogError("test_v1_save_migrates_tick_rate: could not find \"{0}\" to rewrite", from);
		std::error_code ec;
		fs::remove_all(dir, ec);
		return false;
	}
	contents.replace(pos, from.size(), to);
	writeTextFile(gamestateXmlPath, contents);

	GameState reloaded;
	bool loaded = reloaded.loadGame(dir);
	std::error_code ec;
	fs::remove_all(dir, ec);
	if (!loaded)
	{
		LogError("test_v1_save_migrates_tick_rate: loadGame() failed on a v1 save, expected it "
		         "to migrate and load successfully");
		return false;
	}

	bool ok = true;
	auto check = [&ok](const UString &what, uint64_t actual, uint64_t expected)
	{
		if (actual != expected)
		{
			LogError("test_v1_save_migrates_tick_rate: {0} = {1}, expected {2}", what, actual,
			         expected);
			ok = false;
		}
	};
	// 400 * 5 / 4 = 500
	check("gameTime.ticks", reloaded.gameTime.getTicks(), 500);
	// 100 * 5 / 4 = 125
	check("nextInvasion", reloaded.nextInvasion, 125);
	if (reloaded.messages.empty())
	{
		LogError("test_v1_save_migrates_tick_rate: messages list is empty after reload");
		ok = false;
	}
	else
	{
		// 200 * 5 / 4 = 250
		check("messages.front().time.ticks", reloaded.messages.front().time.getTicks(), 250);
	}
	auto reloadedAgent = reloaded.agents["AGENT_TEST_MIGRATION"];
	// 40 * 5 / 4 = 50
	check("agent trainingPhysicalTicksAccumulated", reloadedAgent->trainingPhysicalTicksAccumulated,
	      50);
	auto reloadedVehicle = reloaded.vehicles["VEHICLE_TEST_MIGRATION"];
	// 20 * 5 / 4 = 25
	check("vehicle ticksToTurn", reloadedVehicle->ticksToTurn, 25);
	return ok;
}

// Gamestate content baked before the 180 TPS tick-rate change (dataVersion < 2, see
// CURRENT_BAKED_DATA_VERSION's comment in gamestate.h) carries fire_delay/projectile_delay/
// stunTicks values pre-multiplied by the old TICKS_MULTIPLIER==4. GameState::deserialize() must
// rescale them by 5/4 on load and bump dataVersion so a later re-save/re-load does not rescale
// again.
bool test_stale_data_version_rescales_baked_ticks()
{
	GameState state;
	state.dataVersion = 1;
	auto agentWeapon = mksp<AEquipmentType>();
	agentWeapon->fire_delay = 100;
	agentWeapon->projectile_delay = 64;
	state.agent_equipment["AEQUIPMENTTYPE_TEST_RESCALE"] = agentWeapon;
	auto vehicleWeapon = mksp<VEquipmentType>();
	vehicleWeapon->fire_delay = 40;
	vehicleWeapon->stunTicks = 288;
	state.vehicle_equipment["VEQUIPMENTTYPE_TEST_RESCALE"] = vehicleWeapon;

	UString dir = tempDirPath("staledata");
	if (!state.saveGame(dir, false, true))
	{
		LogError("test_stale_data_version_rescales_baked_ticks: saveGame() failed");
		return false;
	}

	GameState reloaded;
	bool loaded = reloaded.loadGame(dir);
	std::error_code ec;
	fs::remove_all(dir, ec);
	if (!loaded)
	{
		LogError("test_stale_data_version_rescales_baked_ticks: loadGame() failed");
		return false;
	}

	bool ok = true;
	// Rescale is round((value * 5 + 2) / 4), matching the rounding rescaleBakedTickData() uses.
	auto check = [&ok](const UString &what, int actual, int expected)
	{
		if (actual != expected)
		{
			LogError("test_stale_data_version_rescales_baked_ticks: {0} = {1}, expected {2}", what,
			         actual, expected);
			ok = false;
		}
	};
	auto reloadedAgentWeapon = reloaded.agent_equipment["AEQUIPMENTTYPE_TEST_RESCALE"];
	check("agent fire_delay", reloadedAgentWeapon->fire_delay, 125);
	check("agent projectile_delay", reloadedAgentWeapon->projectile_delay, 80);
	auto reloadedVehicleWeapon = reloaded.vehicle_equipment["VEQUIPMENTTYPE_TEST_RESCALE"];
	check("vehicle fire_delay", reloadedVehicleWeapon->fire_delay, 50);
	check("vehicle stunTicks", reloadedVehicleWeapon->stunTicks, 360);
	if (reloaded.dataVersion != CURRENT_BAKED_DATA_VERSION)
	{
		LogError("test_stale_data_version_rescales_baked_ticks: dataVersion = {0} after rescale, "
		         "expected CURRENT_BAKED_DATA_VERSION {1}",
		         reloaded.dataVersion, CURRENT_BAKED_DATA_VERSION);
		ok = false;
	}
	return ok;
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool allPassed = true;
	allPassed &= test_fresh_save_writes_numeric_version();
	allPassed &= test_round_trip_preserves_version();
	allPassed &= test_unstamped_save_still_loads();
	allPassed &= test_v1_save_migrates_tick_rate();
	allPassed &= test_stale_data_version_rescales_baked_ticks();

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
