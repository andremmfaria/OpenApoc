#pragma once

#include "forms/control.h"
#include "forms/forms_enums.h"
#include "framework/uicadence.h"
#include "library/sp.h"
#include "library/strings.h"
#include <algorithm>
#include <queue>

namespace OpenApoc
{

class BitmapFont;

class Ticker : public Control
{

  private:
	// Cosmetic, frame-counted cadences (framework/uicadence.h): Control::update() carries no
	// elapsed-time parameter (unlike Stage::update(const StageFrame&)), so a real-duration
	// conversion here would require threading elapsedRealUs through Control/Form's update()
	// interface, which stays out of scope here. "N frames at the
	// original 60 FPS baseline" for the scroll (1s) and display-hold (10s) cadences.
	static int ANIM_TICKS() { return uiCosmeticFramesPerSecond(); }
	static int DISPLAY_TICKS() { return uiCosmeticFramesPerSecond() * 10; }
	// One line of ticker text is LINE_HEIGHT px tall and must scroll that many px over
	// ANIM_TICKS() calls regardless of FPS - onRender() divides animTimer by this instead of
	// the old hardcoded 4 (60 / LINE_HEIGHT) so the scroll *distance* stays fixed while only
	// the call count needed to cover it scales with FPS.
	static constexpr int LINE_HEIGHT = 15;
	static int SCROLL_DIVISOR() { return std::max(1, ANIM_TICKS() / LINE_HEIGHT); }

	bool animating;
	int animTimer, displayTimer;

	UString text;
	std::queue<UString> messages;
	sp<BitmapFont> font;

  protected:
	void onRender() override;

  public:
	HorizontalAlignment TextHAlign;
	VerticalAlignment TextVAlign;

	Ticker(sp<BitmapFont> font = nullptr);
	~Ticker() override;

	void eventOccured(Event *e) override;
	void update() override;
	void unloadResources() override;

	void addMessage(const UString &Text);
	bool hasMessages() const { return !text.empty() || !messages.empty(); }

	sp<BitmapFont> getFont() const;
	void setFont(sp<BitmapFont> NewFont);

	sp<Control> copyTo(sp<Control> CopyParent) override;
	void configureSelfFromXml(pugi::xml_node *node) override;
};

}; // namespace OpenApoc
