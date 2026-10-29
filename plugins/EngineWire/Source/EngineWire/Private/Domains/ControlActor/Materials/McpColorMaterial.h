// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;

// set_material's color: a flat colour ("#rrggbb", "#rgb" or a name) as a material instance of the engine's basic shape
// material, /Game/Materials/Colors/MI_Color_RRGGBB, made and saved the first time and reused by every later call.
namespace McpColorMaterial
{
/** A payload with color comes back as a copy naming that instance in materialPath (made when missing, and only once an
 *  actor it is for resolves through Find, so a refused call leaves no asset); one without color comes back as it is.
 *  False with ErrorCode and Error when the colour does not read, no actor resolves or the instance cannot be made. */
bool Resolve(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutPayload, FString& ErrorCode,
             FString& Error, TFunctionRef<AActor*(const FString&)> Find);
}
