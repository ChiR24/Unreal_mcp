#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

class FMcpBridgeWebSocket;
class UClass;
class ULevel;
class ULevelStreaming;
class UMcpAutomationBridgeSubsystem;
class UWorld;

namespace LevelStructureHelpers
{
using McpHandlerUtils::GetEditorWorld;

// Replies success with Message and Result, first saving Level when the payload's `save` is true (default false).
// A save request on an unsaved /Temp/ level, or a failed save, replies SAVE_FAILED instead; the edit itself stays
// in memory. Shared by the level-structure and volume edits, which all change the open level.
void SendLevelEditResult(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, ULevel* Level, const FString& Message, TSharedPtr<FJsonObject> Result);

// streamingMethod "Blueprint" names ULevelStreamingDynamic and "AlwaysLoaded" ULevelStreamingAlwaysLoaded
// (case-insensitive); null for anything else.
UClass* ResolveLevelStreamingClass(const FString& StreamingMethod);

// The editor world's streaming level whose package (or its short name) is LevelName; when none is and the package
// exists at LevelName, under the persistent level's folder, or under /Game, a ULevelStreamingDynamic is added for it.
ULevelStreaming* FindOrAddStreamingLevel(UWorld* World, const FString& LevelName);

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
}
