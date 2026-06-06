#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;

namespace VolumeHelpers
{
#if WITH_EDITOR
void SetCullDistancesFromPayload(AActor* VolumeActor, const TSharedPtr<FJsonObject>& Payload);
#endif
}
