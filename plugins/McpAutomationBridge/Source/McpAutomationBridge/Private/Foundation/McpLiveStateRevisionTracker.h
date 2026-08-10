#pragma once

#include "CoreMinimal.h"

// Task 42 editor-event bridge: binds the editor/asset delegates that advance
// FMcpLiveStateRevisions, so the counters a client pins against actually move
// when the editor changes.
//
// Exposed as free functions over a file-static tracker rather than a subsystem
// member, because the subsystem's PUBLIC header must not include a Private
// header and is already near the 250 pure-line ceiling. The subsystem only needs
// the start/stop pair.

/** Bind every tracked editor delegate. Game thread only; idempotent. */
void McpStartLiveStateTracking();

/** Unbind every tracked editor delegate. Idempotent; safe without a prior start. */
void McpStopLiveStateTracking();
