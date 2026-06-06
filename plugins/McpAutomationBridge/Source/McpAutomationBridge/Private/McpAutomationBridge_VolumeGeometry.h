#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Engine/Brush.h"
#include "Engine/World.h"
#endif

namespace VolumeHelpers
{
#if WITH_EDITOR
bool CreateBoxBrushForVolume(ABrush* Volume, const FVector& Extent);
void SetVolumeExtentGeometry(AActor* VolumeActor, const FVector& Extent);

template<typename TVolumeClass>
TVolumeClass* SpawnVolumeActor(
    UWorld* World,
    const FString& VolumeName,
    const FVector& Location,
    const FRotator& Rotation,
    const FVector& Extent)
{
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TVolumeClass* Volume = World->SpawnActor<TVolumeClass>(Location, Rotation, SpawnParams);
    if (Volume)
    {
        if (!VolumeName.IsEmpty())
        {
            Volume->SetActorLabel(VolumeName);
        }
        if (Extent != FVector::ZeroVector)
        {
            if constexpr (TIsDerivedFrom<TVolumeClass, ABrush>::IsDerived)
            {
                CreateBoxBrushForVolume(Volume, Extent);
            }
        }
    }
    return Volume;
}
#endif
}
