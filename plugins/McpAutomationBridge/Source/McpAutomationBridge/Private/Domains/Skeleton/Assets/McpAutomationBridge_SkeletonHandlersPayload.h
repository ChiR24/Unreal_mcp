#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

class UObject;

namespace McpSkeletonHandlers
{
// Overwrites only the components present in the payload: location, rotation
// and scale (a bare number is a uniform scale). Returns how many were applied.
int32 ApplyTransformFieldsFromJson(const TSharedPtr<FJsonObject>& Payload, FTransform& InOutTransform);
// Writes location {x,y,z}, rotation {pitch,yaw,roll} and scale {x,y,z} onto Target.
void WriteTransformToJson(const FTransform& Transform, const TSharedPtr<FJsonObject>& Target);
// Saves Asset unless the request sent save:false. The default is true: an edit
// left only in memory is silently lost when the editor restarts. True when the
// save succeeded or was not requested.
bool SaveIfRequested(UObject* Asset, const TSharedPtr<FJsonObject>& Payload);
}
