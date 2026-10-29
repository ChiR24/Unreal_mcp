// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;

// set_material's color: a flat colour ("#rrggbb", "#rgb" or a name) as a material instance of the engine's basic shape
// material, /Game/Materials/Colors/MI_Color_RRGGBB, made and saved the first time and reused by every later call. Its
// glow: the colour (white when none is given) glowing at that strength, an instance MI_Glow_RRGGBB_G<glow x 10> of
// M_ColorGlow in the same folder (base colour Color, emissive Color x Glow), made once.
namespace McpColorMaterial
{
/** A payload with color or glow comes back as a copy naming that instance in materialPath (made when missing, and only
 *  once an actor it is for resolves through Find, so a refused call leaves no asset); one with neither comes back as it
 *  is. False with ErrorCode and Error when the colour does not read, the glow is out of range, no actor resolves or the
 *  instance cannot be made. */
bool Resolve(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutPayload, FString& ErrorCode,
             FString& Error, TFunctionRef<AActor*(const FString&)> Find);
}
