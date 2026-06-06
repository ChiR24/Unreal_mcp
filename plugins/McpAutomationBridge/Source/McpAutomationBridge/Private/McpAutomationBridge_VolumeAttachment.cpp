#include "McpAutomationBridge_VolumeAttachment.h"

#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Brush.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_VolumeRequestParsing.h"
#include "McpAutomationBridge_VolumeWorldResolution.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/EngineVersionComparison.h"

namespace VolumeHelpers
{
#if WITH_EDITOR
bool ResolveAttachmentTarget(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FVector& DefaultExtent, FVolumeAttachmentArgs& OutArgs)
{
    const FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"), TEXT(""));
    if (ActorPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("actorPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return false;
    }
    if (ActorPath.Contains(TEXT("..")) || ActorPath.Contains(TEXT("\\")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("actorPath contains invalid characters"), nullptr, TEXT("SECURITY_VIOLATION"));
        return false;
    }
    if (!ReadExtent(Subsystem, RequestId, Payload, Socket, TEXT("extent"), DefaultExtent, OutArgs.Extent) ||
        !ResolveEditorWorld(Subsystem, RequestId, Socket, OutArgs.World))
    {
        return false;
    }
    OutArgs.TargetActor = FindActorByPathOrName(OutArgs.World, ActorPath);
    if (!OutArgs.TargetActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("Actor not found: %s"), *ActorPath), nullptr, TEXT("NOT_FOUND"));
        return false;
    }
    OutArgs.Location = OutArgs.TargetActor->GetActorLocation();
    OutArgs.Rotation = OutArgs.TargetActor->GetActorRotation();
    return true;
}

bool AttachVolumeToTarget(AActor* VolumeActor, AActor* TargetActor)
{
    if (!VolumeActor || !TargetActor)
    {
        return false;
    }
    if (USceneComponent* TargetRootComponent = TargetActor->GetRootComponent())
    {
        if (TargetRootComponent->Mobility != EComponentMobility::Static)
        {
            if (ABrush* BrushVolume = Cast<ABrush>(VolumeActor))
            {
                if (UBrushComponent* BrushComp = BrushVolume->GetBrushComponent())
                {
                    BrushComp->SetMobility(EComponentMobility::Movable);
                }
            }
        }
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    return VolumeActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
#else
    VolumeActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
    return true;
#endif
}

void SendAttachedVolumeResponse(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, AActor* TargetActor, AActor* VolumeActor, TSharedPtr<FJsonObject> ResponseJson, const FString& DisplayName, bool bAttachmentSucceeded)
{
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    McpHandlerUtils::AddVerification(ResponseJson, VolumeActor);
    const FString ResponseMessage = bAttachmentSucceeded
        ? FString::Printf(TEXT("Added %s to actor: %s"), *DisplayName, *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("%s created but attachment to '%s' failed (volume is static, target may be movable)"), *DisplayName, *TargetActor->GetActorLabel());
    Subsystem->SendAutomationResponse(Socket, RequestId, bAttachmentSucceeded, ResponseMessage, ResponseJson,
        bAttachmentSucceeded ? TEXT("") : TEXT("ATTACHMENT_FAILED"));
}
#endif
}
