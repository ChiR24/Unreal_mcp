#include "McpAutomationBridge_VolumeGeometry.h"

#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "UObject/Package.h"
#endif

namespace VolumeHelpers
{
#if WITH_EDITOR
bool CreateBoxBrushForVolume(ABrush* Volume, const FVector& Extent)
{
    if (!Volume)
    {
        return false;
    }
    UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(GetTransientPackage());
    CubeBuilder->X = Extent.X * 2.0f;
    CubeBuilder->Y = Extent.Y * 2.0f;
    CubeBuilder->Z = Extent.Z * 2.0f;
    CubeBuilder->Build(Volume->GetWorld(), Volume);
    return true;
}

void SetVolumeExtentGeometry(AActor* VolumeActor, const FVector& Extent)
{
    if (ABrush* BrushVolume = Cast<ABrush>(VolumeActor))
    {
        CreateBoxBrushForVolume(BrushVolume, Extent);
        return;
    }
    if (VolumeActor)
    {
        VolumeActor->SetActorScale3D(FVector(Extent.X / 100.0f, Extent.Y / 100.0f, Extent.Z / 100.0f));
    }
}
#endif
}
