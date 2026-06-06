#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "Engine/BlockingVolume.h"
#include "Engine/CullDistanceVolume.h"
#include "Engine/TriggerVolume.h"
#include "GameFramework/KillZVolume.h"
#include "GameFramework/PhysicsVolume.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeAttachment.h"
#include "McpAutomationBridge_VolumeCullDistances.h"
#include "McpAutomationBridge_VolumeGeometry.h"
#include "McpAutomationBridge_VolumeResponses.h"
#include "McpBridgeWebSocket.h"

#if MCP_HAS_POSTPROCESS_VOLUME
#include "Engine/PostProcessVolume.h"
#endif

namespace McpVolumeHandlers
{
#if WITH_EDITOR
template<typename TVolumeClass, typename ConfigureFunc>
static bool AddVolumeToActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FVector& DefaultExtent,
    const FString& NameSuffix,
    const FString& ClassText,
    const FString& DisplayName,
    const FString& FailureText,
    ConfigureFunc Configure)
{
    using namespace VolumeHelpers;
    FVolumeAttachmentArgs Args;
    if (!ResolveAttachmentTarget(Subsystem, RequestId, Payload, Socket, DefaultExtent, Args))
    {
        return true;
    }
    Configure(Args);
    const FString VolumeName = Args.TargetActor->GetActorLabel() + NameSuffix;
    TVolumeClass* Volume = SpawnVolumeActor<TVolumeClass>(Args.World, VolumeName, Args.Location, Args.Rotation, Args.Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FailureText, nullptr);
        return true;
    }
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, ClassText);
    Configure(Volume, ResponseJson);
    const bool bAttachmentSucceeded = AttachVolumeToTarget(Volume, Args.TargetActor);
    SendAttachedVolumeResponse(Subsystem, RequestId, Socket, Args.TargetActor, Volume, ResponseJson, DisplayName, bAttachmentSucceeded);
    return true;
}

struct FNoAttachConfig
{
    void operator()(VolumeHelpers::FVolumeAttachmentArgs&) const {}
    template<typename TVolumeClass>
    void operator()(TVolumeClass*, const TSharedPtr<FJsonObject>&) const {}
};

bool HandleAddTriggerVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return AddVolumeToActor<ATriggerVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(100.0f, 100.0f, 100.0f), TEXT("_TriggerVolume"), TEXT("ATriggerVolume"),
        TEXT("TriggerVolume"), TEXT("Failed to spawn TriggerVolume"), FNoAttachConfig());
}

bool HandleAddBlockingVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return AddVolumeToActor<ABlockingVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(200.0f, 200.0f, 200.0f), TEXT("_BlockingVolume"), TEXT("ABlockingVolume"),
        TEXT("BlockingVolume"), TEXT("Failed to spawn BlockingVolume"), FNoAttachConfig());
}

bool HandleAddKillZVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    struct FKillZConfig
    {
        float KillZHeight;
        void operator()(VolumeHelpers::FVolumeAttachmentArgs& Args) const { if (KillZHeight != 0.0f) { Args.Location.Z = KillZHeight; } }
        void operator()(AKillZVolume* Volume, const TSharedPtr<FJsonObject>& ResponseJson) const { ResponseJson->SetNumberField(TEXT("killZHeight"), Volume->GetActorLocation().Z); }
    };
    const float KillZHeight = GetJsonNumberField(Payload, TEXT("killZHeight"), 0.0f);
    return AddVolumeToActor<AKillZVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(1000.0f, 1000.0f, 100.0f), TEXT("_KillZVolume"), TEXT("AKillZVolume"),
        TEXT("KillZVolume"), TEXT("Failed to spawn KillZVolume"), FKillZConfig{KillZHeight});
}

bool HandleAddPhysicsVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    struct FPhysicsConfig
    {
        bool bWaterVolume;
        float FluidFriction;
        float TerminalVelocity;
        void operator()(VolumeHelpers::FVolumeAttachmentArgs&) const {}
        void operator()(APhysicsVolume* Volume, const TSharedPtr<FJsonObject>& ResponseJson) const
        {
            Volume->bWaterVolume = bWaterVolume;
            Volume->FluidFriction = FluidFriction;
            Volume->TerminalVelocity = TerminalVelocity;
            ResponseJson->SetBoolField(TEXT("bWaterVolume"), bWaterVolume);
        }
    };
    return AddVolumeToActor<APhysicsVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(300.0f, 300.0f, 300.0f), TEXT("_PhysicsVolume"), TEXT("APhysicsVolume"),
        TEXT("PhysicsVolume"), TEXT("Failed to spawn PhysicsVolume"),
        FPhysicsConfig{GetJsonBoolField(Payload, TEXT("bWaterVolume"), false), GetJsonNumberField(Payload, TEXT("fluidFriction"), 0.3f), GetJsonNumberField(Payload, TEXT("terminalVelocity"), 4000.0f)});
}

bool HandleAddCullDistanceVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    struct FCullConfig
    {
        const TSharedPtr<FJsonObject>& Payload;
        void operator()(VolumeHelpers::FVolumeAttachmentArgs&) const {}
        void operator()(ACullDistanceVolume* Volume, const TSharedPtr<FJsonObject>&) const { VolumeHelpers::SetCullDistancesFromPayload(Volume, Payload); }
    };
    return AddVolumeToActor<ACullDistanceVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(1000.0f, 1000.0f, 500.0f), TEXT("_CullDistanceVolume"), TEXT("ACullDistanceVolume"),
        TEXT("CullDistanceVolume"), TEXT("Failed to spawn CullDistanceVolume"), FCullConfig{Payload});
}

#if MCP_HAS_POSTPROCESS_VOLUME
bool HandleAddPostProcessVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    struct FPostProcessConfig
    {
        float Priority;
        float BlendRadius;
        float BlendWeight;
        bool bEnabled;
        bool bUnbound;
        void operator()(VolumeHelpers::FVolumeAttachmentArgs&) const {}
        void operator()(APostProcessVolume* Volume, const TSharedPtr<FJsonObject>& ResponseJson) const
        {
            Volume->Priority = Priority;
            Volume->BlendRadius = BlendRadius;
            Volume->BlendWeight = BlendWeight;
            Volume->bEnabled = bEnabled;
            Volume->bUnbound = bUnbound;
            ResponseJson->SetNumberField(TEXT("priority"), Priority);
        }
    };
    return AddVolumeToActor<APostProcessVolume>(Subsystem, RequestId, Payload, Socket,
        FVector(500.0f, 500.0f, 500.0f), TEXT("_PostProcessVolume"), TEXT("APostProcessVolume"),
        TEXT("PostProcessVolume"), TEXT("Failed to spawn PostProcessVolume"),
        FPostProcessConfig{GetJsonNumberField(Payload, TEXT("priority"), 0.0f), GetJsonNumberField(Payload, TEXT("blendRadius"), 100.0f), GetJsonNumberField(Payload, TEXT("blendWeight"), 1.0f), GetJsonBoolField(Payload, TEXT("enabled"), true), GetJsonBoolField(Payload, TEXT("bUnbound"), false)});
}
#endif
#endif
}
