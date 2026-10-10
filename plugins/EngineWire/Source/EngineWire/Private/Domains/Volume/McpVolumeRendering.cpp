#include "Domains/Volume/McpVolumeActionDecls.h"

#include "Dom/JsonObject.h"
#include "Engine/CullDistanceVolume.h"
#include "Lightmass/LightmassImportanceVolume.h"
#include "Lightmass/PrecomputedVisibilityVolume.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Domains/Volume/McpVolumeCullDistances.h"
#include "Domains/Volume/McpVolumeGeometry.h"
#include "Domains/Volume/McpVolumeRequestParsing.h"
#include "Domains/Volume/McpVolumeResponses.h"
#include "Domains/Volume/McpVolumeWorldResolve.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if MCP_HAS_POSTPROCESS_VOLUME
#include "Engine/PostProcessVolume.h"
#endif

namespace McpVolumeHandlers
{
#if MCP_HAS_POSTPROCESS_VOLUME
void ApplyPostProcessSettings(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& Payload)
{
    if (!Volume || !Payload->HasTypedField<EJson::Object>(TEXT("postProcessSettings")))
    {
        return;
    }
    TSharedPtr<FJsonObject> SettingsJson = Payload->GetObjectField(TEXT("postProcessSettings"));
    if (SettingsJson->HasTypedField<EJson::Boolean>(TEXT("bloomEnabled")))
    {
        Volume->Settings.bOverride_BloomIntensity = true;
        Volume->Settings.BloomIntensity = SettingsJson->GetBoolField(TEXT("bloomEnabled")) ? 1.0f : 0.0f;
    }
    if (SettingsJson->HasTypedField<EJson::Number>(TEXT("exposureBias")))
    {
        Volume->Settings.bOverride_AutoExposureBias = true;
        Volume->Settings.AutoExposureBias = SettingsJson->GetNumberField(TEXT("exposureBias"));
    }
    if (SettingsJson->HasTypedField<EJson::Number>(TEXT("vignetteIntensity")))
    {
        Volume->Settings.bOverride_VignetteIntensity = true;
        Volume->Settings.VignetteIntensity = SettingsJson->GetNumberField(TEXT("vignetteIntensity"));
    }
    if (SettingsJson->HasTypedField<EJson::Number>(TEXT("saturation")))
    {
        Volume->Settings.bOverride_ColorSaturation = true;
        const float Value = SettingsJson->GetNumberField(TEXT("saturation"));
        Volume->Settings.ColorSaturation = FVector4(Value, Value, Value, Volume->Settings.ColorSaturation.W);
    }
    if (SettingsJson->HasTypedField<EJson::Number>(TEXT("contrast")))
    {
        Volume->Settings.bOverride_ColorContrast = true;
        const float Value = SettingsJson->GetNumberField(TEXT("contrast"));
        Volume->Settings.ColorContrast = FVector4(Value, Value, Value, Volume->Settings.ColorContrast.W);
    }
    if (SettingsJson->HasTypedField<EJson::Number>(TEXT("gamma")))
    {
        Volume->Settings.bOverride_ColorGamma = true;
        const float Value = SettingsJson->GetNumberField(TEXT("gamma"));
        Volume->Settings.ColorGamma = FVector4(Value, Value, Value, Volume->Settings.ColorGamma.W);
    }
}

bool HandleCreatePostProcessVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(500.0f, 500.0f, 500.0f), Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    APostProcessVolume* Volume = SpawnVolumeActor<APostProcessVolume>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn PostProcessVolume"), nullptr);
        return true;
    }
    Volume->Priority = GetJsonNumberField(Payload, TEXT("priority"), 0.0f);
    Volume->BlendRadius = GetJsonNumberField(Payload, TEXT("blendRadius"), 100.0f);
    Volume->BlendWeight = GetJsonNumberField(Payload, TEXT("blendWeight"), 1.0f);
    // bEnabled is the declared name; `enabled` is only a legacy fallback.
    Volume->bEnabled = GetJsonBoolField(Payload, TEXT("bEnabled"), GetJsonBoolField(Payload, TEXT("enabled"), true));
    Volume->bUnbound = GetJsonBoolField(Payload, TEXT("bUnbound"), false);
    ApplyPostProcessSettings(Volume, Payload);
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("APostProcessVolume"));
    ResponseJson->SetNumberField(TEXT("priority"), Volume->Priority);
    ResponseJson->SetNumberField(TEXT("blendRadius"), Volume->BlendRadius);
    ResponseJson->SetNumberField(TEXT("blendWeight"), Volume->BlendWeight);
    ResponseJson->SetBoolField(TEXT("enabled"), Volume->bEnabled);
    ResponseJson->SetBoolField(TEXT("unbound"), Volume->bUnbound);
    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Volume->GetLevel(),
        FString::Printf(TEXT("Created PostProcessVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}
#endif

bool HandleCreateCullDistanceVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(1000.0f, 1000.0f, 500.0f), Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    ACullDistanceVolume* Volume = SpawnVolumeActor<ACullDistanceVolume>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn CullDistanceVolume"), nullptr);
        return true;
    }
    SetCullDistancesFromPayload(Volume, Payload);
    LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, Volume->GetLevel(),
        FString::Printf(TEXT("Created CullDistanceVolume: %s"), *Args.VolumeName), CreateVolumeResponse(Volume, TEXT("ACullDistanceVolume")));
    return true;
}

bool HandleCreatePrecomputedVisibilityVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<APrecomputedVisibilityVolume>(Subsystem, RequestId, Payload, Socket, FVector(1000.0f, 1000.0f, 500.0f));
}

bool HandleCreateLightmassImportanceVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ALightmassImportanceVolume>(Subsystem, RequestId, Payload, Socket, FVector(5000.0f, 5000.0f, 2000.0f));
}
}
