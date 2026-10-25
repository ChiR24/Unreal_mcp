#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// A build_material_graph step written as the single call is, {"edit": "add_material_node", "nodeKind":
// "world_position"}, failed with "Missing 'nodeType'": the gateway turns nodeKind into the typed adder (the
// add_material_node fold), and batch steps run inside the plugin, past it. This is that fold's member map:
// add_<kind>, except the three below; "batch" resolves to build_material_graph, which a step may not run.
inline FString McpMaterialBatchStepEdit(const FString& Edit, const TSharedPtr<FJsonObject>& Step)
{
    FString Kind;
    if (Edit != TEXT("add_material_node") || !Step->TryGetStringField(TEXT("nodeKind"), Kind) || Kind.IsEmpty())
    {
        return Edit;
    }
    if (Kind == TEXT("node"))
    {
        return Edit;
    }
    if (Kind == TEXT("math"))
    {
        return TEXT("add_math_node");
    }
    if (Kind == TEXT("material_function"))
    {
        return TEXT("use_material_function");
    }
    return Kind == TEXT("batch") ? FString(TEXT("build_material_graph")) : TEXT("add_") + Kind;
}

// The step helpers build_material_graph runs and checks with (McpMatGraphBatchCheck.cpp).
namespace McpMaterialAuthoringHandlers
{
/** Additive edits only: deleting and disconnecting stay single calls behind their own consent. */
bool IsBatchableMaterialEdit(const FString& Edit);

/** Why a step's edit cannot run in a batch, listing the edits that can. */
FString DescribeUnbatchableMaterialEdit(const FString& Edit);

/** A node adder or use_material_function: a step whose id later steps may name as "$id". */
bool CreatesNode(const FString& Edit);

/** Whether a step places its node itself (x or posX); the batch lays out the others. */
bool HasPosition(const TSharedPtr<FJsonObject>& Step);

/** The step's own fields over the batch's shared ones, from/to shorthand expanded into node and pin fields. */
TSharedPtr<FJsonObject> BuildStepPayload(const TSharedPtr<FJsonObject>& Batch, const TSharedPtr<FJsonObject>& Step,
                                         const FString& Edit);

/** "$name" -> the node id an earlier step created under `id: "name"`; false, with OutError, for one it did not. */
bool ResolveAliases(const TMap<FString, FString>& Aliases, const TSharedPtr<FJsonObject>& Payload, FString& OutError);

/** Every step whose edit the batch does not run or whose "$id" names no earlier node-making step, as
 *  {index, edit, errorCode, error}: what a run would stop at, found before anything is applied. */
TArray<TSharedPtr<FJsonValue>> CheckMaterialBatch(const TSharedPtr<FJsonObject>& Payload,
                                                  const TArray<TSharedPtr<FJsonValue>>& Steps);

/** Replies for the check: the problems (nothing applied), or for dryRun with none the steps checked and the nodes the
 *  batch would create. */
bool AnswerMaterialBatchCheck(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const TArray<TSharedPtr<FJsonValue>>& Steps, const TArray<TSharedPtr<FJsonValue>>& Problems,
                              bool bDryRun);
} // namespace McpMaterialAuthoringHandlers
