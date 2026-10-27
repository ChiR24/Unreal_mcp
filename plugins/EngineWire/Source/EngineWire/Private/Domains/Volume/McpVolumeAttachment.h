#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;

namespace VolumeHelpers
{
struct FVolumeAttachmentArgs
{
    UWorld* World = nullptr;
    AActor* TargetActor = nullptr;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Extent = FVector::ZeroVector;
};

bool ResolveAttachmentTarget(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FVector& DefaultExtent, FVolumeAttachmentArgs& OutArgs);
bool AttachVolumeToTarget(AActor* VolumeActor, AActor* TargetActor);
void SendAttachedVolumeResponse(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload, AActor* TargetActor, AActor* VolumeActor, TSharedPtr<FJsonObject> ResponseJson, const FString& DisplayName, bool bAttachmentSucceeded);
}
