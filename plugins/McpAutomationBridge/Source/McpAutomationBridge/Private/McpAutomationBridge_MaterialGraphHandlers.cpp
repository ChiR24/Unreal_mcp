#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "Materials/Material.h"

bool UMcpAutomationBridgeSubsystem::HandleMaterialGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_material_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
    {
        SendAutomationError(
            Socket,
            RequestId,
            TEXT("Missing 'subAction' for manage_material_graph"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    using namespace McpMaterialGraphHandlers;
    if (HandleAddNode(*this, RequestId, SubAction, Payload, Socket, *Material)) { return true; }
    if (HandleRemoveNode(*this, RequestId, SubAction, Payload, Socket, *Material)) { return true; }
    if (HandleConnectNodes(*this, RequestId, SubAction, Payload, Socket, *Material)) { return true; }
    if (HandleBreakConnections(*this, RequestId, SubAction, Payload, Socket, *Material)) { return true; }
    if (HandleGetNodeDetails(*this, RequestId, SubAction, Payload, Socket, *Material)) { return true; }

    SendAutomationError(
        Socket,
        RequestId,
        FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
        TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
