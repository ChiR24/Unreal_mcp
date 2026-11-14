#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Reflection/McpEnumName.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

namespace McpGASHandlers
{
static inline FString NormalizeGASToken(FString Value)
{
    Value.TrimStartAndEndInline();
    FString Normalized = Value.ToLower();
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    return Normalized;
}

}
