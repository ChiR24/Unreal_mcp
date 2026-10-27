#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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

// Replies success with Message and Result, first saving Level when the payload's `save` is true (default false).
// A save request on an unsaved /Temp/ level, or a failed save, replies SAVE_FAILED instead; the edit itself stays
// in memory. Shared by the level-structure and volume edits, which all change the open level.
void SendLevelEditResult(
    UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
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

// The node of Graph that Wanted names (above), else the last one whose name or title contains it (connect_nodes takes
// a part of a title); an exact match always wins.
UEdGraphNode* FindLevelBlueprintNode(UEdGraph* Graph, const FString& Wanted);
}
