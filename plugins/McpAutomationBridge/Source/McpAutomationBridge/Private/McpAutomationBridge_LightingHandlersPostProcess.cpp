#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Subsystems/EditorActorSubsystem.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

static APostProcessVolume* FindOrSpawnUnboundPostProcessVolume(UEditorActorSubsystem& ActorSS)
{
    for (AActor* Actor : ActorSS.GetAllLevelActors())
    {
        if (Actor && Actor->IsA<APostProcessVolume>())
        {
            APostProcessVolume* Candidate = Cast<APostProcessVolume>(Actor);
            if (Candidate->bUnbound)
            {
                return Candidate;
            }
        }
    }

    APostProcessVolume* PPV = Cast<APostProcessVolume>(
        SpawnActorInActiveWorld<AActor>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
    if (PPV)
    {
        PPV->bUnbound = true;
    }
    return PPV;
}

bool HandleSetExposure(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UEditorActorSubsystem* ActorSS)
{
    APostProcessVolume* PPV = FindOrSpawnUnboundPostProcessVolume(*ActorSS);
    if (!PPV)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to find/spawn PostProcessVolume"), TEXT("EXECUTION_ERROR"));
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
    APostProcessVolume* PPV = FindOrSpawnUnboundPostProcessVolume(*ActorSS);
    if (!PPV)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("Failed to find/spawn PostProcessVolume"), TEXT("EXECUTION_ERROR"));
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
