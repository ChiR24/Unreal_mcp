#pragma once

#include "CoreMinimal.h"

class AActor;

namespace McpLevelHandlers
{
/**
 * The engine's Map Check of only these actors (AActor::CheckForErrors, the per-actor half of Build > Map Check): each
 * warning, error or performance warning it raises, as a line. The change watch asks it for the actors a call changed;
 * the findings also land in the Map Check log, as any Map Check's do.
 */
void MapCheckActors(const TArray<AActor*>& Actors, TArray<FString>& OutIssues);
} // namespace McpLevelHandlers
