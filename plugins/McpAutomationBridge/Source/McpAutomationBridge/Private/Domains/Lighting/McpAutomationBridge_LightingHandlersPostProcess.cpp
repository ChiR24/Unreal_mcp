#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "Subsystems/EditorActorSubsystem.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

bool HandleSetExposure(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS)
{
    if (!ActorSS)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString ResolveError;
    FString ResolveErrorCode;
    APostProcessVolume* PPV = McpRenderHandlers::McpResolvePostProcessVolume(
        World, Payload, true, ResolveError, ResolveErrorCode);
    if (!PPV)
    {
        const FString ErrorMessage = ResolveError.IsEmpty()
            ? FString(TEXT("Failed to find/spawn PostProcessVolume"))
            : ResolveError;
        const FString ErrorCode = ResolveErrorCode.IsEmpty()
            ? FString(TEXT("EXECUTION_ERROR"))
            : ResolveErrorCode;
        Subsystem.SendAutomationError(RequestingSocket, RequestId, ErrorMessage, ErrorCode);
        return true;
    }

    double MinB = 0.0;
    double MaxB = 0.0;
    if (Payload->TryGetNumberField(TEXT("minBrightness"), MinB))
    {
        PPV->Settings.AutoExposureMinBrightness = static_cast<float>(MinB);
    }
    if (Payload->TryGetNumberField(TEXT("maxBrightness"), MaxB))
    {
        PPV->Settings.AutoExposureMaxBrightness = static_cast<float>(MaxB);
    }

    double Comp = 0.0;
    if (Payload->TryGetNumberField(TEXT("compensationValue"), Comp))
    {
        PPV->Settings.AutoExposureBias = static_cast<float>(Comp);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, PPV);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Exposure settings applied"), Resp);
    return true;
}

bool HandleSetAmbientOcclusion(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS)
{
    if (!ActorSS)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("EditorActorSubsystem not available"), TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString ResolveError;
    FString ResolveErrorCode;
    APostProcessVolume* PPV = McpRenderHandlers::McpResolvePostProcessVolume(
        World, Payload, true, ResolveError, ResolveErrorCode);
    if (!PPV)
    {
        const FString ErrorMessage = ResolveError.IsEmpty()
            ? FString(TEXT("Failed to find/spawn PostProcessVolume"))
            : ResolveError;
        const FString ErrorCode = ResolveErrorCode.IsEmpty()
            ? FString(TEXT("EXECUTION_ERROR"))
            : ResolveErrorCode;
        Subsystem.SendAutomationError(RequestingSocket, RequestId, ErrorMessage, ErrorCode);
        return true;
    }

    bool bEnabled = true;
    if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled))
    {
        PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
        PPV->Settings.AmbientOcclusionIntensity = bEnabled ? 0.5f : 0.0f;
    }

    double Intensity;
    if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
    {
        PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
        PPV->Settings.AmbientOcclusionIntensity = static_cast<float>(Intensity);
    }

    double Radius;
    if (Payload->TryGetNumberField(TEXT("radius"), Radius))
    {
        PPV->Settings.bOverride_AmbientOcclusionRadius = true;
        PPV->Settings.AmbientOcclusionRadius = static_cast<float>(Radius);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());
    McpHandlerUtils::AddVerification(Resp, PPV);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Ambient Occlusion settings configured"), Resp);
    return true;
}

}

#endif
