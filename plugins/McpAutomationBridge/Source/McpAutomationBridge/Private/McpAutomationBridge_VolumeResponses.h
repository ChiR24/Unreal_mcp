#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;

namespace VolumeHelpers
{
#if WITH_EDITOR
TSharedPtr<FJsonObject> CreateVectorObject(const FVector& Value);
TSharedPtr<FJsonObject> CreateVolumeResponse(AActor* Volume, const FString& VolumeClass);
#endif
}
