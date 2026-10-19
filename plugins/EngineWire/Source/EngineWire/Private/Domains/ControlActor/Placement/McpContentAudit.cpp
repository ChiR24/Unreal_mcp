// Copyright (c) 2024 EngineWire Contributors
//
// audit_placement's content kinds: what a level shows that was never finished. A slot left on the engine's default
// grid, a label still reading "Text" and an actor pasted twice in the same place each read as slop in the game, and
// each was found only by flying the camera around a level. The change watch runs the same checks on every actor a
// call changes (McpChangeWatch.cpp).

#include "Domains/ControlActor/Placement/McpAuditFinding.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "GameFramework/Actor.h"
// EMaterialDomain's own header exists only on the engines that split it out; 5.0 declares it in MaterialShared.h.
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif
#include "MaterialShared.h"
#include "Materials/Material.h"

namespace
{
bool DrawnInGame(const UPrimitiveComponent* Part)
{
    return Part && Part->IsRegistered() && Part->IsVisible() && !Part->bHiddenInGame && !Part->IsEditorOnly();
}

double ContentPartSize(const UPrimitiveComponent* Part)
{
    return 2.0 * Part->Bounds.BoxExtent.GetMax();
}

// What an actor draws and where, so a copy pasted onto the original matches it and nothing else does: the class,
// the meshes it shows, and its transform to half a centimetre, a tenth of a degree and a thousandth of scale.
FString DrawnKey(const AActor* Actor)
{
    TArray<FString> Meshes;
    TInlineComponentArray<UStaticMeshComponent*> Parts(Actor);
    for (const UStaticMeshComponent* Part : Parts)
    {
        if (DrawnInGame(Part) && Part->GetStaticMesh())
        {
            Meshes.Add(Part->GetStaticMesh()->GetPathName());
        }
    }
    if (Meshes.Num() == 0)
    {
        return FString();
    }
    Meshes.Sort();
    const FTransform Transform = Actor->GetActorTransform();
    const FVector Location = Transform.GetLocation() * 2.0;
    const FRotator Rotation = Transform.Rotator() * 10.0;
    const FVector Scale = Transform.GetScale3D() * 1000.0;
    FString Key = Actor->GetClass()->GetPathName() + TEXT("|") + FString::Join(Meshes, TEXT(","));
    for (const double Value : {Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Scale.X, Scale.Y, Scale.Z})
    {
        Key += TEXT("|") + LexToString(static_cast<int64>(FMath::RoundToDouble(Value)));
    }
    return Key;
}

double ContentBoundsSize(const AActor* Actor)
{
    FVector Origin, Extent;
    Actor->GetActorBounds(true, Origin, Extent);
    return 2.0 * Extent.GetMax();
}
} // namespace

namespace McpContentAudit
{
// Where every part stands (a part moved inside its actor counts, and its bounds catch a new mesh) and what each draws:
// assets, not component objects, summed per part, so a construction script rebuilding the same parts reads unchanged.
void Fingerprint(const AActor* Actor, uint32& OutWhere, uint32& OutDrawn)
{
    OutWhere = GetTypeHash(Actor->GetActorLocation());
    OutDrawn = 0;
    TInlineComponentArray<UPrimitiveComponent*> Parts(Actor);
    for (const UPrimitiveComponent* Part : Parts)
    {
        const FTransform& Transform = Part->GetComponentTransform();
        uint32 Where = HashCombine(GetTypeHash(Transform.GetLocation()), GetTypeHash(Transform.Rotator().Euler()));
        OutWhere += HashCombine(HashCombine(Where, GetTypeHash(Transform.GetScale3D())), GetTypeHash(Part->Bounds.BoxExtent));
        uint32 Drawn = (Part->IsVisible() ? 2u : 0u) | (Part->bHiddenInGame ? 1u : 0u);
        if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Part))
        {
            Drawn = HashCombine(Drawn, PointerHash(Static->GetStaticMesh()));
        }
        if (const UMeshComponent* Mesh = Cast<UMeshComponent>(Part))
        {
            for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
            {
                Drawn = HashCombine(Drawn, PointerHash(Mesh->GetMaterial(Slot)));
            }
        }
        if (const UTextRenderComponent* Text = Cast<UTextRenderComponent>(Part))
        {
            Drawn = HashCombine(Drawn, GetTypeHash(Text->Text.ToString()));
        }
        OutDrawn += Drawn;
    }
}

void FindUnfinished(const AActor* Actor, TArray<FIssue>& Out)
{
    const UMaterial* Default = UMaterial::GetDefaultMaterial(MD_Surface);
    TInlineComponentArray<UPrimitiveComponent*> Parts(Actor);
    for (const UPrimitiveComponent* Part : Parts)
    {
        if (!DrawnInGame(Part))
        {
            continue;
        }
        if (const UMeshComponent* Mesh = Cast<UMeshComponent>(Part))
        {
            const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Mesh);
            for (int32 Slot = 0; (!Static || Static->GetStaticMesh()) && Slot < Mesh->GetNumMaterials(); ++Slot)
            {
                const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
                if (!Material || Material->GetMaterial() == Default)
                {
                    Out.Add({TEXT("default_material"), Part->GetName(), FString::Printf(
                        TEXT("'%s' slot %d draws the engine's default grid material (%s), the checker of an unfinished asset: give it a material."),
                        *Part->GetName(), Slot, Material ? *Material->GetName() : TEXT("no material")), ContentPartSize(Part)});
                    break;
                }
            }
        }
        else if (const UTextRenderComponent* Text = Cast<UTextRenderComponent>(Part))
        {
            if (Text->Text.ToString() == TEXT("Text"))
            {
                Out.Add({TEXT("placeholder_text"), Part->GetName(), FString::Printf(
                    TEXT("'%s' still reads the engine default \"Text\": give it its label, or an empty text when a script writes it in play."),
                    *Part->GetName()), Text->WorldSize * Part->GetComponentScale().GetMax()});
            }
        }
    }
}

const AActor* FindTwin(const AActor* Actor)
{
    UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    const FString Key = World ? DrawnKey(Actor) : FString();
    if (Key.IsEmpty())
    {
        return nullptr;
    }
    const FVector Location = Actor->GetActorLocation();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        // The location first: building a key for every actor in the level, for each one a call changed, is the cost.
        if (*It != Actor && !It->IsHidden() && FVector::DistSquared(It->GetActorLocation(), Location) < 1.0 && DrawnKey(*It) == Key)
        {
            return *It;
        }
    }
    return nullptr;
}

FString DuplicateIssue(const AActor* Copy, const AActor* Original)
{
    return FString::Printf(
        TEXT("'%s' is a copy of '%s' in the same place, drawing the same meshes, so both are drawn and every face z-fights with its twin: delete one."),
        *McpActorRef(Copy), *McpActorRef(Original));
}

void MergeContent(UWorld* World, const FString& NameFilter, TArray<FMcpPlacementFinding>& Findings,
                  TMap<const AActor*, int32>& FindingIndex)
{
    TMap<FString, const AActor*> Originals;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        const AActor* Actor = *It;
        if (!Actor || Actor->IsHidden())
        {
            continue;
        }
        TArray<FIssue> Issues;
        const FString Key = DrawnKey(Actor);
        if (const AActor* const* Original = Key.IsEmpty() ? nullptr : Originals.Find(Key))
        {
            Issues.Add({TEXT("duplicate"), FString(), DuplicateIssue(Actor, *Original), ContentBoundsSize(Actor)});
        }
        else if (!Key.IsEmpty())
        {
            Originals.Add(Key, Actor);
        }
        if (!NameFilter.IsEmpty() && !Actor->GetActorLabel().Contains(NameFilter) && !Actor->GetName().Contains(NameFilter))
        {
            continue;
        }
        FindUnfinished(Actor, Issues);
        if (Issues.Num() == 0)
        {
            continue;
        }
        Issues.Sort([](const FIssue& A, const FIssue& B) { return A.Size > B.Size; });
        const int32* Existing = FindingIndex.Find(Actor);
        FMcpPlacementFinding& Finding = Existing ? Findings[*Existing] : Findings.AddDefaulted_GetRef();
        if (!Existing)
        {
            Finding.ActorName = McpActorRef(Actor);
            FindingIndex.Add(Actor, Findings.Num() - 1);
        }
        for (const FIssue& Issue : Issues)
        {
            const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("kind"), Issue.Kind);
            if (!Issue.Component.IsEmpty())
            {
                Entry->SetStringField(TEXT("component"), Issue.Component);
            }
            Entry->SetStringField(TEXT("issue"), Issue.Issue);
            Finding.ContentIssues.Add(MakeShared<FJsonValueObject>(Entry));
            Finding.ContentKinds.AddUnique(Issue.Kind);
        }
        if (Finding.Kind.IsEmpty())
        {
            Finding.Kind = Issues[0].Kind;
            Finding.Issue = Issues[0].Issue;
            Finding.Severity = Issues[0].Size;
        }
        else
        {
            Finding.Issue += TEXT(" It also shows unfinished content: see contentIssues.");
        }
    }
}
} // namespace McpContentAudit
