#include "Domains/Volume/McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GameFramework/PainCausingVolume.h"
#include "GameFramework/PhysicsVolume.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Volume/McpAutomationBridge_VolumeGeometry.h"
#include "Domains/Volume/McpAutomationBridge_VolumeRequestParsing.h"
#include "Domains/Volume/McpAutomationBridge_VolumeResponses.h"
#include "Domains/Volume/McpAutomationBridge_VolumeWorldResolution.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Sound/AudioVolume.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
static AActor* ResolveVolumeByName(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutVolumeName)
{
    using namespace VolumeHelpers;
    FString ValidationError;
    OutVolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT(""));
    if (!ValidateVolumeName(OutVolumeName, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    UWorld* World = nullptr;
    if (!ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return nullptr;
    }
    AActor* VolumeActor = FindVolumeByName(World, OutVolumeName);
    if (!VolumeActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Volume not found: %s"), *OutVolumeName), nullptr, TEXT("NOT_FOUND"));
    }
    return VolumeActor;
}

bool HandleSetVolumeExtent(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FString VolumeName;
    AActor* VolumeActor = ResolveVolumeByName(Subsystem, RequestId, Payload, Socket, VolumeName);
    if (!VolumeActor)
    {
        return true;
    }
    FVector NewExtent;
    if (!ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f), NewExtent))
    {
        return true;
    }
    SetVolumeExtentGeometry(VolumeActor, NewExtent);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    McpHandlerUtils::AddVerification(ResponseJson, VolumeActor);
    ResponseJson->SetObjectField(TEXT("newExtent"), CreateVectorObject(NewExtent));
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set extent for volume: %s"), *VolumeName), ResponseJson);
    return true;
}

bool HandleSetVolumeProperties(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString VolumeName;
    AActor* VolumeActor = ResolveVolumeByName(Subsystem, RequestId, Payload, Socket, VolumeName);
    if (!VolumeActor)
    {
        return true;
    }
    TArray<FString> PropertiesSet;
    if (APhysicsVolume* PhysicsVol = Cast<APhysicsVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bWaterVolume"))) { PhysicsVol->bWaterVolume = GetJsonBoolField(Payload, TEXT("bWaterVolume"), false); PropertiesSet.Add(TEXT("bWaterVolume")); }
        if (Payload->HasField(TEXT("fluidFriction"))) { PhysicsVol->FluidFriction = GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f); PropertiesSet.Add(TEXT("fluidFriction")); }
        if (Payload->HasField(TEXT("terminalVelocity"))) { PhysicsVol->TerminalVelocity = GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f); PropertiesSet.Add(TEXT("terminalVelocity")); }
        if (Payload->HasField(TEXT("priority"))) { PhysicsVol->Priority = GetJsonIntField(Payload, TEXT("priority"), 0); PropertiesSet.Add(TEXT("priority")); }
    }
    if (APainCausingVolume* PainVol = Cast<APainCausingVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bPainCausing"))) { PainVol->bPainCausing = GetJsonBoolField(Payload, TEXT("bPainCausing"), true); PropertiesSet.Add(TEXT("bPainCausing")); }
        if (Payload->HasField(TEXT("damagePerSec"))) { PainVol->DamagePerSec = GetJsonNumberField(Payload, TEXT("damagePerSec"), 10.0f); PropertiesSet.Add(TEXT("damagePerSec")); }
    }
    if (AAudioVolume* AudioVol = Cast<AAudioVolume>(VolumeActor))
    {
        if (Payload->HasField(TEXT("bEnabled"))) { AudioVol->SetEnabled(GetJsonBoolField(Payload, TEXT("bEnabled"), true)); PropertiesSet.Add(TEXT("bEnabled")); }
        bool bModifiedReverb = false;
        FReverbSettings ReverbSettings = AudioVol->GetReverbSettings();
        if (Payload->HasField(TEXT("reverbVolume"))) { ReverbSettings.Volume = GetJsonNumberField(Payload, TEXT("reverbVolume"), 0.5f); PropertiesSet.Add(TEXT("reverbVolume")); bModifiedReverb = true; }
        if (Payload->HasField(TEXT("fadeTime"))) { ReverbSettings.FadeTime = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.5f); PropertiesSet.Add(TEXT("fadeTime")); bModifiedReverb = true; }
        if (bModifiedReverb) { AudioVol->SetReverbSettings(ReverbSettings); }
    }
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    McpHandlerUtils::AddVerification(ResponseJson, VolumeActor);
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString& Prop : PropertiesSet)
    {
        PropsArray.Add(MakeShared<FJsonValueString>(Prop));
    }
    ResponseJson->SetArrayField(TEXT("propertiesSet"), PropsArray);

    // Any supplied field this volume's class does not carry is reported rather than dropped: the caller
    // otherwise cannot tell a property that applied from one that was silently discarded.
    TArray<TSharedPtr<FJsonValue>> IgnoredArray;
    if (Payload.IsValid())
    {
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Payload->Values)
        {
            const bool bRoutingField = Entry.Key == TEXT("volumeName") || Entry.Key == TEXT("volumeProperty")
                || Entry.Key == TEXT("action") || Entry.Key == TEXT("subAction");
            if (!bRoutingField && !PropertiesSet.Contains(Entry.Key))
            {
                IgnoredArray.Add(MakeShared<FJsonValueString>(Entry.Key));
            }
        }
    }
    if (IgnoredArray.Num() > 0)
    {
        ResponseJson->SetArrayField(TEXT("propertiesIgnored"), IgnoredArray);
    }

    // Setting nothing is not a success. This used to answer "Set 0 properties for volume: X" with
    // success:true, so a payload naming properties the volume's class does not have -- the contract's own
    // example names bEnabled on a post-process volume -- came back as a completed write.
    if (PropertiesSet.Num() == 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No property was set on volume '%s' (class %s). This action supports PhysicsVolume{bWaterVolume,fluidFriction,terminalVelocity,priority}, PainCausingVolume{bPainCausing,damagePerSec} and AudioVolume{bEnabled,reverbVolume,fadeTime}; use the extent or bounds variant for geometry. Nothing was changed."),
                *VolumeName, *VolumeActor->GetClass()->GetName()),
            ResponseJson, TEXT("NO_PROPERTIES_APPLIED"));
        return true;
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %d properties for volume: %s"), PropertiesSet.Num(), *VolumeName), ResponseJson);
    return true;
}
#endif
}
