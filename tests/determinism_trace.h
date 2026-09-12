#pragma once

#include "library/strings.h"
#include <cstdint>
#include <vector>

namespace OpenApoc
{

class GameState;

// One determinism trace is a flat sequence of lines. Each line is a self-contained, stable
// text representation of one piece of checkpoint state, so two traces produced by independent
// runs (or by two different builds) can be compared byte-for-byte with a plain line diff.
using TraceLines = std::vector<UString>;

// Appends one checkpoint's worth of trace lines to `out`: the game time in ticks, the RNG
// stream position, and every vehicle/agent position, in the deterministic std::map key order
// StateRefMap already iterates in. `tickIndex` is the harness's own elapsed-tick counter
// (independent of gameTime, so a checkpoint header is still meaningful if a caller starts the
// clock somewhere other than zero).
void captureCheckpoint(const GameState &state, uint64_t tickIndex, TraceLines &out);

// Returns true if `a` and `b` are identical line-for-line. On mismatch, `firstDifference` is
// set to a human-readable description of the first differing (or missing) line; untouched on
// success.
bool compareTraces(const TraceLines &a, const TraceLines &b, UString &firstDifference);

// Writes `lines` to `path`, one per line (LF-terminated). Returns false on failure.
bool writeTraceFile(const UString &path, const TraceLines &lines);

// Reads a trace previously written by writeTraceFile into `out`. Returns false on failure.
bool readTraceFile(const UString &path, TraceLines &out);

} // namespace OpenApoc
