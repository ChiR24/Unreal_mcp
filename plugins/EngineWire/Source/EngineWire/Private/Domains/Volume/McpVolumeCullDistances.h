#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;

namespace VolumeHelpers
{
void SetCullDistancesFromPayload(AActor* VolumeActor, const TSharedPtr<FJsonObject>& Payload);
}
