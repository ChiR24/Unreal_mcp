#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FProperty;
class UClass;

namespace WidgetAuthoringHelpers
{
FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObject, const FLinearColor& Default = FLinearColor::White);
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);
ESlateVisibility GetVisibility(const FString& VisibilityString);
// Resolves the widget's real style property; see the definition for why "Style"
// alone is not enough.
FProperty* FindWidgetStyleProperty(const UClass* WidgetClass);
}
