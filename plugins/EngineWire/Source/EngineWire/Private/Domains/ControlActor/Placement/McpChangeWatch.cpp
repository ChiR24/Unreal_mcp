// Copyright (c) 2024 EngineWire Contributors
//
// The checks audit_placement runs, run on whatever a request changed. Spawn and set_transform judged their own actors,
// but an actor duplicated, scattered, snapped, re-meshed or spawned by any other capability (a light, a volume, a
// generated mesh, a level helper) answered a bare success, so an agent that never thought to run the audit left it
// sunk, floating, z-fighting or on the default grid material and moved on.

#include "Domains/ControlActor/Placement/McpChangeWatch.h"

#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Placement/McpAuditFinding.h"
#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
// Every placed copy of a Blueprint the request edits, by path: the compile that follows replaces each copy with a new
// object of the same name, so a part moved in the Blueprint moved in every copy with no actor of the level modified.
TMap<FString, FWatchBefore> GWatchCopies;
FDelegateHandle GWatchModifiedHandle;
FDelegateHandle GWatchAddedHandle;

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
    McpContentAudit::Fingerprint(Actor, Before.Where, Before.Drawn); // Modify is called before the change, so this is the old state
}

// The Blueprint an edit of Object changes: the asset, a component template, a construction-script node or its defaults.
const UBlueprint* WatchBlueprintOf(const UObject* Object)
{
    if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object))
    {
        return Blueprint;
    }
    if (const USCS_Node* Node = Cast<USCS_Node>(Object))
    {
        return Node->GetSCS() ? Node->GetSCS()->GetBlueprint() : nullptr;
    }
    const UClass* Class = Object->HasAnyFlags(RF_ClassDefaultObject) ? Object->GetClass() : Cast<UClass>(Object->GetOuter());
    return Class ? UBlueprint::GetBlueprintFromClass(Class) : nullptr;
}

void WatchBlueprintCopies(const UObject* Object)
{
    const UBlueprint* Blueprint = GIsReinstancing ? nullptr : WatchBlueprintOf(Object);
    UClass* Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!Class || !World || !Class->IsChildOf(AActor::StaticClass()))
    {
        return;
    }
    for (TActorIterator<AActor> It(World, Class); It; ++It)
    {
        const FString Path = It->GetPathName();
        if (!GWatchCopies.Contains(Path))
        {
            FWatchBefore& Before = GWatchCopies.Add(Path);
            McpContentAudit::Fingerprint(*It, Before.Where, Before.Drawn);
        }
    }
}

void WatchOnModified(UObject* Object)
{
    if (!IsInGameThread() || !Object)
    {
        return;
    }
    const UActorComponent* Component = Cast<UActorComponent>(Object);
    if (AActor* Actor = Cast<AActor>(Object))
    {
        WatchNote(Actor, false);
    }
    else if (Component && Component->GetOwner())
    {
        WatchNote(Component->GetOwner(), false);
    }
    else
    {
        WatchBlueprintCopies(Object);
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
    GWatchCopies.Reset();
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
    if (GWatchRequestId.IsEmpty() || RequestId != GWatchRequestId || !Result.IsValid() ||
        GWatchTouched.Num() + GWatchCopies.Num() == 0)
    {
        return;
    }
    TMap<AActor*, TSharedPtr<FJsonObject>> Rows; // judged together, so faces two changed actors share are found once
    TArray<FString> Content;
    TSet<const AActor*> Doubled; // named as another's twin: one warning covers the pair
    int32 Changed = 0;
    // Where it stands is judged when it moved, what it shows when that changed: a new material alone repeats no placement.
    const auto Consider = [&](AActor* Actor, const FWatchBefore& Before) {
        uint32 Where = 0, Drawn = 0;
        if (!IsValid(Actor) || Actor->IsHidden() || Doubled.Contains(Actor))
        {
            return;
        }
        McpContentAudit::Fingerprint(Actor, Where, Drawn);
        const bool bMoved = Before.bNew || Where != Before.Where;
        const bool bRedrawn = Before.bNew || Drawn != Before.Drawn;
        if ((!bMoved && !bRedrawn) || ++Changed > WatchMaxChecked)
        {
            return; // renamed, tagged, foldered: nothing it shows or where it stands moved
        }
        // A copy on its original intersects it and shares every face with it; the duplicate warning says all of that.
        const AActor* Twin = bMoved ? McpContentAudit::FindTwin(Actor) : nullptr;
        if (Twin)
        {
            Content.Add(McpContentAudit::DuplicateIssue(Actor, Twin));
            Doubled.Add(Twin);
        }
        else if (bMoved && !GWatchReported.Contains(Actor))
        {
            Rows.Add(Actor, MakeShared<FJsonObject>());
            McpPlacement::DescribePlacement(Actor, Rows[Actor]);
        }
        TArray<McpContentAudit::FIssue> Issues;
        if (bRedrawn)
        {
            McpContentAudit::FindUnfinished(Actor, Issues);
        }
        for (const McpContentAudit::FIssue& Issue : Issues)
        {
            Content.Add(FString::Printf(TEXT("'%s': %s"), *McpActorRef(Actor), *Issue.Issue));
        }
    };
    for (const TPair<TWeakObjectPtr<AActor>, FWatchBefore>& Pair : GWatchTouched)
    {
        Consider(Pair.Key.Get(), Pair.Value);
    }
    for (const TPair<FString, FWatchBefore>& Pair : GWatchCopies)
    {
        AActor* Copy = FindObject<AActor>(nullptr, *Pair.Key);
        if (Copy && !GWatchTouched.Contains(Copy))
        {
            Consider(Copy, Pair.Value);
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
