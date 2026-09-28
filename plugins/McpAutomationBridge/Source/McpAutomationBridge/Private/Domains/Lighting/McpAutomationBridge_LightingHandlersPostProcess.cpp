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

    // Each value sets its bOverride_ flag: without it the volume overrides nothing and the
    // written value never takes effect. method was declared and never read.
    static const TMap<FString, EAutoExposureMethod> Methods = {
        {TEXT("Manual"), AEM_Manual}, {TEXT("AutoExposureHistogram"), AEM_Histogram}, {TEXT("AutoExposureBasic"), AEM_Basic}};
    const FString Method = GetJsonStringField(Payload, TEXT("method"));
    const EAutoExposureMethod* MethodValue = Methods.Find(Method);
    if (!Method.IsEmpty() && !MethodValue)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid exposure method: %s. Valid values: Manual, AutoExposureHistogram, AutoExposureBasic"), *Method),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    PPV->Modify();
    TArray<FString> Applied;
    if (MethodValue)
    {
        PPV->Settings.bOverride_AutoExposureMethod = true;
        PPV->Settings.AutoExposureMethod = *MethodValue;
        Applied.Add(TEXT("AutoExposureMethod"));
    }
    double Value = 0.0;
    if (Payload->TryGetNumberField(TEXT("minBrightness"), Value))
    {
        PPV->Settings.bOverride_AutoExposureMinBrightness = true;
        PPV->Settings.AutoExposureMinBrightness = static_cast<float>(Value);
        Applied.Add(TEXT("AutoExposureMinBrightness"));
    }
    if (Payload->TryGetNumberField(TEXT("maxBrightness"), Value))
    {
        PPV->Settings.bOverride_AutoExposureMaxBrightness = true;
        PPV->Settings.AutoExposureMaxBrightness = static_cast<float>(Value);
        Applied.Add(TEXT("AutoExposureMaxBrightness"));
    }
    if (Payload->TryGetNumberField(TEXT("compensationValue"), Value))
    {
        PPV->Settings.bOverride_AutoExposureBias = true;
        PPV->Settings.AutoExposureBias = static_cast<float>(Value);
        Applied.Add(TEXT("AutoExposureBias"));
    }
    if (Applied.Num() == 0)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("No exposure value supplied: pass method, minBrightness, maxBrightness or compensationValue"),
            TEXT("NO_SETTING_SUPPLIED"));
        return true;
    }
    PPV->MarkPackageDirty();
    PPV->MarkComponentsRenderStateDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(PPV));
    Resp->SetArrayField(TEXT("appliedSettings"), McpHandlerUtils::ToJsonStringArray(Applied));
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

    // quality was declared and never read; the volume's AO quality runs 0 to 100.
    static const TMap<FString, float> QualityLevels = {{TEXT("Low"), 25.0f}, {TEXT("Medium"), 50.0f}, {TEXT("High"), 100.0f}};
    const FString Quality = GetJsonStringField(Payload, TEXT("quality"));
    const float* QualityLevel = QualityLevels.Find(Quality);
    if (!Quality.IsEmpty() && !QualityLevel)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid ambient occlusion quality: %s. Valid values: Low, Medium, High"), *Quality),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    PPV->Modify();
    if (QualityLevel)
    {
        PPV->Settings.bOverride_AmbientOcclusionQuality = true;
        PPV->Settings.AmbientOcclusionQuality = *QualityLevel;
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
    PPV->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), McpActorRef(PPV));
    McpHandlerUtils::AddVerification(Resp, PPV);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Ambient Occlusion settings configured"), Resp);
    return true;
}

}

