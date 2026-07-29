#pragma once

#include "CoreMinimal.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

namespace McpCombatHandlers
{
inline FVector GetVectorFromJsonCombat(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }

    return FVector(
        GetJsonNumberField(Obj, TEXT("x"), 0.0),
        GetJsonNumberField(Obj, TEXT("y"), 0.0),
        GetJsonNumberField(Obj, TEXT("z"), 0.0));
}
}
