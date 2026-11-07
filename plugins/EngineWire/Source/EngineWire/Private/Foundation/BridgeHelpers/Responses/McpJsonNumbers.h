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

// A vector (or with bColor a color) sent to a parameter the gateway cannot type: an object, whose keys the readers take by
// name, or an array of exactly 3 numbers (3 or 4 for a color). False for anything else, which the loose readers turned
// into the default ([2, 2], "abc") or cut short ([1, 2, 3, 4]) without a word.
inline bool McpIsJsonVectorValue(const TSharedPtr<FJsonValue>& Value, bool bColor = false)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    return Value.IsValid() && (Value->Type == EJson::Object || (Value->TryGetArray(Values) && Values &&
        (McpIsJsonFloatArray(*Values, 3) || (bColor && McpIsJsonFloatArray(*Values, 4)))));
}
