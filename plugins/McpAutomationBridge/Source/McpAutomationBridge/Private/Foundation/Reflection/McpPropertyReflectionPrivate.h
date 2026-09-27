#pragma once

#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Internationalization/Text.h"
#include "Runtime/Launch/Resources/Version.h"
#if __has_include("UObject/StrProperty.h")
#include "UObject/StrProperty.h"
#endif
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace McpPropertyReflection
{
namespace Private
{
FString ExportTextToJsonString(const FText& TextValue);
FText ImportTextFromJsonString(const FString& TextValue);
// One container element as JSON; export text for a type ExportPropertyToJsonValue has no form for.
TSharedPtr<FJsonValue> ExportElementToJsonValue(FProperty* Inner, void* ElemPtr);
TSharedPtr<FJsonValue> ExportMapToJsonValue(void* TargetContainer, FMapProperty* MapProp);
TSharedPtr<FJsonValue> ExportSetToJsonValue(void* TargetContainer, FSetProperty* SetProp);
bool TryImportNormalizedColor(const TSharedPtr<FJsonObject>& Object, FColor& OutColor);
}
}
