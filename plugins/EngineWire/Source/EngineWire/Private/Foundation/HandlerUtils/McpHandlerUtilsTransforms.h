#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace McpHandlerUtils
{
// Transform math leaves noise in the last digits: a 90 degree yaw read 89.99999999999999 and a camera y
// 1710.5776600334307. Ten significant digits keep every real position and angle, and the JSON writer prints the
// rounded value in its shortest form (90, 1710.57766).
inline double ReadableDouble(double Value)
{
    return FMath::IsFinite(Value) ? FCString::Atod(*FString::Printf(TEXT("%.10g"), Value)) : Value;
}

// The [x, y, z] array shape the actor get/transform/spawn replies use.
inline TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    return {MakeShared<FJsonValueNumber>(ReadableDouble(Vector.X)), MakeShared<FJsonValueNumber>(ReadableDouble(Vector.Y)),
            MakeShared<FJsonValueNumber>(ReadableDouble(Vector.Z))};
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
    Obj->SetNumberField(TEXT("r"), Color.R);
    Obj->SetNumberField(TEXT("g"), Color.G);
    Obj->SetNumberField(TEXT("b"), Color.B);
    Obj->SetNumberField(TEXT("a"), Color.A);
    return Obj;
}
}
