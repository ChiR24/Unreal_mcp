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

namespace McpLightingHandlers
{
namespace
{
// The payload's post-process volume in the editor world (spawned when missing); replies and returns null otherwise.
APostProcessVolume* ResolvePostProcessOrReply(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ResolveError;
    FString ResolveErrorCode;
    APostProcessVolume* PPV = McpRenderHandlers::McpResolvePostProcessVolume(
        McpHandlerUtils::GetEditorWorld(), Payload, true, ResolveError, ResolveErrorCode);
    if (!PPV)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            ResolveError.IsEmpty() ? FString(TEXT("Failed to find/spawn PostProcessVolume")) : ResolveError,
            ResolveErrorCode.IsEmpty() ? FString(TEXT("EXECUTION_ERROR")) : ResolveErrorCode);
    }
    return PPV;
}
}

bool HandleSetExposure(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    APostProcessVolume* PPV = ResolvePostProcessOrReply(Subsystem, RequestId, Payload, RequestingSocket);
    if (!PPV)
    {
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
    Resp->SetStringField(TEXT("actorName"), McpActorRef(PPV));
    McpHandlerUtils::AddVerification(Resp, PPV);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Exposure settings applied"), Resp);
    return true;
}

bool HandleSetAmbientOcclusion(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    APostProcessVolume* PPV = ResolvePostProcessOrReply(Subsystem, RequestId, Payload, RequestingSocket);
    if (!PPV)
    {
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
    Resp->SetStringField(TEXT("actorName"), McpActorRef(PPV));
    McpHandlerUtils::AddVerification(Resp, PPV);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Ambient Occlusion settings configured"), Resp);
    return true;
}

}

