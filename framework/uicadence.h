#pragma once

#include <cstdint>

namespace OpenApoc
{

// One StageFrame::elapsedRealUs tick per real second - names the unit so UI real-duration
// constants read as "N seconds" (e.g. US_PER_SECOND / 2 for "0.5s") instead of a bare literal.
static constexpr uint64_t US_PER_SECOND = 1000000ull;

/*
    Effective frame rate for purely cosmetic, frame-counted UI cadences (icon rotation,
    palette pulsation, caret/ticker blink).

    This answers "how many times a second does render()/update() run right now?", not "how
    much real time actually elapsed", and is only appropriate for effects that merely need
    their *visual rate* to stay correct, not a real-world duration:

    - It is Options::targetFPS, substituting its own reference rate (60) when the option is
      0 ("unlimited"): a frame-counted cadence needs *some* rate to divide by, and 60 is what
      these constants were tuned at originally, making it the least surprising fallback when
      the render loop is uncapped and the achieved rate is unknown up front.
    - It is the *configured target*, not the achieved rate - Framework does not currently
      expose an achieved-FPS measurement to the UI layer. A machine that cannot hit its
      target will see these cosmetics run slightly slow, same as plain frame-counting always
      has; that is a pre-existing limitation, not one introduced here.

    Real-duration UI timers that gate a UI or game action (as opposed to decorating the
    screen) should not use this: they should accumulate StageFrame::elapsedRealUs directly
    against a named *_US constant, which is exact regardless of the actual frame rate. That
    route is only available inside Stage::update(const StageFrame&) - Control::update() (see
    forms/control.h) takes no timing parameter at all and is called from many places besides
    Stage::update(), so Controls (Ticker, TextEdit, EquipmentPaperDoll) use this frame-counted
    route instead: widening Control::update() to carry elapsedRealUs would touch every
    override across the forms/ hierarchy, which stays out of scope here.
*/
int uiCosmeticFramesPerSecond();

} // namespace OpenApoc
