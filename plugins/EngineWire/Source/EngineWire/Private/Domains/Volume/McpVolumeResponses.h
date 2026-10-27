#pragma once

#include "CoreMinimal.h"

class AActor;
class FJsonObject;

namespace VolumeHelpers
{
TSharedPtr<FJsonObject> CreateVectorObject(const FVector& Value);
TSharedPtr<FJsonObject> CreateVolumeResponse(AActor* Volume, const FString& VolumeClass);
}
