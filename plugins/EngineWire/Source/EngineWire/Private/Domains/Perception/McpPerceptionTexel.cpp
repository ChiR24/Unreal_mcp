// Copyright (c) 2024 EngineWire Contributors
//
// audit_materials' texel_density and instancing checks. Texel density as the texture streamer reckons it: a texture's
// size times the material's sampling scale for it (its UV tiling) over the mesh's UV density for that material slot and
// UV channel (world cm per UV unit) times the component's largest scale; each slot in view against the view's median.
// Instancing: static mesh actors of the level that draw one mesh with one set of materials, the groups worth instancing.

#include "Domains/Perception/McpPerception.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 2
#include "Components.h" // FMeshUVChannelInfo, in MeshUVChannelInfo.h from 5.2
#else
#include "MeshUVChannelInfo.h"
#endif

namespace McpPerception
{
namespace
{
constexpr double TexelOutlier = 4.0; // this many times the view's median, or this many times under it
constexpr int32 TexelMaxRows = 40;
constexpr int32 InstancingMinCopies = 8;
constexpr int32 InstancingMaxGroups = 20;
constexpr int32 InstancingNamesShown = 10;

struct FTexelSlot
{
    const AActor* Actor = nullptr;
    int32 Slot = 0;
    const UMaterialInterface* Material = nullptr;
    const UTexture* Texture = nullptr;
    double PerMetre = 0.0;
    bool bApproximate = true; // the material has no streaming data for the texture: UV 0, untiled, assumed
};

TArray<UTexture*> TexelTexturesOf(const UMaterialInterface* Material)
{
    TArray<UTexture*> Textures;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 7
    Material->GetUsedTextures(Textures); // the five-argument form is deprecated from 5.7 with an empty body: it finds none
#else
    Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
#endif
    return Textures;
}

// Each material slot of the actor's static meshes, at the density of its sharpest 2D texture.
void TexelSlotsOf(const AActor* Actor, TArray<FTexelSlot>& Out)
{
    TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
    for (const UStaticMeshComponent* Component : Components)
    {
        const UStaticMesh* Mesh = Component->GetStaticMesh();
        const double Scale = Component->GetComponentTransform().GetMaximumAxisScale();
        for (int32 Slot = 0; Mesh && Scale > 0.0 && Slot < Component->GetNumMaterials(); ++Slot)
        {
            const FMeshUVChannelInfo* Uv = Mesh->GetUVChannelData(Slot);
            const UMaterialInterface* Material = Component->GetMaterial(Slot);
            FTexelSlot Best;
            for (const UTexture* Texture : Uv && Uv->bInitialized && Material ? TexelTexturesOf(Material) : TArray<UTexture*>())
            {
                const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
                double Sampling = 1.0;
                int32 Channel = 0, Lower = 0, Upper = -1;
                bool bUsed = false;
                const bool bKnown = Texture2D && Material->FindTextureStreamingDataIndexRange(Texture->GetFName(), Lower, Upper);
                for (int32 Entry = Lower; bKnown && Entry <= Upper; ++Entry) // the most finely sampled use of the texture
                {
                    const FMaterialTextureInfo& Info = Material->GetTextureStreamingData()[Entry];
                    if (Info.IsValid() && (!bUsed || Info.SamplingScale > Sampling))
                    {
                        bUsed = true;
                        Sampling = Info.SamplingScale;
                        Channel = FMath::Clamp(Info.UVChannelIndex, 0, int32(UE_ARRAY_COUNT(Uv->LocalUVDensities)) - 1);
                    }
                }
                const double Density = Texture2D ? Uv->LocalUVDensities[Channel] : 0.0;
                const double PerMetre = Density > 0.0 ? 100.0 * FMath::Max(Texture2D->GetSurfaceWidth(), Texture2D->GetSurfaceHeight()) * Sampling / (Density * Scale) : 0.0;
                if (PerMetre > Best.PerMetre)
                {
                    Best = {Actor, Slot, Material, Texture2D, PerMetre, !bUsed};
                }
            }
            if (Best.Texture)
            {
                Out.Add(Best);
            }
        }
    }
}
} // namespace

int32 TexelVerdict(double Ratio)
{
    return Ratio >= TexelOutlier ? 1 : Ratio <= 1.0 / TexelOutlier ? -1 : 0;
}

TSharedPtr<FJsonObject> TexelDensity(const FFrame& Frame)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    TSet<const AActor*> InView;
    for (const AActor* Actor : Frame.Actor)
    {
        InView.Add(Actor);
    }
    TArray<FTexelSlot> Slots;
    for (const AActor* Actor : InView)
    {
        if (Actor)
        {
            TexelSlotsOf(Actor, Slots);
        }
    }
    if (Slots.Num() == 0)
    {
        Out->SetStringField(TEXT("note"), TEXT("no static mesh in view draws a 2D texture through UV data the mesh holds"));
        return Out;
    }
    // Only a slot whose material says how it samples the texture is judged, and the median is theirs when there are any:
    // a world-aligned material (the engine's grid) has no such data, and its mesh UVs say nothing of its density.
    TArray<double> Judged, All;
    for (const FTexelSlot& Slot : Slots)
    {
        (Slot.bApproximate ? All : Judged).Add(Slot.PerMetre);
    }
    All.Append(Judged);
    TArray<double>& Basis = Judged.Num() > 0 ? Judged : All;
    Basis.Sort();
    const double Median = Basis[Basis.Num() / 2];
    Slots.StableSort([Median](const FTexelSlot& L, const FTexelSlot& R) { return FMath::Abs(FMath::Loge(L.PerMetre / Median)) > FMath::Abs(FMath::Loge(R.PerMetre / Median)); });
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Outliers = 0;
    for (const FTexelSlot& Slot : Slots)
    {
        const int32 Verdict = Slot.bApproximate ? 0 : TexelVerdict(Slot.PerMetre / Median);
        Outliers += Verdict != 0 ? 1 : 0;
        if (Rows.Num() >= TexelMaxRows)
        {
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), Slot.Actor->GetActorLabel());
        Row->SetNumberField(TEXT("slot"), Slot.Slot);
        Row->SetStringField(TEXT("material"), Slot.Material->GetPathName());
        Row->SetStringField(TEXT("texture"), Slot.Texture->GetPathName());
        Row->SetNumberField(TEXT("texelsPerMetre"), FMath::RoundToDouble(Slot.PerMetre));
        Row->SetNumberField(TEXT("ratio"), FMath::RoundToDouble(Slot.PerMetre / Median * 100.0) / 100.0);
        if (Verdict != 0)
        {
            Row->SetStringField(TEXT("finding"), Verdict > 0 ? TEXT("too_sharp") : TEXT("too_blurry"));
        }
        if (Slot.bApproximate)
        {
            Row->SetBoolField(TEXT("approximate"), true);
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetNumberField(TEXT("medianTexelsPerMetre"), FMath::RoundToDouble(Median));
    Out->SetNumberField(TEXT("slots"), Slots.Num());
    Out->SetNumberField(TEXT("judged"), Judged.Num());
    Out->SetNumberField(TEXT("outliers"), Outliers);
    Out->SetArrayField(TEXT("rows"), Rows);
    return Out;
}

TSharedPtr<FJsonObject> InstancingGroups(UWorld* World)
{
    TMap<FString, TArray<const AStaticMeshActor*>> Groups;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
        if (!Component || !Component->GetStaticMesh())
        {
            continue;
        }
        // One level, one mobility, one mesh, the same material in every slot: what one instanced component could draw.
        FString Key = FString::Printf(TEXT("%s|%d|%s"), *It->GetLevel()->GetPathName(), int32(Component->Mobility), *Component->GetStaticMesh()->GetPathName());
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            const UMaterialInterface* Material = Component->GetMaterial(Slot);
            Key += TEXT("|") + (Material ? Material->GetPathName() : FString());
        }
        Groups.FindOrAdd(Key).Add(*It);
    }
    Groups.ValueStableSort([](const TArray<const AStaticMeshActor*>& L, const TArray<const AStaticMeshActor*>& R) { return L.Num() > R.Num(); });
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Copies = 0;
    for (const TPair<FString, TArray<const AStaticMeshActor*>>& Group : Groups)
    {
        if (Group.Value.Num() < InstancingMinCopies || Rows.Num() >= InstancingMaxGroups)
        {
            continue;
        }
        Copies += Group.Value.Num();
        const UStaticMeshComponent* First = Group.Value[0]->GetStaticMeshComponent();
        TArray<TSharedPtr<FJsonValue>> Names;
        for (int32 At = 0; At < Group.Value.Num() && At < InstancingNamesShown; ++At)
        {
            Names.Add(MakeShared<FJsonValueString>(Group.Value[At]->GetActorLabel()));
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("mesh"), First->GetStaticMesh()->GetPathName());
        Row->SetStringField(TEXT("mobility"), First->Mobility == EComponentMobility::Movable ? TEXT("Movable")
            : First->Mobility == EComponentMobility::Stationary ? TEXT("Stationary") : TEXT("Static"));
        Row->SetNumberField(TEXT("count"), Group.Value.Num());
        Row->SetArrayField(TEXT("actors"), Names);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("groups"), Rows);
    Out->SetNumberField(TEXT("actors"), Copies);
    Out->SetStringField(TEXT("note"), TEXT("Loaded actors only: a World Partition level counts the cells it has loaded."));
    return Out;
}
} // namespace McpPerception
