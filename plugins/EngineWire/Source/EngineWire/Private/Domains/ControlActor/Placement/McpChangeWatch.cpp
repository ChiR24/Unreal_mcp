// Copyright (c) 2024 EngineWire Contributors
//
// The checks audit_placement runs, run on whatever a request changed. Spawn and set_transform judged their own actors,
// but an actor duplicated, scattered, snapped, re-meshed or spawned by any other capability (a light, a volume, a
// generated mesh, a level helper) answered a bare success, so an agent that never thought to run the audit left it
// sunk, floating, z-fighting or on the default grid material and moved on.

#include "Domains/ControlActor/Placement/McpChangeWatch.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Placement/McpAuditFinding.h"
#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

namespace
{
// An actor as the request first touched it: where its parts stand and what they draw.
struct FWatchBefore
{
    bool bNew = false;
    uint32 Where = 0;
    uint32 Drawn = 0;
};

constexpr int32 WatchMaxChecked = 64; // each placement check walks the level once
constexpr int32 WatchMaxListed = 12;

FString GWatchRequestId;
TMap<TWeakObjectPtr<AActor>, FWatchBefore> GWatchTouched;
TSet<const AActor*> GWatchReported; // compared, never read: a request does not outlive its actors
FDelegateHandle GWatchModifiedHandle;
FDelegateHandle GWatchAddedHandle;

// Where every part stands (a part moved inside its actor counts, and its bounds catch a new mesh) and what each draws:
// assets, not component objects, summed per part, so a construction script rebuilding the same parts reads unchanged.
void WatchFingerprint(const AActor* Actor, uint32& OutWhere, uint32& OutDrawn)
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

// Editor-level actors only: a play world is thrown away when play stops, and its game spawns its own actors. A
// Blueprint compile respawns every placed instance (GIsReinstancing) and a child actor is respawned with its parent's
// construction script; neither moved anything, and the editor's own actor-added listeners skip them alike.
void WatchNote(AActor* Actor, bool bNew)
{
    const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    if (!World || World->WorldType != EWorldType::Editor || GIsReinstancing || Actor->IsTemplate() ||
        Actor->HasAnyFlags(RF_Transient) || Actor->IsChildActor() || Actor->bIsEditorPreviewActor)
    {
        return;
    }
    if (FWatchBefore* Known = GWatchTouched.Find(Actor))
    {
        Known->bNew |= bNew; // a component modified while the actor spawned noted it first
        return;
    }
    FWatchBefore& Before = GWatchTouched.Add(Actor);
    Before.bNew = bNew;
    WatchFingerprint(Actor, Before.Where, Before.Drawn); // Modify is called before the change, so this is the old state
}

void WatchOnModified(UObject* Object)
{
    if (!IsInGameThread())
    {
        return;
    }
    if (AActor* Actor = Cast<AActor>(Object))
    {
        WatchNote(Actor, false);
    }
    else if (const UActorComponent* Component = Cast<UActorComponent>(Object))
    {
        WatchNote(Component->GetOwner(), false);
    }
}

void WatchOnAdded(AActor* Actor)
{
    if (IsInGameThread())
    {
        WatchNote(Actor, true);
    }
}
} // namespace

namespace McpChangeWatch
{
void Begin(const FString& RequestId)
{
    End();
    GWatchRequestId = RequestId;
    // The engine reports a Modify once per object per frame, so an actor an earlier request in the same frame touched
    // went unseen by this one. The set only spares listeners a repeat notice within a frame; clearing it costs one.
    FCoreUObjectDelegates::ObjectsModifiedThisFrame.Empty();
    GWatchModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddStatic(&WatchOnModified);
    if (GEngine)
    {
        GWatchAddedHandle = GEngine->OnLevelActorAdded().AddStatic(&WatchOnAdded);
    }
}

// ponytail: a reply sent after its handler returned (call_editor_tool, long tasks) carries no checks; keep a watch per
// request id until that request's reply if a placing capability ever answers late.
void End()
{
    FCoreUObjectDelegates::OnObjectModified.Remove(GWatchModifiedHandle);
    if (GEngine)
    {
        GEngine->OnLevelActorAdded().Remove(GWatchAddedHandle);
    }
    GWatchModifiedHandle.Reset();
    GWatchAddedHandle.Reset();
    GWatchTouched.Reset();
    GWatchReported.Reset();
    GWatchRequestId.Reset();
}

void NotePlacementReported(const AActor* Actor)
{
    if (!GWatchRequestId.IsEmpty() && Actor)
    {
        GWatchReported.Add(Actor);
    }
}

void AddWarnings(const FString& RequestId, const TSharedPtr<FJsonObject>& Result)
{
    if (GWatchRequestId.IsEmpty() || RequestId != GWatchRequestId || !Result.IsValid() || GWatchTouched.Num() == 0)
    {
        return;
    }
    TMap<AActor*, TSharedPtr<FJsonObject>> Rows; // judged together, so faces two changed actors share are found once
    TArray<FString> Content;
    TSet<const AActor*> Doubled; // named as another's twin: one warning covers the pair
    int32 Changed = 0;
    for (const TPair<TWeakObjectPtr<AActor>, FWatchBefore>& Pair : GWatchTouched)
    {
        AActor* Actor = Pair.Key.Get();
        if (!IsValid(Actor) || Actor->IsHidden())
        {
            continue;
        }
        uint32 Where = 0, Drawn = 0;
        WatchFingerprint(Actor, Where, Drawn);
        const bool bRedrawn = Pair.Value.bNew || Drawn != Pair.Value.Drawn;
        if ((!bRedrawn && Where == Pair.Value.Where) || Doubled.Contains(Actor) || ++Changed > WatchMaxChecked)
        {
            continue; // renamed, tagged, foldered: nothing it shows or where it stands moved
        }
        // A copy on its original intersects it and shares every face with it; the duplicate warning says all of that.
        const AActor* Twin = McpContentAudit::FindTwin(Actor);
        if (Twin)
        {
            Content.Add(McpContentAudit::DuplicateIssue(Actor, Twin));
            Doubled.Add(Twin);
        }
        else if (!GWatchReported.Contains(Actor))
        {
            const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            McpPlacement::DescribePlacement(Actor, Row);
            Rows.Add(Actor, Row);
        }
        if (bRedrawn)
        {
            TArray<McpContentAudit::FIssue> Issues;
            McpContentAudit::FindUnfinished(Actor, Issues);
            for (const McpContentAudit::FIssue& Issue : Issues)
            {
                Content.Add(FString::Printf(TEXT("'%s': %s"), *McpActorRef(Actor), *Issue.Issue));
            }
        }
    }
    McpCoplanar::AddCoplanarToRows(Rows);
    TArray<FString> Found;
    for (const TPair<AActor*, TSharedPtr<FJsonObject>>& Row : Rows)
    {
        FString Warning;
        if (Row.Value->TryGetStringField(TEXT("placementWarning"), Warning) && !Warning.IsEmpty())
        {
            Found.Add(Warning);
        }
    }
    const bool bPlacement = Found.Num() > 0;
    Found.Append(Content);
    if (Found.Num() == 0 && Changed <= WatchMaxChecked)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Warnings = *Existing;
    }
    for (int32 Index = 0; Index < Found.Num() && Index < WatchMaxListed; ++Index)
    {
        Warnings.Add(MakeShared<FJsonValueString>(Found[Index]));
    }
    TArray<FString> Unsaid;
    if (Found.Num() > WatchMaxListed)
    {
        Unsaid.Add(FString::Printf(TEXT("%d more problem(s) like these on the actors this call changed"), Found.Num() - WatchMaxListed));
    }
    if (Changed > WatchMaxChecked)
    {
        Unsaid.Add(FString::Printf(TEXT("%d of the %d actors it changed were not checked"), Changed - WatchMaxChecked, Changed));
    }
    if (Unsaid.Num() > 0)
    {
        Warnings.Add(MakeShared<FJsonValueString>(FString::Join(Unsaid, TEXT("; ")) +
            TEXT(": control_actor audit_placement lists every problem in the level.")));
    }
    if (bPlacement)
    {
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("A placement made on purpose drops out of these checks with the actor tag "
            "mcp.placement.ok; faces in one plane flicker whatever the intent.")));
    }
    Result->SetArrayField(TEXT("warnings"), Warnings);
}
} // namespace McpChangeWatch
