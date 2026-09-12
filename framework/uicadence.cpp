#include "framework/uicadence.h"
#include "framework/options.h"

namespace OpenApoc
{

int uiCosmeticFramesPerSecond()
{
	int targetFPS = Options::targetFPS.get();
	return targetFPS > 0 ? targetFPS : 60;
}

} // namespace OpenApoc
