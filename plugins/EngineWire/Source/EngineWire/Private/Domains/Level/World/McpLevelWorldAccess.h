#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class ULevel;
class ULevelStreaming;
class UWorld;

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World);

/** The streaming levels whose package (PIE prefix removed) is Name or ends in /Name; an exact path wins outright. */
TArray<ULevelStreaming*> FindStreamingLevels(UWorld* World, const FString& Name);

/** False for a Level Instance's level or one loaded at run time: a transient entry the open level never saves, which
 *  the Levels panel leaves out. */
bool IsUserSublevel(const ULevelStreaming* Streaming);

/** The one streaming level Wanted names (FindStreamingLevels). OutLevels is {levels: the world's loaded levels} for
 *  this refusal and the caller's own; with no match or several, null, and OutRefusal and OutCode say which, naming
 *  OpenLevel and those levels. */
ULevelStreaming* FindOneStreamingLevel(UWorld* World, const FString& Wanted, const FString& OpenLevel,
                                       TSharedPtr<FJsonObject>& OutLevels, FString& OutRefusal, const TCHAR*& OutCode);
} // namespace McpLevelHandlers
