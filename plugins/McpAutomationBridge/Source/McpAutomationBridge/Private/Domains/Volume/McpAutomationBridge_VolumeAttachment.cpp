#include "Domains/Volume/McpAutomationBridge_VolumeAttachment.h"

#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Brush.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"
#include "Domains/Volume/McpAutomationBridge_VolumeRequestParsing.h"
#include "Domains/Volume/McpAutomationBridge_VolumeWorldResolution.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/EngineVersionComparison.h"

namespace VolumeHelpers
{
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

void SendAttachedVolumeResponse(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload, AActor* TargetActor, AActor* VolumeActor, TSharedPtr<FJsonObject> ResponseJson, const FString& DisplayName, bool bAttachmentSucceeded)
{
    ResponseJson->SetStringField(TEXT("attachedTo"), TargetActor->GetActorLabel());
    ResponseJson->SetBoolField(TEXT("attachmentSucceeded"), bAttachmentSucceeded);
    McpHandlerUtils::AddVerification(ResponseJson, VolumeActor);
    const FString ResponseMessage = bAttachmentSucceeded
        ? FString::Printf(TEXT("Added %s to actor: %s"), *DisplayName, *TargetActor->GetActorLabel())
        : FString::Printf(TEXT("%s created but attachment to '%s' failed (volume is static, target may be movable)"), *DisplayName, *TargetActor->GetActorLabel());
    if (bAttachmentSucceeded)
    {
        LevelStructureHelpers::SendLevelEditResult(Subsystem, RequestId, Socket, Payload, VolumeActor->GetLevel(), ResponseMessage, ResponseJson);
        return;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, false, ResponseMessage, ResponseJson, TEXT("ATTACHMENT_FAILED"));
}
}
