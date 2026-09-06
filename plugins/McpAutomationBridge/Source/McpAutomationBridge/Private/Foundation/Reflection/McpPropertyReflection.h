#pragma once

#include "CoreMinimal.h"
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
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportObjectToJson(UObject* Object, bool bIncludeTransient = false);
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportPropertiesToJson(UObject* Object, const TArray<FName>& PropertyNames);
// A broad export walks every reflected property, so a large CDO answered with
// a payload the gateway then refused as RESULT_TOO_LARGE - advice the caller
// cannot act on, because inspect_cdo has no narrowing parameter other than the
// targeted propertyNames path this deliberately leaves alone.
static constexpr int32 McpMaxBoundedExportProperties = 200;
MCPAUTOMATIONBRIDGE_API TSharedPtr<FJsonObject> ExportObjectToJsonBounded(UObject* Object, bool bIncludeTransient = false, int32 MaxProperties = McpMaxBoundedExportProperties);
MCPAUTOMATIONBRIDGE_API bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError);
// Writes a JSON string/number/bool into a String, Int, Float, Bool or Name property
// value with the lenient coercions the container handlers share; false for other types.
MCPAUTOMATIONBRIDGE_API bool AssignPrimitiveFromJson(FProperty* Property, void* ValuePtr, const TSharedPtr<FJsonValue>& Value);
MCPAUTOMATIONBRIDGE_API FString GetPropertyTypeName(FProperty* Property);
MCPAUTOMATIONBRIDGE_API FString GetPropertyValueAsString(UObject* Object, FProperty* Property);
MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp);

inline FProperty* FindPropertyByName(UObject* Object, const FName& PropertyName)
{
    UClass* Class = Object ? Object->GetClass() : nullptr;
    return Class ? Class->FindPropertyByName(PropertyName) : nullptr;
}

inline TSharedPtr<FJsonValue> VectorToJsonValue(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return MakeShared<FJsonValueArray>(Arr);
}

inline bool JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FVector& OutVector)
{
    if (JsonArray.Num() < 3) return false;
    OutVector = FVector(JsonArray[0]->AsNumber(), JsonArray[1]->AsNumber(), JsonArray[2]->AsNumber());
    return true;
}

inline bool JsonToVector(const TSharedPtr<FJsonObject>& JsonObject, FVector& OutVector)
{
    if (!JsonObject.IsValid()) return false;
    double X = 0.0, Y = 0.0, Z = 0.0;
    if (!JsonObject->TryGetNumberField(TEXT("x"), X)) JsonObject->TryGetNumberField(TEXT("X"), X);
    if (!JsonObject->TryGetNumberField(TEXT("y"), Y)) JsonObject->TryGetNumberField(TEXT("Y"), Y);
    if (!JsonObject->TryGetNumberField(TEXT("z"), Z)) JsonObject->TryGetNumberField(TEXT("Z"), Z);
    OutVector = FVector(X, Y, Z);
    return true;
}

inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), Vector.X); Obj->SetNumberField(TEXT("y"), Vector.Y); Obj->SetNumberField(TEXT("z"), Vector.Z);
    return Obj;
}

inline TSharedPtr<FJsonValue> RotatorToJsonValue(const FRotator& Rotator)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
    Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
    Arr.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
    return MakeShared<FJsonValueArray>(Arr);
}

inline bool JsonArrayToRotator(const TArray<TSharedPtr<FJsonValue>>& JsonArray, FRotator& OutRotator)
{
    if (JsonArray.Num() < 3) return false;
    OutRotator = FRotator(JsonArray[0]->AsNumber(), JsonArray[1]->AsNumber(), JsonArray[2]->AsNumber());
    return true;
}

inline bool JsonToRotator(const TSharedPtr<FJsonObject>& JsonObject, FRotator& OutRotator)
{
    if (!JsonObject.IsValid()) return false;
    double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
    if (!JsonObject->TryGetNumberField(TEXT("pitch"), Pitch)) JsonObject->TryGetNumberField(TEXT("Pitch"), Pitch);
    if (!JsonObject->TryGetNumberField(TEXT("yaw"), Yaw)) JsonObject->TryGetNumberField(TEXT("Yaw"), Yaw);
    if (!JsonObject->TryGetNumberField(TEXT("roll"), Roll)) JsonObject->TryGetNumberField(TEXT("Roll"), Roll);
    OutRotator = FRotator(Pitch, Yaw, Roll);
    return true;
}

inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch); Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw); Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Obj;
}

inline TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("r"), Color.R); Obj->SetNumberField(TEXT("g"), Color.G); Obj->SetNumberField(TEXT("b"), Color.B); Obj->SetNumberField(TEXT("a"), Color.A);
    return Obj;
}

}
