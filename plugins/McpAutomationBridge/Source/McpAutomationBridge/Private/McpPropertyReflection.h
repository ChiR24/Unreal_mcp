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
MCPAUTOMATIONBRIDGE_API bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError);
MCPAUTOMATIONBRIDGE_API int32 ApplyJsonValuesToObject(UObject* Object, const TMap<FName, TSharedPtr<FJsonValue>>& JsonValues, TMap<FName, FString>* OutErrors = nullptr);
MCPAUTOMATIONBRIDGE_API int32 ApplyJsonObjectToObject(UObject* Object, const TSharedPtr<FJsonObject>& JsonObject, TMap<FName, FString>* OutErrors = nullptr);
MCPAUTOMATIONBRIDGE_API FString GetPropertyTypeName(FProperty* Property);
MCPAUTOMATIONBRIDGE_API bool IsPropertyTypeSupported(FProperty* Property);
MCPAUTOMATIONBRIDGE_API FString GetPropertyValueAsString(UObject* Object, FProperty* Property);
MCPAUTOMATIONBRIDGE_API bool SetPropertyValueFromString(UObject* Object, FProperty* Property, const FString& ValueString, FString* OutError = nullptr);
MCPAUTOMATIONBRIDGE_API TArray<FString> GetEnumValueNames(UEnum* Enum);
MCPAUTOMATIONBRIDGE_API FString EnumValueToName(UEnum* Enum, int64 Value);
MCPAUTOMATIONBRIDGE_API bool EnumNameToValue(UEnum* Enum, const FString& Name, int64& OutValue);
MCPAUTOMATIONBRIDGE_API int32 GetArrayPropertyCount(void* Container, FArrayProperty* ArrayProp);
MCPAUTOMATIONBRIDGE_API TArray<TSharedPtr<FJsonValue>> ExportArrayToJson(void* Container, FArrayProperty* ArrayProp);
MCPAUTOMATIONBRIDGE_API bool ImportJsonToArray(void* Container, FArrayProperty* ArrayProp, const TArray<TSharedPtr<FJsonValue>>& JsonArray, FString& OutError);

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

inline TSharedPtr<FJsonObject> ColorToJson(const FColor& Color)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("r"), Color.R); Obj->SetNumberField(TEXT("g"), Color.G); Obj->SetNumberField(TEXT("b"), Color.B); Obj->SetNumberField(TEXT("a"), Color.A);
    return Obj;
}

inline bool JsonToColor(const TSharedPtr<FJsonObject>& Obj, FColor& OutColor)
{
    if (!Obj.IsValid()) return false;
    double R = 255.0, G = 255.0, B = 255.0, A = 255.0;
    Obj->TryGetNumberField(TEXT("r"), R); Obj->TryGetNumberField(TEXT("g"), G); Obj->TryGetNumberField(TEXT("b"), B); Obj->TryGetNumberField(TEXT("a"), A);
    OutColor = FColor(static_cast<uint8>(FMath::Clamp(static_cast<int>(R), 0, 255)), static_cast<uint8>(FMath::Clamp(static_cast<int>(G), 0, 255)), static_cast<uint8>(FMath::Clamp(static_cast<int>(B), 0, 255)), static_cast<uint8>(FMath::Clamp(static_cast<int>(A), 0, 255)));
    return true;
}

inline TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("r"), Color.R); Obj->SetNumberField(TEXT("g"), Color.G); Obj->SetNumberField(TEXT("b"), Color.B); Obj->SetNumberField(TEXT("a"), Color.A);
    return Obj;
}

inline bool JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, FLinearColor& OutColor)
{
    if (!Obj.IsValid()) return false;
    double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
    Obj->TryGetNumberField(TEXT("r"), R); Obj->TryGetNumberField(TEXT("g"), G); Obj->TryGetNumberField(TEXT("b"), B); Obj->TryGetNumberField(TEXT("a"), A);
    OutColor = FLinearColor(R, G, B, A);
    return true;
}

inline bool IsArrayProperty(FProperty* Property) { return Property != nullptr && Property->IsA<FArrayProperty>(); }
inline bool IsMapProperty(FProperty* Property) { return Property != nullptr && Property->IsA<FMapProperty>(); }
inline bool IsSetProperty(FProperty* Property) { return Property != nullptr && Property->IsA<FSetProperty>(); }
inline FProperty* GetArrayInnerProperty(FArrayProperty* ArrayProp) { return ArrayProp ? ArrayProp->Inner : nullptr; }
}
