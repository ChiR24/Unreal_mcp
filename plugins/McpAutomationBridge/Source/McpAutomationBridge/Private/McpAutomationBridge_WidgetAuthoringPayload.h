#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace WidgetAuthoringHelpers
{
FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObject, const FLinearColor& Default = FLinearColor::White);
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);
ESlateVisibility GetVisibility(const FString& VisibilityString);
}
