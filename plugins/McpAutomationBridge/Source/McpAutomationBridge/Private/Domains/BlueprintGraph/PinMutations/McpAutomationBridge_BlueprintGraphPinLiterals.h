// McpAutomationBridge_BlueprintGraphPinLiterals.h -- JSON value -> pin default literal
//
// Split out of McpAutomationBridge_BlueprintGraphPinSetDefaultValue.cpp under the
// 250-pure-line gate. Pure string rendering; the handler owns pin resolution.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphPin.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpBlueprintGraphHandlers::PinLiterals
{
/** Renders a JSON scalar as the literal a pin expects (ints stay ints). */
inline FString PinLiteralFromJson(const TSharedPtr<FJsonValue>& Field)
{
    FString Literal;
    return McpJsonScalarToString(Field, Literal) ? Literal : FString();
}

inline double JsonNumberByKeys(const TSharedPtr<FJsonObject>& Object, const TCHAR* Lower,
                               const TCHAR* Upper, double Fallback)
{
    double Value = Fallback;
    if (!Object->TryGetNumberField(Lower, Value))
    {
        Object->TryGetNumberField(Upper, Value);
    }
    return Value;
}

// {x,y,z} / [x,y,z] / {pitch,yaw,roll} / {r,g,b,a} rendered as the literal the
// pin's struct stores. A JSON object used to fall through PinLiteralFromJson as
// an empty string, the schema stored nothing, and because the literal was also
// empty the read-back guard passed it as a success.
inline FString StructPinLiteralFromJson(const TSharedPtr<FJsonValue>& Field, const UEdGraphPin& Pin)
{
    const UScriptStruct* Struct = Cast<UScriptStruct>(Pin.PinType.PinSubCategoryObject.Get());
    const bool bRotator = Struct == TBaseStructure<FRotator>::Get();
    const bool bColor = Struct == TBaseStructure<FLinearColor>::Get();
    TArray<double> N;
    if (Field->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Item : Field->AsArray())
        {
            N.Add(Item.IsValid() ? Item->AsNumber() : 0.0);
        }
    }
    else if (const TSharedPtr<FJsonObject> O = Field->AsObject())
    {
        if (bRotator)
        {
            N = {JsonNumberByKeys(O, TEXT("pitch"), TEXT("Pitch"), 0.0), JsonNumberByKeys(O, TEXT("yaw"), TEXT("Yaw"), 0.0),
                 JsonNumberByKeys(O, TEXT("roll"), TEXT("Roll"), 0.0)};
        }
        else if (bColor)
        {
            N = {JsonNumberByKeys(O, TEXT("r"), TEXT("R"), 0.0), JsonNumberByKeys(O, TEXT("g"), TEXT("G"), 0.0),
                 JsonNumberByKeys(O, TEXT("b"), TEXT("B"), 0.0), JsonNumberByKeys(O, TEXT("a"), TEXT("A"), 1.0)};
        }
        else
        {
            N = {JsonNumberByKeys(O, TEXT("x"), TEXT("X"), 0.0), JsonNumberByKeys(O, TEXT("y"), TEXT("Y"), 0.0),
                 JsonNumberByKeys(O, TEXT("z"), TEXT("Z"), 0.0)};
        }
    }
    if (bColor && N.Num() >= 3)
    {
        return FString::Printf(TEXT("(R=%f,G=%f,B=%f,A=%f)"), N[0], N[1], N[2], N.Num() > 3 ? N[3] : 1.0);
    }
    if (Struct == TBaseStructure<FVector2D>::Get() && N.Num() >= 2)
    {
        return FString::Printf(TEXT("(X=%f,Y=%f)"), N[0], N[1]);
    }
    if ((bRotator || Struct == TBaseStructure<FVector>::Get()) && N.Num() >= 3)
    {
        return FString::Printf(TEXT("%f,%f,%f"), N[0], N[1], N[2]);
    }
    return FString();
}
} // namespace McpBlueprintGraphHandlers::PinLiterals
