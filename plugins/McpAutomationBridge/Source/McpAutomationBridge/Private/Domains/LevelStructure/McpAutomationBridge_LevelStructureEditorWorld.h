#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class ULevel;
class UWorld;

namespace LevelStructureHelpers
{
#if WITH_EDITOR
UWorld* GetEditorWorld();

/**
 * Resolve the level a level-blueprint request targets.
 *
 * The level blueprint belongs to a level, so a request that names `levelPath`
 * must edit THAT level. The previous handlers edited World->GetCurrentLevel()
 * and ignored `levelPath` entirely, so editing a level that was not the one
 * open silently wrote the node into whatever level happened to be loaded —
 * including a throwaway /Temp/ untitled world, which reported success and then
 * discarded the work.
 *
 *  - An explicit `levelPath` (or `level`) is matched against every loaded level
 *    (persistent and streaming sublevels). No match => OutError says the level
 *    is not open.
 *  - With no explicit path the persistent level is used: the level blueprint
 *    does not belong to a sublevel that happens to be selected in the panel.
 *  - Transient /Temp/ levels are refused for a MUTATING request, because their
 *    level blueprint cannot survive being replaced. A caller that only opens or
 *    reads the blueprint passes bAllowTransient=true.
 *
 * Returns nullptr and fills OutError on refusal; returns the target otherwise.
 */
ULevel* ResolveTargetLevelForBlueprintRequest(
    UWorld* World, const TSharedPtr<FJsonObject>& Payload, FString& OutError,
    bool bAllowTransient = false);
#endif
}
