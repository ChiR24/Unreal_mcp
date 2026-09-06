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
