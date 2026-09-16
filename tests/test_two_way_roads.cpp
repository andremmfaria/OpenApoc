#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "game/state/city/city.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/tilemap/tile.h"
#include "game/state/tilemap/tilemap.h"
#include <iostream>

using namespace OpenApoc;

namespace
{

UString commonName;
UString gamestateName;
int failures = 0;

bool check(bool condition, const UString &what)
{
	if (condition)
	{
		LogWarning("PASS: {0}", what);
	}
	else
	{
		LogWarning("FAIL: {0}", what);
		failures++;
	}
	return condition;
}

// Each case places vehicles and runs the clock, so every case gets its own state rather than
// inheriting the previous case's traffic and game time.
sp<GameState> loadState()
{
	auto state = mksp<GameState>();
	if (!state->loadGame(commonName) || !state->loadGame(gamestateName))
	{
		LogError("Failed to load gamestate");
		return nullptr;
	}
	state->startGame();
	state->initState();
	state->fillPlayerStartingProperty();
	return state;
}

struct StraightRun
{
	std::vector<Vec3<int>> tiles;
	Vec3<int> dir;
};

// Uses the game's own passability helper rather than reimplementing road connectivity, so the
// run found here is one real pathfinding would also accept.
bool findStraightRoadRun(GameState &state, int minLength, StraightRun &out)
{
	auto &map = *state.current_city->map;
	GroundVehicleTileHelper helper(map, VehicleType::Type::Road);
	std::vector<Vec3<int>> axes = {{1, 0, 0}, {0, 1, 0}};

	for (int z = 0; z < map.size.z; z++)
	{
		for (int y = 0; y < map.size.y; y++)
		{
			for (int x = 0; x < map.size.x; x++)
			{
				Vec3<int> start = {x, y, z};
				auto startTile = map.getTile(start);
				if (!startTile->presentScenery ||
				    startTile->presentScenery->type->tile_type != SceneryTileType::TileType::Road)
				{
					continue;
				}
				for (const auto &dir : axes)
				{
					std::vector<Vec3<int>> run = {start};
					Vec3<int> cur = start;
					while ((int)run.size() < minLength)
					{
						Vec3<int> next = cur + dir;
						if (!map.tileIsValid(next))
						{
							break;
						}
						if (!helper.canEnterTile(map.getTile(cur), map.getTile(next)))
						{
							break;
						}
						run.push_back(next);
						cur = next;
					}
					if ((int)run.size() >= minLength)
					{
						out.tiles = run;
						out.dir = dir;
						return true;
					}
				}
			}
		}
	}
	return false;
}

float lateralOffset(const Vec3<float> &position, const Vec3<int> &tile, const Vec3<int> &dir)
{
	if (dir.x != 0)
	{
		return position.y - (tile.y + 0.5f);
	}
	return position.x - (tile.x + 0.5f);
}

// Two vehicles driving in opposite directions down the same straight road must not share a
// centre line.
void testLaneSeparation()
{
	auto state = loadState();
	if (!check(state != nullptr, "lane separation: gamestate loaded"))
	{
		return;
	}

	StraightRun run;
	if (!check(findStraightRoadRun(*state, 6, run), "lane separation: found a straight road run"))
	{
		return;
	}

	StateRef<VehicleType> vType{state.get(), UString("VEHICLETYPE_AUTOTRANS")};
	if (!check((bool)vType, "lane separation: VEHICLETYPE_AUTOTRANS exists"))
	{
		return;
	}

	auto &map = *state->current_city->map;
	Vec3<int> first = run.tiles.front();
	Vec3<int> last = run.tiles.back();

	auto forward = state->current_city->placeVehicle(*state, vType, state->getPlayer(),
	                                                 map.getTile(first)->getRestingPosition());
	auto backward = state->current_city->placeVehicle(*state, vType, state->getPlayer(),
	                                                  map.getTile(last)->getRestingPosition());
	if (!check(forward && backward, "lane separation: test vehicles placed"))
	{
		return;
	}

	forward->setMission(*state, VehicleMission::gotoLocation(*state, *forward, last));
	backward->setMission(*state, VehicleMission::gotoLocation(*state, *backward, first));

	Vec3<int> midTile = run.tiles[run.tiles.size() / 2];
	bool sawForward = false;
	bool sawBackward = false;
	float forwardLateral = 0.0f;
	float backwardLateral = 0.0f;

	for (int i = 0; i < 400 && !(sawForward && sawBackward); i++)
	{
		state->update(TICKS_PER_SECOND / 4);

		if (!sawForward && (Vec3<int>)forward->position == midTile)
		{
			sawForward = true;
			forwardLateral = lateralOffset(forward->position, midTile, run.dir);
		}
		if (!sawBackward && (Vec3<int>)backward->position == midTile)
		{
			sawBackward = true;
			backwardLateral = lateralOffset(backward->position, midTile, run.dir);
		}
	}

	if (!check(sawForward && sawBackward, "lane separation: both vehicles crossed the mid tile"))
	{
		return;
	}

	LogWarning("lane separation: forward lateral = {0}, backward lateral = {1}", forwardLateral,
	           backwardLateral);

	check(std::abs(forwardLateral) > 0.1f && std::abs(backwardLateral) > 0.1f,
	      "lane separation: both vehicles are off the tile centre line");
	check((forwardLateral > 0.0f) != (backwardLateral > 0.0f),
	      "lane separation: opposing vehicles use opposite lanes");
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

	gamestateName = config().getString("gamestate");
	commonName = config().getString("common");
	if (gamestateName.empty() || commonName.empty())
	{
		std::cerr << "Must provide common and gamestate\n";
		config().showHelp();
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);

	testLaneSeparation();

	if (failures > 0)
	{
		LogWarning("test_two_way_roads: {0} failure(s)", failures);
		return failures;
	}
	LogWarning("test_two_way_roads success - all cases passed");
	return EXIT_SUCCESS;
}
