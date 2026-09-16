#include "Domains/Volume/McpAutomationBridge_VolumeGeometry.h"

#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "Components/BrushComponent.h"
#include "Engine/Polys.h"
#include "Model.h"
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
    UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>(Volume);
    CubeBuilder->X = Extent.X * 2.0f;
    CubeBuilder->Y = Extent.Y * 2.0f;
    CubeBuilder->Z = Extent.Z * 2.0f;

    Volume->PreEditChange(nullptr);

    // Build() writes polygons INTO Volume->Brush, so that UModel and its Polys
    // have to exist first. A volume spawned through SpawnActor has neither, so
    // the build had nowhere to write and every volume created over the bridge
    // came out with bounds {0,0,0} — a NavMeshBoundsVolume built that way
    // encloses no navigable area at all, while still reporting success.
    Volume->Brush = NewObject<UModel>(Volume, NAME_None, RF_Transactional);
    Volume->Brush->Initialize(nullptr, true);
    Volume->Brush->Polys = NewObject<UPolys>(Volume->Brush, NAME_None, RF_Transactional);
    Volume->BrushBuilder = DuplicateObject<UBrushBuilder>(CubeBuilder, Volume);
    if (UBrushComponent* BrushComponent = Volume->GetBrushComponent())
    {
        BrushComponent->Brush = Volume->Brush;
    }

    CubeBuilder->Build(Volume->GetWorld(), Volume);
    // BuildBound() is what actually populates UModel::Bounds; UBrushComponent
    // derives its own bounds from that, so without it the component keeps the
    // zero bound it was spawned with even though the polys now exist.
    Volume->Brush->BuildBound();
    if (UBrushComponent* Built = Volume->GetBrushComponent())
    {
        Built->UpdateBounds();
        Built->MarkRenderStateDirty();
    }

    Volume->PostEditChange();
    Volume->MarkPackageDirty();
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
