#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/TriggerBox.h"
#include "Engine/TriggerCapsule.h"
#include "Engine/TriggerSphere.h"
#include "Engine/TriggerVolume.h"
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
bool HandleCreateTriggerVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
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
    ATriggerVolume* Volume = SpawnVolumeActor<ATriggerVolume>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn TriggerVolume"), nullptr);
        return true;
    }
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("ATriggerVolume"));
    ResponseJson->SetObjectField(TEXT("location"), CreateVectorObject(Volume->GetActorLocation()));
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerVolume: %s"), *Args.VolumeName), ResponseJson);
    return true;
}

bool HandleCreateTriggerBox(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    UWorld* World = nullptr;
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        return true;
    }
    FVector Extent = GetVectorFromPayload(Payload, TEXT("boxExtent"), FVector(100.0f, 100.0f, 100.0f));
    if (Extent == FVector::ZeroVector)
    {
        Extent = GetVectorFromPayload(Payload, TEXT("extent"), FVector(100.0f, 100.0f, 100.0f));
    }
    FString ValidationError;
    if (!ValidateExtent(Extent, ValidationError))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    ATriggerBox* Volume = SpawnVolumeActor<ATriggerBox>(World, Args.VolumeName, Args.Location, Args.Rotation, Extent);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn TriggerBox"), nullptr);
        return true;
    }
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("ATriggerBox"));
    ResponseJson->SetObjectField(TEXT("boxExtent"), CreateVectorObject(Extent));
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerBox: %s"), *Args.VolumeName), ResponseJson);
    return true;
}

bool HandleCreateTriggerSphere(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    UWorld* World = nullptr;
    FString ValidationError;
    const float Radius = GetJsonNumberField(Payload, TEXT("sphereRadius"), 100.0f);
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ValidateRadius(Radius, ValidationError) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        if (!ValidationError.IsEmpty())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        }
        return true;
    }
    ATriggerSphere* Volume = SpawnVolumeActor<ATriggerSphere>(World, Args.VolumeName, Args.Location, Args.Rotation, FVector::ZeroVector);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn TriggerSphere"), nullptr);
        return true;
    }
    if (USphereComponent* SphereComp = Volume->GetCollisionComponent() ? Cast<USphereComponent>(Volume->GetCollisionComponent()) : nullptr)
    {
        SphereComp->SetSphereRadius(Radius);
    }
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("ATriggerSphere"));
    ResponseJson->SetNumberField(TEXT("radius"), Radius);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerSphere: %s"), *Args.VolumeName), ResponseJson);
    return true;
}

bool HandleCreateTriggerCapsule(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace VolumeHelpers;
    FVolumeCreateArgs Args;
    UWorld* World = nullptr;
    FString ValidationError;
    const float Radius = GetJsonNumberField(Payload, TEXT("capsuleRadius"), 50.0f);
    const float HalfHeight = GetJsonNumberField(Payload, TEXT("capsuleHalfHeight"), 100.0f);
    if (!ReadNamedTransform(Subsystem, RequestId, Payload, Socket, TEXT("TriggerVolume"), Args) ||
        !ValidateCapsuleDimensions(Radius, HalfHeight, ValidationError) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, World))
    {
        if (!ValidationError.IsEmpty())
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false, *ValidationError, nullptr, TEXT("INVALID_ARGUMENT"));
        }
        return true;
    }
    ATriggerCapsule* Volume = SpawnVolumeActor<ATriggerCapsule>(World, Args.VolumeName, Args.Location, Args.Rotation, FVector::ZeroVector);
    if (!Volume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to spawn TriggerCapsule"), nullptr);
        return true;
    }
    if (UCapsuleComponent* CapsuleComp = Volume->GetCollisionComponent() ? Cast<UCapsuleComponent>(Volume->GetCollisionComponent()) : nullptr)
    {
        CapsuleComp->SetCapsuleSize(Radius, HalfHeight);
    }
    TSharedPtr<FJsonObject> ResponseJson = CreateVolumeResponse(Volume, TEXT("ATriggerCapsule"));
    ResponseJson->SetNumberField(TEXT("radius"), Radius);
    ResponseJson->SetNumberField(TEXT("halfHeight"), HalfHeight);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created TriggerCapsule: %s"), *Args.VolumeName), ResponseJson);
    return true;
}
#endif
}
