#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Value to the nearest 1 / Scale (Scale 10: tenths). Divided by Scale, not multiplied by 1 / Scale: 594 x 0.1 is
// 59.400000000000006, 594 / 10 is 59.4.
inline double McpRoundTo(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

namespace McpHandlerUtils
{
// Transform math leaves noise in the last digits: a 90 degree yaw read 89.99999999999999 and a camera y
// 1710.5776600334307. Ten significant digits keep every real position and angle, and the JSON writer prints the
// rounded value in its shortest form (90, 1710.57766). Significant digits keep the noise of a zero (a roll of
// -2.772563934e-15, a bone at y -6.255447271e-9), so a magnitude under a millionth (10 nm, a millionth of a degree) is 0.
inline double ReadableDouble(double Value)
{
    if (!FMath::IsFinite(Value))
    {
        return Value;
    }
    return FMath::Abs(Value) < 1e-6 ? 0.0 : FCString::Atod(*FString::Printf(TEXT("%.10g"), Value));
}

// A float as it was typed: widened to double, 2.2 read 2.200000047683716. Seven significant digits, all a float holds.
inline double ReadableFloat(float Value)
{
    return FCString::Atod(*FString::Printf(TEXT("%.7g"), Value));
}

// The [x, y, z] array shape the actor get/transform/spawn replies use.
inline TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    return {MakeShared<FJsonValueNumber>(ReadableDouble(Vector.X)), MakeShared<FJsonValueNumber>(ReadableDouble(Vector.Y)),
            MakeShared<FJsonValueNumber>(ReadableDouble(Vector.Z))};
}

// VectorToJsonArray of the vector with each axis to the nearest 1 / Scale (Scale 10: tenths).
inline TArray<TSharedPtr<FJsonValue>> RoundedVectorToJsonArray(const FVector& Vector, double Scale = 10.0)
{
    return VectorToJsonArray(
        FVector(McpRoundTo(Vector.X, Scale), McpRoundTo(Vector.Y, Scale), McpRoundTo(Vector.Z, Scale)));
}

// [pitch, yaw, roll], the array counterpart of RotatorToJson.
inline TArray<TSharedPtr<FJsonValue>> RotatorToJsonArray(const FRotator& Rotator)
{
    return {MakeShared<FJsonValueNumber>(ReadableDouble(Rotator.Pitch)), MakeShared<FJsonValueNumber>(ReadableDouble(Rotator.Yaw)),
            MakeShared<FJsonValueNumber>(ReadableDouble(Rotator.Roll))};
}

inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), ReadableDouble(Vector.X));
    Obj->SetNumberField(TEXT("y"), ReadableDouble(Vector.Y));
    Obj->SetNumberField(TEXT("z"), ReadableDouble(Vector.Z));
    return Obj;
}

inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), ReadableDouble(Rotator.Pitch));
    Obj->SetNumberField(TEXT("yaw"), ReadableDouble(Rotator.Yaw));
    Obj->SetNumberField(TEXT("roll"), ReadableDouble(Rotator.Roll));
    return Obj;
}

// {location {x,y,z}, rotation {pitch,yaw,roll}, scale {x,y,z}}.
inline TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetLocation()));
    Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.GetRotation().Rotator()));
    Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
    return Obj;
}

inline TSharedPtr<FJsonObject> LinearColorToJson(const FLinearColor& Color)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("r"), ReadableFloat(Color.R));
    Obj->SetNumberField(TEXT("g"), ReadableFloat(Color.G));
    Obj->SetNumberField(TEXT("b"), ReadableFloat(Color.B));
    Obj->SetNumberField(TEXT("a"), ReadableFloat(Color.A));
    return Obj;
}
}
