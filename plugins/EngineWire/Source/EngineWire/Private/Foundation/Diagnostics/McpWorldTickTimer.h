#pragma once

#include "CoreMinimal.h"

class UWorld;

// How long each world's own tick takes: from the start of its tick to the end of its actor, component and physics tick
// groups. While Play In Editor runs, the game thread time also holds the editor's own UI and viewports (Slate took 8 of
// a PIE frame's 21 ms), so a stats read gives this as the game's share.
namespace McpWorldTickTimer
{
// Starts timing every world's tick; the subsystem calls it at startup and Stop at shutdown, so no binding outlives
// the module (a Live Coding unload).
void Start();
void Stop();

// Milliseconds of World's recent ticks, averaged over about the last 30 frames; negative before one was timed.
double AverageMs(const UWorld* World);
}
