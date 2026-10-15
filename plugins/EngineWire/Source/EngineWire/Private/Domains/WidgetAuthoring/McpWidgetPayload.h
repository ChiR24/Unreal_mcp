#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FProperty;
class UClass;

namespace WidgetAuthoringHelpers
{
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);
// Unknown names read as Visible; TryParseVisibility says whether the name was one of the five.
ESlateVisibility GetVisibility(const FString& VisibilityString);
bool TryParseVisibility(const FString& Name, ESlateVisibility& Out);
// Resolves the widget's real style property; see the definition for why "Style"
// alone is not enough.
FProperty* FindWidgetStyleProperty(const UClass* WidgetClass);
}
