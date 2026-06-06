#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace VolumeHelpers
{
#if WITH_EDITOR
struct FVolumeCreateArgs
{
    FString VolumeName;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
};

FVector GetVectorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FVector Default = FVector::ZeroVector);
FRotator GetRotatorFromPayload(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, FRotator Default = FRotator::ZeroRotator);
bool ValidateVolumeName(const FString& VolumeName, FString& OutError);
bool ValidateExtent(const FVector& Extent, FString& OutError);
bool ValidateRadius(float Radius, FString& OutError);
bool ValidateCapsuleDimensions(float Radius, float HalfHeight, FString& OutError);
bool ValidateLocation(const FVector& Location, FString& OutError);
bool ReadNamedTransform(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& DefaultName, FVolumeCreateArgs& OutArgs);
bool ReadExtent(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& FieldName, const FVector& DefaultExtent, FVector& OutExtent);
#endif
}
