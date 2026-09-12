#pragma once

#include "game/ui/tileview/tileview.h"
#include "library/sp.h"
#include "library/vec.h"

namespace OpenApoc
{

class Image;
class GameState;
class Vehicle;

class CityTileView : public TileView
{
  private:
	std::vector<std::vector<sp<Image>>> selectionBrackets;
	sp<Image> alertImage;
	sp<Image> cargoImage;
	sp<Image> targetTacticalThisLevel;
	sp<Image> selectionImageFriendlySmall;
	sp<Image> selectionImageFriendlyLarge;
	sp<Image> selectionImageHostileSmall;
	sp<Image> selectionImageHostileLarge;

	int selectionFrameTicksAccumulated = 0;
	int portalImageTicksAccumulated = 0;

	sp<Palette> day_palette;
	sp<Palette> twilight_palette;
	sp<Palette> night_palette;

	int colorForward = 1;
	int colorCurrent = 0;
	// Cosmetic palette-pulse cadence: steps colorCurrent once every PALETTE_PULSE_DELAY()
	// update() calls instead of every call, so the pulse keeps its original ~60-step/s rate
	// (framework/uicadence.h) instead of speeding up 1:1 with the render loop's cadence.
	int paletteStepTicksAccumulated = 0;
	static int PALETTE_PULSE_DELAY() { return std::max(1, uiCosmeticFramesPerSecond() / 60); }

	std::vector<sp<Palette>> mod_day_palette;
	std::vector<sp<Palette>> mod_twilight_palette;
	std::vector<sp<Palette>> mod_night_palette;
	std::vector<sp<Palette>> mod_interpolated_palette;
	// Ingame minute when interpolated palette was calculated
	std::vector<int> interpolated_palette_minute;

  protected:
	// Cosmetic border-pulse cadence (framework/uicadence.h) - "50 frames at the original
	// 60 FPS baseline" for one full pulse cycle.
	static int COUNTER_MAX() { return std::max(2, uiCosmeticFramesPerSecond() * 50 / 60); }
	int counter = 0;

  public:
	CityTileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
	             TileViewMode initialMode, Vec3<float> screenCenterTile, GameState &gameState);
	~CityTileView() override;

	void eventOccurred(Event *e) override;
	void render() override;
	void update(const StageFrame &frame) override;

	bool DEBUG_SHOW_VEHICLE_PATH = false;
	bool DEBUG_SHOW_ROAD_PATHFINDING = false;
	bool DEBUG_SHOW_ALIEN_CREW = false;
	bool DEBUG_SHOW_ROADS = false;
	bool DEBUG_SHOW_TUBES = false;
	int DEBUG_WALK_MODE_DISPLAY = 0;
	bool DEBUG_SHOW_HILLS = false;
	bool DEBUG_STRICT_TILE_FILTER = false;
	int DEBUG_CONNECTION_FILTER = -1;
	int DEBUG_ISOLATED_LAYER = -1;
	bool DEBUG_FORCE_ALIEN_DIMENSION = false;
	bool DEBUG_SHOW_VEHICLE_TARGETS = false;

  private:
	GameState &state;
	sp<Image> selectedTileImageBack;
	sp<Image> selectedTileImageFront;
	Vec2<int> selectedTileImageOffset;
	Colour alienDetectionColour;
};
} // namespace OpenApoc
