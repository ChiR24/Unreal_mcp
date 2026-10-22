#include "Domains/PCG/McpPCGPrivate.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Foundation/McpScopedEditorTransaction.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
bool CanAttachSubgraph(UPCGGraph* Parent, UPCGGraph* Subgraph)
{
    TArray<UPCGGraph*> Pending = {Subgraph};
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        if (Pending[Index] == Parent || Pending.Num() > 256) return false;
        for (UPCGNode* Node : Pending[Index]->GetNodes())
        {
            const UPCGBaseSubgraphSettings* Settings = Node ? Cast<UPCGBaseSubgraphSettings>(Node->GetSettings()) : nullptr;
            if (UPCGGraph* Child = Settings ? Settings->GetSubgraph() : nullptr) Pending.AddUnique(Child);
        }
    }
    return true;
}
}

bool HandleCreatePCGSubgraph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    FString SubgraphPath, Error;
    if (!TryGetPCGAssetPath(Payload, {TEXT("subgraphPath"), TEXT("graphPath"), TEXT("assetPath")}, SubgraphPath, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString ParentRaw = GetJsonStringField(Payload, TEXT("parentGraphPath"));
    FString ParentPath;
    UPCGGraph* Parent = nullptr;
    TUniquePtr<FPCGEditState> ParentState;
    if (!ParentRaw.IsEmpty())
    {
        Parent = LoadPCGAsset<UPCGGraph>(ParentRaw, TEXT("PCG graph"), ParentPath, Error);
        if (!Parent || !CheckPCGGraphRevision(Parent, Payload, Error) || ParentPath.Equals(SubgraphPath, ESearchCase::IgnoreCase))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error.IsEmpty() ? TEXT("A graph cannot be its own subgraph.") : Error, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        ParentState = MakeUnique<FPCGEditState>(Parent);
    }
    bool bCreated = false, bSubgraphSaved = false, bParentCommitted = false, bParentSaved = false, bRestored = false;
    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
    UPCGGraph* Subgraph = CreateOrReusePCGGraph(SubgraphPath, bOverwrite, bSave, bCreated, bSubgraphSaved, Error);
    auto Result = BuildGraphResult(Subgraph, SubgraphPath, bCreated, bSubgraphSaved);
    Result->SetStringField(TEXT("subgraphPath"), SubgraphPath);
    if (Parent) Result->SetStringField(TEXT("parentGraphPath"), ParentPath);
    const auto Reply = [&](bool bSuccess, const FString& Message, const FString& Code)
    {
        Result->SetBoolField(TEXT("committed"), Subgraph && (!Parent || bParentCommitted));
        Result->SetBoolField(TEXT("saved"), bSubgraphSaved && (!Parent || bParentSaved));
        Result->SetBoolField(TEXT("subgraphSaved"), bSubgraphSaved);
        Result->SetBoolField(TEXT("parentCommitted"), bParentCommitted);
        Result->SetBoolField(TEXT("parentSaved"), bParentSaved);
        Result->SetBoolField(TEXT("rolledBack"), bRestored);
        Result->SetBoolField(TEXT("residualChanges"), !bSuccess && (bCreated || bParentCommitted));
        Result->SetBoolField(TEXT("partialSave"), Parent && ((bSubgraphSaved && !bParentSaved) || (bCreated && bParentSaved && !bSubgraphSaved)));
        Result->SetBoolField(TEXT("atomic"), false);
        Result->SetStringField(TEXT("rollbackScope"), TEXT("parent_authored_state_only_no_cross_package_disk_rollback"));
        Bridge->SendAutomationResponse(Socket, RequestId, bSuccess, Message, Result, Code);
        return true;
    };
    if (!Subgraph) return Reply(false, Error, bCreated ? TEXT("SAVE_FAILED") : bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
    if (!Parent) return Reply(true, TEXT("PCG subgraph created."), FString());
    if (!CanAttachSubgraph(Parent, Subgraph))
        return Reply(false, TEXT("The attachment would create a recursive subgraph dependency or exceed its bounded traversal."), TEXT("INVALID_SUBGRAPH"));
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Attach PCG Subgraph")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, ParentState->GetObjects());
    UPCGSettings* DefaultSettings = nullptr;
    UPCGNode* Node = Parent->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), DefaultSettings);
    UPCGBaseSubgraphSettings* Settings = Cast<UPCGBaseSubgraphSettings>(DefaultSettings);
    if (Node && Settings)
    {
        Settings->SetSubgraph(Subgraph);
        ApplyNodeMetadata(Node, Payload);
        Node->UpdateAfterSettingsChangeDuringCreation();
        Settings->PostEditChange();
    }
    TSharedPtr<FJsonObject> EditorState;
    if (!Node || !Settings || !SyncPCGEditorGraph(Parent, EditorState, Error) || GetPCGGraphRevision(Parent).IsEmpty())
    {
        bRestored = ParentState->Restore();
        bParentCommitted = !bRestored;
        return Reply(false, Error.IsEmpty() ? TEXT("Could not attach the PCG subgraph with a complete authored snapshot.") : Error,
            bRestored ? TEXT("SUBGRAPH_ATTACH_FAILED") : TEXT("ROLLBACK_FAILED"));
    }
    bParentCommitted = true;
    Result->SetStringField(TEXT("nodeId"), Node->GetName());
    Result->SetStringField(TEXT("nodeName"), Node->GetName());
    Result->SetObjectField(TEXT("editorGraph"), EditorState);
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Parent));
    Transaction.DescribeInto(Result);
    if (!SaveGraphIfRequested(Parent, bSave, bParentSaved, Error)) return Reply(false, Error, TEXT("SAVE_FAILED"));
    return Reply(true, TEXT("PCG subgraph attached; package saves are reported separately, not as a disk transaction."), FString());
}
}
#endif
