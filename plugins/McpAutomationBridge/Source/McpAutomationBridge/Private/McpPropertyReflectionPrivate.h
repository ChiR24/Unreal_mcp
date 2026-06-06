#pragma once

#include "McpPropertyReflection.h"
#include "McpVersionCompatibility.h"
#include "Internationalization/Text.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/StrProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace McpPropertyReflection
{
namespace Private
{
FString ExportTextToJsonString(const FText& TextValue);
FText ImportTextFromJsonString(const FString& TextValue);
TSharedPtr<FJsonValue> ExportMapToJsonValue(void* TargetContainer, FMapProperty* MapProp);
TSharedPtr<FJsonValue> ExportSetToJsonValue(void* TargetContainer, FSetProperty* SetProp);
}
}
