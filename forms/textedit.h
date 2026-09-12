#pragma once

#include "forms/control.h"
#include "forms/forms_enums.h"
#include "framework/uicadence.h"
#include "library/sp.h"
#include "library/strings.h"
#include <algorithm>

namespace OpenApoc
{

class BitmapFont;

// Cosmetic caret-blink cadence (framework/uicadence.h): Control::update() carries no
// elapsed-time parameter, so this stays frame-counted rather than real-time, the same
// reasoning documented in forms/ticker.h. "5 frames at the original 60 FPS baseline"
// (a 12 Hz toggle rate).
inline int textEditorCaretToggleFrames() { return std::max(1, uiCosmeticFramesPerSecond() / 12); }

class TextEdit : public Control
{

  private:
	bool caretDraw;
	int caretTimer;
	U32String text;
	UString cursor;
	sp<BitmapFont> font;
	bool editing;
	U32String allowedCharacters;
	size_t textMaxLength = std::string::npos;
	void raiseEvent(FormEventType Type);

  protected:
	void onRender() override;

  public:
	bool isFocused() const override;
	unsigned int SelectionStart;
	HorizontalAlignment TextHAlign;
	VerticalAlignment TextVAlign;

	TextEdit(const UString &Text = "", sp<BitmapFont> font = nullptr);
	~TextEdit() override;

	void eventOccured(Event *e) override;
	void update() override;
	void unloadResources() override;

	UString getText() const;
	void setText(const UString &Text);
	void setCursor(const UString &cursor);
	void setTextMaxSize(size_t length);
	// set to empty string to allow everything
	void setAllowedCharacters(const UString &allowedCharacters);

	sp<BitmapFont> getFont() const;
	void setFont(sp<BitmapFont> NewFont);

	sp<Control> copyTo(sp<Control> CopyParent) override;
	void configureSelfFromXml(pugi::xml_node *node) override;
};

}; // namespace OpenApoc
