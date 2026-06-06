#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpHandlerUtils
{
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

inline TSharedPtr<FJsonObject> TransformToJson(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetObjectField(TEXT("location"), VectorToJson(Transform.GetTranslation()));
    Obj->SetObjectField(TEXT("rotation"), RotatorToJson(Transform.Rotator()));
    Obj->SetObjectField(TEXT("scale"), VectorToJson(Transform.GetScale3D()));
    return Obj;
}

inline bool JsonToVector(const TSharedPtr<FJsonObject>& Obj, FVector& OutVector)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    Obj->TryGetNumberField(TEXT("x"), X);
    Obj->TryGetNumberField(TEXT("y"), Y);
    Obj->TryGetNumberField(TEXT("z"), Z);
    if (!Obj->HasField(TEXT("x")))
    {
        Obj->TryGetNumberField(TEXT("X"), X);
        Obj->TryGetNumberField(TEXT("Y"), Y);
        Obj->TryGetNumberField(TEXT("Z"), Z);
    }
    OutVector = FVector(X, Y, Z);
    return true;
}

inline bool JsonToRotator(const TSharedPtr<FJsonObject>& Obj, FRotator& OutRotator)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    double Pitch = 0.0;
    double Yaw = 0.0;
    double Roll = 0.0;
    Obj->TryGetNumberField(TEXT("pitch"), Pitch);
    Obj->TryGetNumberField(TEXT("yaw"), Yaw);
    Obj->TryGetNumberField(TEXT("roll"), Roll);
    if (!Obj->HasField(TEXT("pitch")))
    {
        Obj->TryGetNumberField(TEXT("Pitch"), Pitch);
        Obj->TryGetNumberField(TEXT("Yaw"), Yaw);
        Obj->TryGetNumberField(TEXT("Roll"), Roll);
    }
    OutRotator = FRotator(Pitch, Yaw, Roll);
    return true;
}

inline bool JsonToLinearColor(const TSharedPtr<FJsonObject>& Obj, FLinearColor& OutColor)
{
    if (!Obj.IsValid())
    {
        return false;
    }
    double R = 1.0;
    double G = 1.0;
    double B = 1.0;
    double A = 1.0;
    Obj->TryGetNumberField(TEXT("r"), R);
    Obj->TryGetNumberField(TEXT("g"), G);
    Obj->TryGetNumberField(TEXT("b"), B);
    Obj->TryGetNumberField(TEXT("a"), A);
    OutColor = FLinearColor(R, G, B, A);
    return true;
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
