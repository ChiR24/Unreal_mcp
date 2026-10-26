#pragma once

#include "CoreMinimal.h"

class UWorld;

// Which Play session this server started (control_editor play notes it once play has begun): what drives the running
// game refuses a session the user started, since they may be playing it. Kept by the session's game instance, so a
// level change inside the session still counts; weak, so a session that ended matches nothing.
namespace McpPlaySession
{
void NoteStarted(UWorld* PlayWorld);
bool StartedByMcp(const UWorld* PlayWorld);
}
