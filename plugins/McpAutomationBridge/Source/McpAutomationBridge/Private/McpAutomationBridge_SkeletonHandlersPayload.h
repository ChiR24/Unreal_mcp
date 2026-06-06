#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpersJsonFields.h"

#define GetStringFieldSkel GetJsonStringField
#define GetNumberFieldSkel GetJsonNumberField
#define GetBoolFieldSkel GetJsonBoolField

namespace McpSkeletonHandlers
{
int32 GetIntFieldSkel(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, int32 DefaultValue);
FVector ParseVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FVector& Default = FVector::ZeroVector);
FRotator ParseRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FRotator& Default = FRotator::ZeroRotator);
}
