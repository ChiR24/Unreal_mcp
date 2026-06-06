#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeGeometry.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"
#include "Sound/AudioVolume.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
static AAudioVolume* SpawnAudioVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& FailureText,
    VolumeHelpers::FVolumeCreateArgs& OutArgs)
{
    using namespace VolumeHelpers;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), OutArgs) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(500.0f, 500.0f, 200.0f), Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return nullptr;
    }
    AAudioVolume* Volume = SpawnVolumeActor<AAudioVolume>(World, OutArgs.VolumeName, OutArgs.Location, OutArgs.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FailureText, nullptr);
    }
    return Volume;
}

bool HandleCreateAudioVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    AAudioVolume* Volume = SpawnAudioVolume(Subsystem, RequestId, Payload, Socket, TEXT("Failed to spawn AudioVolume"), Args);
    if (!Volume)
    {
        return true;
    }
    const bool bEnabled = GetJsonBoolField(Payload, TEXT("bEnabled"), true);
    Volume->SetEnabled(bEnabled);
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("AAudioVolume"));
    ResponseJson->SetBoolField(TEXT("bEnabled"), bEnabled);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created AudioVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}

bool HandleCreateReverbVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    AAudioVolume* Volume = SpawnAudioVolume(Subsystem, RequestId, Payload, Socket, TEXT("Failed to spawn ReverbVolume (AudioVolume)"), Args);
    if (!Volume)
    {
        return true;
    }
    const bool bEnabled = GetJsonBoolField(Payload, TEXT("bEnabled"), true);
    const float ReverbVolumeLevel = GetJsonNumberField(Payload, TEXT("reverbVolume"), 0.5f);
    const float FadeTime = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.5f);
    Volume->SetEnabled(bEnabled);
    FReverbSettings ReverbSettings = Volume->GetReverbSettings();
    ReverbSettings.bApplyReverb = true;
    ReverbSettings.Volume = ReverbVolumeLevel;
    ReverbSettings.FadeTime = FadeTime;
    Volume->SetReverbSettings(ReverbSettings);
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("AAudioVolume (Reverb)"));
    ResponseJson->SetBoolField(TEXT("bEnabled"), bEnabled);
    ResponseJson->SetNumberField(TEXT("reverbVolume"), ReverbVolumeLevel);
    ResponseJson->SetNumberField(TEXT("fadeTime"), FadeTime);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created ReverbVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}
#endif
}
