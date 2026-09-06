#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpSkeletonHandlers
{
// Both accept the {x,y,z} / {pitch,yaw,roll} object form and the [x,y,z] /
// [pitch,yaw,roll] array form the capability records document.
FVector ParseVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FVector& Default = FVector::ZeroVector);
FRotator ParseRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FRotator& Default = FRotator::ZeroRotator);
// Overwrites only the components present in the payload: location, rotation
// and scale (a bare number is a uniform scale). Returns how many were applied.
int32 ApplyTransformFieldsFromJson(const TSharedPtr<FJsonObject>& Payload, FTransform& InOutTransform);
// Writes location {x,y,z}, rotation {pitch,yaw,roll} and scale {x,y,z} onto Target.
void WriteTransformToJson(const FTransform& Transform, const TSharedPtr<FJsonObject>& Target);
}
