#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/FileHelper.h"

class FMcpBridgeWebSocket;
class UClass;
class UEdGraph;
class UEdGraphNode;
class ULevel;
class ULevelScriptBlueprint;
class ULevelStreaming;
class UEngineWireSubsystem;
class UWorld;

namespace LevelStructureHelpers
{
using McpHandlerUtils::GetEditorWorld;

// Replies success with Message and Result, first saving Level when the payload's `save` is true (default
// bSaveByDefault).
// A save request on an unsaved /Temp/ level, or a failed save, replies SAVE_FAILED instead; the edit itself stays
// in memory. Shared by the level-structure and volume edits, which all change the open level.
void SendLevelEditResult(
    UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, ULevel* Level, const FString& Message, TSharedPtr<FJsonObject> Result,
    bool bSaveByDefault = false);

// streamingMethod "Blueprint" names ULevelStreamingDynamic and "AlwaysLoaded" ULevelStreamingAlwaysLoaded
// (case-insensitive); null for anything else.
UClass* ResolveLevelStreamingClass(const FString& StreamingMethod);

// A level name is an object name, a package name and a file name: false with OutReason for a character none of them
// takes, a reserved device name (CON, LPT1, ...) or a length past what a file takes. create_level and create_sublevel.
inline bool IsValidLevelName(const FString& Name, FText& OutReason)
{
    return FName::IsValidXName(Name, FString(INVALID_OBJECTNAME_CHARACTERS) + INVALID_LONGPACKAGE_CHARACTERS, &OutReason) &&
           FFileHelper::IsFilenameValidForSaving(Name, OutReason);
}

// The editor world's streaming level LevelName names (its package, object path or short name, as manage_level finds a
// sublevel: McpLevelHandlers::FindStreamingLevels); when none does, bAddFromDisk is set and the package exists at
// LevelName, under the persistent level's folder, or under /Game, a ULevelStreamingDynamic is added for it. Null once the
// refusal is sent:
// LEVEL_NOT_FOUND, or AMBIGUOUS_LEVEL for a short name two sublevels share.
ULevelStreaming* FindOrAddStreamingLevelOrReply(UEngineWireSubsystem* Subsystem, const FString& RequestId,
                                                TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, const FString& LevelName,
                                                bool bAddFromDisk = true);

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

// The level a level-blueprint edit targets (above), its level Blueprint and that Blueprint's event graph; replies and
// returns false when one is missing. bCreate makes the level Blueprint when the level has none yet (adding a node
// does); an edit of nodes that must already exist never creates one.
bool ResolveLevelBlueprintGraph(
    UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
    const TSharedPtr<FJsonObject>& Payload, bool bCreate, ULevel*& OutLevel, ULevelScriptBlueprint*& OutBlueprint,
    UEdGraph*& OutGraph);

// Whether Wanted is Node's name, nodeId (guid) or title (the full title, its first line as the node shows it, or the
// list title), any case.
bool IsLevelBlueprintNodeNamed(UEdGraphNode* Node, const FString& Wanted);

// The nodes of every graph of LevelBP (its functions too) that Match accepts, in graph order.
TArray<UEdGraphNode*> FindLevelBlueprintNodes(ULevelScriptBlueprint* LevelBP, TFunctionRef<bool(UEdGraphNode*)> Match);
}
