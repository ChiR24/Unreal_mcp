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
    UPCGGraph* Graph = CreateOrReusePCGGraph(GraphPath, bOverwrite, bSave, bCreated, bSaved, Error);
    if (!Graph)
    {
        auto Result = BuildGraphResult(nullptr, GraphPath, bCreated, bSaved);
        Result->SetBoolField(TEXT("committed"), bCreated);
        Result->SetBoolField(TEXT("residualChanges"), bCreated);
        Result->SetBoolField(TEXT("rolledBack"), false);
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result,
            bCreated ? TEXT("SAVE_FAILED") : bOverwrite ? TEXT("CREATE_FAILED") : TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    auto Result = BuildGraphResult(Graph, GraphPath, bCreated, bSaved);
    Result->SetBoolField(TEXT("committed"), true);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Result->SetBoolField(TEXT("residualChanges"), false);
    Bridge->SendAutomationResponse(Socket, RequestId, true, bCreated ? TEXT("PCG graph created.") : TEXT("PCG graph already exists."), Result);
    return true;
}

}
#endif
