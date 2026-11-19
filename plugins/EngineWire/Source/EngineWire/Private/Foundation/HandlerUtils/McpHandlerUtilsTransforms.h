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
// rounded value in its shortest form (90, 1710.57766). They keep the noise of a zero too (a bone at y -6.255447271e-9),
// so a value under 1e-8 reads 0: no Unreal length, angle, speed or mass is that small. A cut-off of 1e-6 read a velocity
// change of 5e-7 as nothing.
inline double ReadableDouble(double Value)
{
    if (!FMath::IsFinite(Value))
    {
        return Value;
    }
    return FMath::Abs(Value) < 1e-8 ? 0.0 : FCString::Atod(*FString::Printf(TEXT("%.10g"), Value));
}

// A vector's or rotator's axes to ten significant digits of the largest one: an axis under a billionth of it is the
// noise of a zero beside it (a light's roll of -2.772563934e-15 beside its pitch) and reads 0.
inline FVector ReadableAxes(double X, double Y, double Z)
{
    const auto Size = [](double Value) { return FMath::IsFinite(Value) ? FMath::Abs(Value) : 0.0; };
    const double Noise = FMath::Max3(Size(X), Size(Y), Size(Z)) * 1e-9;
    const auto Axis = [Noise](double Value) { return FMath::Abs(Value) < Noise ? 0.0 : ReadableDouble(Value); };
    return FVector(Axis(X), Axis(Y), Axis(Z));
}

// A float as it was typed: widened to double, 2.2 read 2.200000047683716. Seven significant digits, all a float holds.
inline double ReadableFloat(float Value)
{
    return FCString::Atod(*FString::Printf(TEXT("%.7g"), Value));
}

// The [x, y, z] array shape the actor get/transform/spawn replies use.
inline TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    const FVector Axes = ReadableAxes(Vector.X, Vector.Y, Vector.Z);
    return {MakeShared<FJsonValueNumber>(Axes.X), MakeShared<FJsonValueNumber>(Axes.Y), MakeShared<FJsonValueNumber>(Axes.Z)};
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
    const FVector Axes = ReadableAxes(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
    return {MakeShared<FJsonValueNumber>(Axes.X), MakeShared<FJsonValueNumber>(Axes.Y), MakeShared<FJsonValueNumber>(Axes.Z)};
}

inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
{
    const FVector Axes = ReadableAxes(Vector.X, Vector.Y, Vector.Z);
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), Axes.X);
    Obj->SetNumberField(TEXT("y"), Axes.Y);
    Obj->SetNumberField(TEXT("z"), Axes.Z);
    return Obj;
}

inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
{
    const FVector Axes = ReadableAxes(Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), Axes.X);
    Obj->SetNumberField(TEXT("yaw"), Axes.Y);
    Obj->SetNumberField(TEXT("roll"), Axes.Z);
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
