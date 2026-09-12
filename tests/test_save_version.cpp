#include "framework/configfile.h"
#include "framework/filesystem.h"
#include "framework/logger.h"
#include "game/state/gamestate.h"
#include "game/state/gamestate_serialize.h"
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
	UString valueStr = contents.substr(openPos + openTag.size(),
	                                   closePos - (openPos + openTag.size()));
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

	if (!allPassed)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
