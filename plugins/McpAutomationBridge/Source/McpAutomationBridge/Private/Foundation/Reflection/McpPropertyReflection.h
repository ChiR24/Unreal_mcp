#pragma once

#include "CoreMinimal.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

namespace McpPropertyReflection
{
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonValue> ExportPropertyToJsonValue(void* TargetContainer, FProperty* Property);
// The echo of a write: a struct, or an array of them, shows its editable fields only (see the .cpp).
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonValue> ExportWrittenValueToJson(void* TargetContainer, FProperty* Property);
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportPropertiesToJson(UObject* Object, const TArray<FName>& PropertyNames);
// A broad export walks every reflected property, so a large CDO answered with
// a payload the gateway then refused as RESULT_TOO_LARGE - advice the caller
// cannot act on, because inspect_cdo has no narrowing parameter other than the
// targeted propertyNames path this deliberately leaves alone.
static constexpr int32 McpMaxBoundedExportProperties = 200;
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportObjectToJsonBounded(UObject* Object, bool bIncludeTransient = false, int32 MaxProperties = McpMaxBoundedExportProperties);
MCPAUTOMATIONBRIDGE_API bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError);
MCPAUTOMATIONBRIDGE_API FString GetPropertyTypeName(FProperty* Property);
MCPAUTOMATIONBRIDGE_API FString GetPropertyValueAsString(UObject* Object, FProperty* Property);
MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp);

inline FProperty* FindPropertyByName(UObject* Object, const FName& PropertyName)
{
    UClass* Class = Object ? Object->GetClass() : nullptr;
    return Class ? Class->FindPropertyByName(PropertyName) : nullptr;
}

}
