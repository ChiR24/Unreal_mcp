#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace LevelStructureHelpers
{
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FVector Default = FVector::ZeroVector);
FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FRotator Default = FRotator::ZeroRotator);
}
