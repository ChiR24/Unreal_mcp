#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Item 13: the engine's CSV profiler writes one row per frame under Saved/Profiling/CSV (csvprofile start/stop); this
// reads that table as plain text, so it is the same on every 5.x. Summarize gives the frame-time stats (mean, p50, p95,
// max, in ms), the top stats by mean and by peak, and the worst hitch frames (frame time over HitchMs) with each frame's
// largest other columns. A pure function of the text: no engine state, safe on a worker thread.
namespace McpCsvProfile
{
/** Null (OutError set) when the text holds no header row and at least one frame. Top is 1-20; HitchMs above 0. */
TSharedPtr<FJsonObject> Summarize(const FString& Csv, int32 Top, double HitchMs, FString& OutError);
} // namespace McpCsvProfile
