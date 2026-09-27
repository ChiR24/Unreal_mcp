#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "Engine/StaticMesh.h"
#include "Runtime/Launch/Resources/Version.h"

namespace McpRenderHandlers
{
bool HandleNaniteRebuildMesh(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"));
    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, TEXT("assetPath required."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
    if (!StaticMesh)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, TEXT("StaticMesh not found."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    FMeshNaniteSettings Settings = StaticMesh->GetNaniteSettings();
    Settings.bEnabled = true;
    StaticMesh->SetNaniteSettings(Settings);
#else
    StaticMesh->NaniteSettings.bEnabled = true;
#endif

    if (UPackage* Package = StaticMesh->GetOutermost())
    {
        Package->MarkPackageDirty();
    }
    StaticMesh->Build(true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), TEXT("manage_render"));
    Result->SetStringField(TEXT("subAction"), TEXT("nanite_rebuild_mesh"));
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("naniteEnabled"), true);
    Result->SetBoolField(TEXT("rebuilt"), true);
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Nanite enabled and mesh rebuilt."), Result);
    return true;
}
}
