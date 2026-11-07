#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

// True when Values is exactly Count JSON numbers, each within float range: a vector sent as an array to a parameter
// the gateway cannot type, held to the rule the gateway applies to a typed one. A numeric string ("1") or a missing or
// extra element fails; AsNumber() would read "a" as 0, and the readers pad a short array or leave a fourth number unread.
inline bool McpIsJsonFloatArray(const TArray<TSharedPtr<FJsonValue>>& Values, int32 Count)
{
    return Values.Num() == Count && !Values.ContainsByPredicate([](const TSharedPtr<FJsonValue>& Value)
    {
        return !Value.IsValid() || Value->Type != EJson::Number || !(FMath::Abs(Value->AsNumber()) <= TNumericLimits<float>::Max());
    });
}
