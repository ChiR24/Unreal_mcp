#pragma once

#include "CoreMinimal.h"

class APawn;
class FJsonObject;
class UWorld;
namespace McpPratt
{
struct FNode;
}

// Which Play session this server started (control_editor play notes it once play has begun): what drives the running
// game refuses a session the user started, since they may be playing it. Kept by the session's game instance, so a
// level change inside the session still counts; weak, so a session that ended matches nothing.
namespace McpPlaySession
{
void NoteStarted(UWorld* PlayWorld);
bool StartedByMcp(const UWorld* PlayWorld);
// The pawn Name names in World (label, name or path), else the first player's pawn; null when there is none.
APawn* ResolvePawn(UWorld* World, const FString& Name);
// The NO_PAWN refusal of what drives a pawn: ResolvePawn found none, or one no controller drives.
inline const TCHAR* const NoPawnMessage = TEXT("No controlled pawn to drive: the player has none, or actorName names no pawn with a controller.");
// Each text of the string array Field, an assertion in execute's expect language, parsed into Roots beside its text in
// Texts; false with OutError naming Field at the first that does not parse or past the sixteenth.
bool ParseAssertions(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, TArray<TSharedPtr<McpPratt::FNode>>& Roots,
                     TArray<FString>& Texts, FString& OutError);
}
