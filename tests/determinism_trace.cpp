#include "determinism_trace.h"

#include "framework/logger.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/shared/agent.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace OpenApoc
{

namespace
{
UString formatVec3(const Vec3<float> &v)
{
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(6) << v.x << "," << v.y << "," << v.z;
	return ss.str();
}
} // namespace

void captureCheckpoint(const GameState &state, uint64_t tickIndex, TraceLines &out)
{
	{
		std::ostringstream ss;
		ss << "CHECKPOINT tick_index=" << tickIndex << " game_ticks=" << state.gameTime.getTicks();
		out.push_back(ss.str());
	}

	// The RNG's internal state is a stream-position fingerprint: two runs with identical state
	// here have consumed bit-identical draw sequences since the seed, which is a strictly
	// tighter determinism check than a raw draw count would be (a count can coincide while the
	// underlying draws differ; the state cannot).
	{
		uint64_t s[2] = {0, 0};
		state.rng.getState(s);
		std::ostringstream ss;
		ss << "rng_state s0=0x" << std::hex << std::setw(16) << std::setfill('0') << s[0]
		   << " s1=0x" << std::setw(16) << std::setfill('0') << s[1] << std::dec;
		out.push_back(ss.str());
	}

	{
		std::ostringstream ss;
		ss << "vehicle_count=" << state.vehicles.size();
		out.push_back(ss.str());
	}
	for (const auto &v : state.vehicles)
	{
		std::ostringstream ss;
		ss << "vehicle id=" << v.first << " pos=" << formatVec3(v.second->position);
		out.push_back(ss.str());
	}

	{
		std::ostringstream ss;
		ss << "agent_count=" << state.agents.size();
		out.push_back(ss.str());
	}
	for (const auto &a : state.agents)
	{
		std::ostringstream ss;
		ss << "agent id=" << a.first << " pos=" << formatVec3(a.second->position);
		out.push_back(ss.str());
	}

	out.push_back("END_CHECKPOINT");
}

bool compareTraces(const TraceLines &a, const TraceLines &b, UString &firstDifference)
{
	const size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n; i++)
	{
		if (a[i] != b[i])
		{
			std::ostringstream ss;
			ss << "line " << i << ": \"" << a[i] << "\" != \"" << b[i] << "\"";
			firstDifference = ss.str();
			return false;
		}
	}
	if (a.size() != b.size())
	{
		std::ostringstream ss;
		ss << "trace length differs: " << a.size() << " vs " << b.size() << " lines";
		firstDifference = ss.str();
		return false;
	}
	return true;
}

bool writeTraceFile(const UString &path, const TraceLines &lines)
{
	std::ofstream f(path);
	if (!f)
	{
		LogError("Failed to open trace file \"{0}\" for writing", path);
		return false;
	}
	for (const auto &line : lines)
	{
		f << line << "\n";
	}
	return static_cast<bool>(f);
}

bool readTraceFile(const UString &path, TraceLines &out)
{
	std::ifstream f(path);
	if (!f)
	{
		LogError("Failed to open trace file \"{0}\" for reading", path);
		return false;
	}
	out.clear();
	std::string line;
	while (std::getline(f, line))
	{
		out.push_back(line);
	}
	return true;
}

} // namespace OpenApoc
