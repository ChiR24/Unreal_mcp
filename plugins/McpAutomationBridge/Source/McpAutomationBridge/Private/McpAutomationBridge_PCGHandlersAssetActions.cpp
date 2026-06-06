#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleCreatePCGGraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    FString GraphPath;
    FString Error;
    if (!TryGetPCGAssetPath(Payload, {TEXT("graphPath"), TEXT("assetPath")}, GraphPath, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
    bool bCreated = false;
    bool bSaved = false;
    UPCGGraph* Graph = CreateOrReusePCGGraph(GraphPath, bOverwrite, bSave, bCreated, bSaved, Error);
    if (!Graph)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, bCreated ? TEXT("PCG graph created.") : TEXT("PCG graph already exists."), BuildGraphResult(Graph, GraphPath, bCreated, bSaved));
    return true;
}

bool HandleCreatePCGSubgraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    FString SubgraphPath;
    FString Error;
    if (!TryGetPCGAssetPath(Payload, {TEXT("subgraphPath"), TEXT("graphPath"), TEXT("assetPath")}, SubgraphPath, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString ParentGraphRawPath = GetJsonStringField(Payload, TEXT("parentGraphPath"));
    FString ParentGraphPath;
    UPCGGraph* ParentGraph = nullptr;
    if (!ParentGraphRawPath.IsEmpty())
    {
        ParentGraph = LoadPCGGraph(ParentGraphRawPath, ParentGraphPath, Error);
        if (!ParentGraph)
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
    bool bCreated = false;
    bool bSubgraphSaved = false;
    UPCGGraph* Subgraph = CreateOrReusePCGGraph(SubgraphPath, bOverwrite, bSave, bCreated, bSubgraphSaved, Error);
    if (!Subgraph)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = BuildGraphResult(Subgraph, SubgraphPath, bCreated, bSubgraphSaved);
    Result->SetStringField(TEXT("subgraphPath"), SubgraphPath);

    if (ParentGraph)
    {
        UPCGSettings* DefaultSettings = nullptr;
        UPCGNode* Node = ParentGraph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), DefaultSettings);
        UPCGBaseSubgraphSettings* SubgraphSettings = Cast<UPCGBaseSubgraphSettings>(DefaultSettings);
        if (!Node || !SubgraphSettings)
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create PCG subgraph node."), TEXT("CREATE_FAILED"));
            return true;
        }

        SubgraphSettings->SetSubgraph(Subgraph);
        ApplyNodeMetadata(Node, Payload);
        Node->UpdateAfterSettingsChangeDuringCreation();
        SubgraphSettings->PostEditChange();
        bool bParentSaved = false;
        if (!SaveGraphIfRequested(ParentGraph, bSave, bParentSaved, Error))
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("SAVE_FAILED"));
            return true;
        }

        Result->SetStringField(TEXT("parentGraphPath"), ParentGraphPath);
        Result->SetStringField(TEXT("nodeId"), Node->GetName());
        Result->SetStringField(TEXT("nodeName"), Node->GetName());
        Result->SetBoolField(TEXT("parentSaved"), bParentSaved);
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG subgraph created."), Result);
    return true;
}
}
#endif
