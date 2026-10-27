#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPDiag.h"

namespace McpBlueprintGraphHandlers
{

void FActionContext::SendError(
    const FString& Message,
    const FString& ErrorCode) const
{
    Subsystem->SendAutomationError(
        RequestingSocket,
        RequestId,
        Message,
        ErrorCode);
}

void FActionContext::SendNodeNotFound(const FString& Id) const
{
    // A bare "Node not found." sent callers back to a GUID copied from the Blueprint this
    // one was duplicated from; a duplicate gets new GUIDs but keeps every node name.
    SendError(
        FString::Printf(TEXT("Could not find node '%s' in graph %s (%d nodes). Use a node's nodeGuid (an "
                             "unambiguous prefix of 8+ hex characters also works) or its node name such as "
                             "K2Node_IfThenElse_0; a duplicated Blueprint gets new GUIDs but keeps the names. "
                             "inspect_graph info \"graph\" lists them."),
                        *Id, TargetGraph ? *TargetGraph->GetName() : TEXT("?"),
                        TargetGraph ? TargetGraph->Nodes.Num() : 0),
        TEXT("NODE_NOT_FOUND"));
}

void FActionContext::SendErrorWithDetails(
    const FString& Message,
    const FString& ErrorCode,
    const TSharedPtr<FJsonObject>& Details) const
{
    // SendError carries message + code only; placement refusals (and any future
    // caller that must hand back coordinates, suggestions or payloads on
    // failure) need the result object too, so route through the full response.
    Subsystem->SendAutomationResponse(
        RequestingSocket,
        RequestId,
        false,
        Message,
        Details,
        ErrorCode);
}

void FActionContext::NameBlueprint(const TSharedPtr<FJsonObject>& Result, bool bChanged) const
{
    if (!Result.IsValid() || !Blueprint)
    {
        return;
    }
    const FString Path = Blueprint->GetOutermost()->GetName();
    if (!Result->HasField(TEXT("assetPath")) && !Result->HasField(TEXT("blueprintPath")))
    {
        Result->SetStringField(TEXT("blueprintPath"), Path);
    }
    if (bChanged && !Result->HasField(TEXT("changedAssets")))
    {
        TArray<TSharedPtr<FJsonValue>> Changed;
        Changed.Add(MakeShared<FJsonValueString>(Path));
        Result->SetArrayField(TEXT("changedAssets"), Changed);
    }
}

void FActionContext::SendResponse(
    const FString& Message,
    const TSharedPtr<FJsonObject>& Result) const
{
    FString OutMessage = Message;
    // A reply that named no asset (create_node on its dynamic path, build_graph) left the receipt with no
    // handle and no change. The dirty test below is the one this funnel already uses for "THIS call changed it";
    // a read (bDeferCompile, as a batch step that the batch names itself) never did: a widget edit that saved but
    // left the Blueprint uncompiled made the next inspect_graph list it under changedAssets.
    NameBlueprint(Result, !bDeferCompile && Blueprint && Blueprint->Status == BS_Dirty);
    // "Node created." while the graph no longer compiles is the worst answer a
    // mutation can give: nothing surfaces until someone presses Play, and by
    // then the edit that broke it is many calls back. Every mutation here marks
    // the blueprint modified (Status -> BS_Dirty), and a read does not, so a
    // dirty blueprint at response time means THIS call changed it. Compile it
    // once and report the outcome under the names the contract already
    // declares. The edit itself still succeeded; this only tells the caller
    // whether the blueprint survived it.
    if (!bDeferCompile && Blueprint && Blueprint->Status == BS_Dirty && Result.IsValid())
    {
        FString FirstError;
        if (!McpCompileBlueprintWithDiagnostics(Blueprint, Result, FirstError, 6))
        {
            OutMessage = FString::Printf(
                TEXT("%s WARNING: the blueprint no longer compiles: %s (see "
                     "`diagnostics`)"),
                *Message,
                FirstError.IsEmpty() ? TEXT("no compiler message") : *FirstError);
        }
        // The handler saved before this compile and the compile dirties the
        // package again, so every single-step edit left the Blueprint unsaved.
        Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Blueprint));
    }
    if (OnSuccess)
    {
        OnSuccess(OutMessage, Result);
        return;
    }
    Subsystem->SendAutomationResponse(
        RequestingSocket,
        RequestId,
        true,
        OutMessage,
        Result);
}

bool ValidateProvidedPaths(const FActionContext& Context)
{
    FString AssetPath;
    if (Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
        !AssetPath.IsEmpty())
    {
        if (SanitizeProjectRelativePath(AssetPath).IsEmpty())
        {
            Context.SendError(McpPathRefusalMessage(TEXT("assetPath"), AssetPath), TEXT("INVALID_PATH"));
            return false;
        }
    }

    FString BlueprintPath;
    if (Context.Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) &&
        !BlueprintPath.IsEmpty())
    {
        if (SanitizeProjectRelativePath(BlueprintPath).IsEmpty())
        {
            Context.SendError(McpPathRefusalMessage(TEXT("blueprintPath"), BlueprintPath), TEXT("INVALID_PATH"));
            return false;
        }
    }

    return true;
}

void FActionContext::FinishCreatedNode(UEdGraphNode* NewNode, float X, float Y) const
{
    // Refuse stacked placements: the node is already in the graph, so pull it back out on overlap and fail with
    // coordinates instead of silently stacking.
    FString OverlapMessage;
    TSharedPtr<FJsonObject> OverlapDetails;
    if (McpGraphLayout::RefuseOverlappingNode(TargetGraph, NewNode, X, Y, OverlapMessage, OverlapDetails,
                                              McpGraphLayout::NamesNoPosition(Payload)))
    {
        SendErrorWithDetails(OverlapMessage, TEXT("NODE_OVERLAP"), OverlapDetails);
        return;
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    SaveLoadedAssetThrottled(Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // `nodeGuid` is the name blueprint.create_node/add_event declare and mark REQUIRED in their output schema (the
    // gateway projects to declared names only); `nodeId` is the spelling delete_node/connect_pins consume.
    const FString NodeGuidText = NewNode->NodeGuid.ToString();
    Result->SetStringField(TEXT("nodeGuid"), NodeGuidText);
    Result->SetStringField(TEXT("nodeId"), NodeGuidText);
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    // Where the node landed and how big it is, so consecutive creates do not stack.
    const FString PlacementWarning = McpGraphLayout::AddNodePlacementFields(Result, *NewNode);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    SendResponse(PlacementWarning.IsEmpty() ? FString(TEXT("Node created.")) : FString::Printf(TEXT("Node created. %s"), *PlacementWarning),
                 Result);
}
}