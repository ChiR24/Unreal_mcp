#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include <initializer_list>

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
// name (a color's with a number under r or x, which an empty object lacked and read as the default), or an array of
// exactly 3 numbers (3 or 4 for a color). False for anything else, which the loose readers turned into the default
// ([2, 2], "abc") or cut short ([1, 2, 3, 4]) without a word.
inline bool McpIsJsonVectorValue(const TSharedPtr<FJsonValue>& Value, bool bColor = false)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    const TSharedPtr<FJsonObject>* Object = nullptr;
    double First = 0.0;
    return Value.IsValid() &&
        ((Value->TryGetObject(Object) && Object && (!bColor || (*Object)->TryGetNumberField(TEXT("r"), First) ||
                                                    (*Object)->TryGetNumberField(TEXT("x"), First))) ||
         (Value->TryGetArray(Values) && Values && (McpIsJsonFloatArray(*Values, 3) || (bColor && McpIsJsonFloatArray(*Values, 4)))));
}

// The first of Fields holding a number, into OutValue; false, OutValue untouched, when none does.
inline bool McpGetFirstNumberField(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Fields, double& OutValue)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    for (const TCHAR* Field : Fields)
    {
        double Value = 0.0;
        if (Obj->TryGetNumberField(Field, Value))
        {
            OutValue = Value;
            return true;
        }
    }
    return false;
}
