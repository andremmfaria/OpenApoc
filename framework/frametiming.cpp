#include "framework/frametiming.h"

namespace OpenApoc
{

FrameDuration frameDurationForTargetFPS(int targetFPS)
{
	if (targetFPS <= 0)
	{
		return {true, 0};
	}
	return {false, 1000000ull / static_cast<uint64_t>(targetFPS)};
}

} // namespace OpenApoc
