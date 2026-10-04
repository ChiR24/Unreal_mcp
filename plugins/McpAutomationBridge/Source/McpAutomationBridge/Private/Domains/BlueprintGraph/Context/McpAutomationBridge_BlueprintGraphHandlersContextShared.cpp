#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"

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


}