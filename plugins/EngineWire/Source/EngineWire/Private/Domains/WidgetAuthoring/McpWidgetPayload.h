#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

class FProperty;
class UClass;

namespace WidgetAuthoringHelpers
{
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);
// One of the five visibility names, in any case, into Out; false for any other.
bool TryParseVisibility(const FString& Name, ESlateVisibility& Out);
// Resolves the widget's real style property; see the definition for why "Style"
// alone is not enough.
FProperty* FindWidgetStyleProperty(const UClass* WidgetClass);
}
