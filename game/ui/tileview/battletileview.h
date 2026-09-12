#pragma once

#include "framework/uicadence.h"
#include "game/state/battle/battleunit.h"
#include "game/ui/tileview/tileview.h"
#include "library/enum_traits.h"
#include "library/sp.h"
#include "library/vec.h"
#include <algorithm>
#include <cstdint>
#include <list>
#include <vector>

namespace OpenApoc
{

class TileObjectBattleUnit;
class Battle;
class Form;
class Image;

class BattleTileView : public TileView
{
  protected:
	// Cosmetic, frame-counted icon-rotation cadences (framework/uicadence.h) - not real
	// durations. Formula: uiCosmeticFramesPerSecond() / DESIRED_ANIMATIONS_PER_SECOND.

	static int TARGET_ICONS_ANIMATION_DELAY()
	{
		return std::max(1, uiCosmeticFramesPerSecond() / 4);
	}
	static int HEALING_ICON_ANIMATION_DELAY()
	{
		return std::max(1, uiCosmeticFramesPerSecond() / 4);
	}
	static int PSI_ICON_ANIMATION_DELAY() { return std::max(1, uiCosmeticFramesPerSecond() / 4); }
	static int LOWMORALE_ICON_ANIMATION_DELAY()
	{
		return std::max(1, uiCosmeticFramesPerSecond() / 2);
	}

	// Total amount of different focus icon states
	static const int FOCUS_ICONS_ANIMATION_FRAMES = 4;

	// Formula: FPS / FOCUS_ICONS_ANIMATION_FRAMES(both ways) / DESIRED_ANIMATIONS_PER_SECOND
	static int FOCUS_ICONS_ANIMATION_DELAY()
	{
		return std::max(1,
		                uiCosmeticFramesPerSecond() / (2 * FOCUS_ICONS_ANIMATION_FRAMES - 2) / 2);
	}

  public:
	enum class LayerDrawingMode
	{
		UpToCurrentLevel,
		AllLevels,
		OnlyCurrentLevel
	};

  protected:
	sp<Form> hiddenForm;

  private:
	LayerDrawingMode layerDrawingMode;
	GameState &state;
	Battle &battle;

	sp<Image> selectedTileEmptyImageBack;
	sp<Image> selectedTileEmptyImageFront;
	sp<Image> selectedTileFilledImageBack;
	sp<Image> selectedTileFilledImageFront;
	sp<Image> selectedTileFireImageBack;
	sp<Image> selectedTileFireImageFront;
	sp<Image> selectedTileBackgroundImageBack;
	sp<Image> selectedTileBackgroundImageFront;
	Vec2<int> selectedTileImageOffset;

	std::vector<sp<Image>> activeUnitSelectionArrow;
	std::vector<sp<Image>> inactiveUnitSelectionArrow;
	std::vector<sp<Image>> arrowHealthBars;
	std::map<BattleUnit::BehaviorMode, sp<Image>> behaviorUnitSelectionUnderlay;
	sp<Image> runningIcon;
	sp<Image> bleedingIcon;
	std::vector<sp<Image>> healingIcons;
	std::vector<sp<Image>> lowMoraleIcons;
	std::map<PsiStatus, std::vector<sp<Image>>> psiIcons;
	std::vector<sp<Image>> targetLocationIcons;
	Vec2<float> targetLocationOffset;
	std::vector<sp<Image>> tuIndicators;
	sp<Image> tuSeparator; // forward slash
	// Must have same amount it items as in targetLocationIcons
	std::vector<sp<Image>> waypointIcons;
	std::vector<sp<Image>> waypointDarkIcons;
	sp<Image> targetTacticalThisLevel;
	sp<Image> targetTacticalOtherLevel;
	sp<Image> selectionImageFriendlySmall;
	sp<Image> selectionImageFriendlyLarge;
	int iconAnimationTicksAccumulated = 0;
	int healingIconTicksAccumulated = 0;
	int lowMoraleIconTicksAccumulated = 0;
	int psiIconTicksAccumulated = 0;
	int focusAnimationTicksAccumulated = 0;
	int selectionFrameTicksAccumulated = 0;

	bool colorForward = true;
	int colorCurrent = 0;
	// Cosmetic palette-pulse cadence: steps colorCurrent once every PALETTE_PULSE_DELAY()
	// update() calls instead of every call, so the pulse keeps its original ~60-step/s rate
	// (framework/uicadence.h) instead of speeding up 1:1 with the render loop's cadence.
	int paletteStepTicksAccumulated = 0;
	static int PALETTE_PULSE_DELAY() { return std::max(1, uiCosmeticFramesPerSecond() / 60); }
	sp<Palette> palette;
	std::vector<sp<Palette>> modPalette;

  public:
	BattleTileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
	               TileViewMode initialMode, Vec3<float> screenCenterTile, GameState &gameState);
	~BattleTileView() override;

	// In turn-based, preview path cost when hovering over same tile for more than set amount of
	// time
	StateRef<BattleUnit> lastSelectedUnit;
	Vec3<int> lastSelectedUnitPosition;
	Vec2<int> lastSelectedUnitFacing;
	// Real-time debounce (StageFrame::elapsedRealUs) against replaying the burn sample while
	// it is still playing - the sample's own duration, not a frame count: it gates whether a
	// sound replays, so it must stay accurate regardless of frame rate.
	uint64_t fireSoundDelayUs = 0;

	// Real-time debounce (StageFrame::elapsedRealUs) between hidden-unit-bar refreshes while
	// hideDisplay is active. Named HIDDEN_BAR_REFRESH_DELAY_US in battletileview.cpp.
	uint64_t hiddenBarElapsedUs = 0;
	void updateHiddenBar();

	sp<Image> pathPreviewTooFar;
	sp<Image> pathPreviewUnreachable;
	std::list<Vec3<int>> pathPreview;
	// Real-time hover duration (StageFrame::elapsedRealUs), not a frame count - see
	// battleview.cpp's PATH_PREVIEW_HOVER_DELAY_US.
	uint64_t pathPreviewElapsedUs = 0;
	enum class PreviewedPathCostSpecial : int
	{
		UNREACHABLE = -3,
		TOO_FAR = -2,
		NONE = -1
	};
	int previewedPathCost = static_cast<int>(PreviewedPathCostSpecial::NONE);
	void resetPathPreview();

	sp<Image> attackCostOutOfRange;
	sp<Image> attackCostNoArc;
	// Real-time debounce (StageFrame::elapsedRealUs), not a frame count - see
	// battleview.cpp's ATTACK_COST_CALC_DELAY_US.
	uint64_t attackCostElapsedUs = 0;
	enum class CalculatedAttackCostSpecial : int
	{
		NO_WEAPON = -4,
		NO_ARC = -3,
		OUT_OF_RANGE = -2,
		NONE = -1
	};
	int calculatedAttackCost = static_cast<int>(CalculatedAttackCostSpecial::NONE);
	void resetAttackCost();
	// updateAttackCost is in battleView as it requires data about selection mode

	bool hideDisplay = false;

	bool revealWholeMap = false;

	void setZLevel(int zLevel);
	int getZLevel();

	void setScreenCenterTile(Vec2<float> center) override;
	void setScreenCenterTile(Vec3<float> center) override;
	void setScreenCenterTile(Vec2<int> center) override
	{
		this->setScreenCenterTile(Vec2<float>{center.x, center.y});
	}
	void setScreenCenterTile(Vec3<int> center) override
	{
		this->setScreenCenterTile(Vec3<float>{center.x, center.y, center.z});
	}

	void setLayerDrawingMode(LayerDrawingMode mode);

	void setSelectedTilePosition(Vec3<int> newPosition) override;

	void eventOccurred(Event *e) override;
	void render() override;
	void update(const StageFrame &frame) override;
};

template <> struct is_partial_enum<BattleTileView::PreviewedPathCostSpecial> : std::true_type
{
};
template <> struct is_partial_enum<BattleTileView::CalculatedAttackCostSpecial> : std::true_type
{
};

} // namespace OpenApoc
