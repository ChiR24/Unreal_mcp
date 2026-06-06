#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "Engine/BlockingVolume.h"
#include "GameFramework/KillZVolume.h"
#include "GameFramework/PainCausingVolume.h"
#include "GameFramework/PhysicsVolume.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeGeometry.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"

namespace McpVolumeHandlers
{
#if WITH_EDITOR
template<typename TVolumeClass>
static bool CreateSimpleGameplayVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FVector& DefaultExtent,
    const FString& ClassText,
    const FString& FailureText,
    const FString& CreatedPrefix)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), DefaultExtent, Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    TVolumeClass* Volume = SpawnVolumeActor<TVolumeClass>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FailureText, nullptr);
        return true;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        CreatedPrefix + Args.VolumeName, CreateVolumeResponse(Volume, ClassText));
    return true;
}

bool HandleCreateBlockingVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return CreateSimpleGameplayVolume<ABlockingVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(100.0f, 100.0f, 100.0f), TEXT("ABlockingVolume"), TEXT("Failed to spawn BlockingVolume"), TEXT("Created BlockingVolume: "));
}

bool HandleCreateKillZVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return CreateSimpleGameplayVolume<AKillZVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(10000.0f, 10000.0f, 100.0f), TEXT("AKillZVolume"), TEXT("Failed to spawn KillZVolume"), TEXT("Created KillZVolume: "));
}

bool HandleCreatePainCausingVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f), Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    const bool bPainCausing = GetJsonBoolField(Payload, TEXT("bPainCausing"), true);
    const float DamagePerSec = GetJsonNumberField(Payload, TEXT("damagePerSec"), 10.0f);
    APainCausingVolume* Volume = SpawnVolumeActor<APainCausingVolume>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn PainCausingVolume"), nullptr);
        return true;
    }
    Volume->bPainCausing = bPainCausing;
    Volume->DamagePerSec = DamagePerSec;
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("APainCausingVolume"));
    ResponseJson->SetBoolField(TEXT("bPainCausing"), bPainCausing);
    ResponseJson->SetNumberField(TEXT("damagePerSec"), DamagePerSec);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PainCausingVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}

bool HandleCreatePhysicsVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    FVector Extent;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f), Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    APhysicsVolume* Volume = SpawnVolumeActor<APhysicsVolume>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn PhysicsVolume"), nullptr);
        return true;
    }
    Volume->bWaterVolume = GetJsonBoolField(Payload, TEXT("bWaterVolume"), false);
    Volume->FluidFriction = GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f);
    Volume->TerminalVelocity = GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f);
    Volume->Priority = GetJsonIntField(Payload, TEXT("priority"), 0);
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("APhysicsVolume"));
    ResponseJson->SetBoolField(TEXT("bWaterVolume"), Volume->bWaterVolume);
    ResponseJson->SetNumberField(TEXT("fluidFriction"), Volume->FluidFriction);
    ResponseJson->SetNumberField(TEXT("terminalVelocity"), Volume->TerminalVelocity);
    ResponseJson->SetNumberField(TEXT("priority"), Volume->Priority);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created PhysicsVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}
#endif
}
