#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleCreatePCGGraph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
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
    FString Code;
    UPCGGraph* Graph = CreateOrReusePCGGraph(GraphPath, bOverwrite, bSave, bCreated, bSaved, Error, Code);
    if (!Graph)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, bCreated ? TEXT("PCG graph created.") : TEXT("PCG graph already exists."), BuildGraphResult(Graph, GraphPath, bCreated, bSaved));
    return true;
}

bool HandleCreatePCGSubgraph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave)
{
    FString SubgraphPath;
    FString Error;
    if (!TryGetPCGAssetPath(Payload, {TEXT("subgraphPath")}, SubgraphPath, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString ParentGraphRawPath = GetJsonStringField(Payload, TEXT("parentGraphPath"));
    FString ParentGraphPath;
    UPCGGraph* ParentGraph = nullptr;
    if (!ParentGraphRawPath.IsEmpty())
    {
        ParentGraph = LoadPCGAsset<UPCGGraph>(ParentGraphRawPath, TEXT("PCG graph"), ParentGraphPath, Error);
        if (!ParentGraph)
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);
    bool bCreated = false;
    bool bSubgraphSaved = false;
    FString Code;
    UPCGGraph* Subgraph = CreateOrReusePCGGraph(SubgraphPath, bOverwrite, bSave, bCreated, bSubgraphSaved, Error, Code);
    if (!Subgraph)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }

    TSharedPtr<FJsonObject> Result = BuildGraphResult(Subgraph, SubgraphPath, bCreated, bSubgraphSaved);

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
