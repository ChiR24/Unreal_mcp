#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace McpHandlerUtils
{
// The [x, y, z] array shape the actor get/transform/spawn replies use.
inline TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    return {MakeShared<FJsonValueNumber>(Vector.X), MakeShared<FJsonValueNumber>(Vector.Y),
            MakeShared<FJsonValueNumber>(Vector.Z)};
}

// [pitch, yaw, roll], the array counterpart of RotatorToJson.
inline TArray<TSharedPtr<FJsonValue>> RotatorToJsonArray(const FRotator& Rotator)
{
    return {MakeShared<FJsonValueNumber>(Rotator.Pitch), MakeShared<FJsonValueNumber>(Rotator.Yaw),
            MakeShared<FJsonValueNumber>(Rotator.Roll)};
}

inline TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("x"), Vector.X);
    Obj->SetNumberField(TEXT("y"), Vector.Y);
    Obj->SetNumberField(TEXT("z"), Vector.Z);
    return Obj;
}

inline TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
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
