// Copyright (c) 2024 EngineWire Contributors
//
// The journal's recorder: edits of objects and level actors, kept for actors of the editor level and real assets and
// merged per object (the package events are in McpChangeJournalEvents.cpp). OnObjectModified fires on every Modify,
// thousands of times in one Blueprint compile, so pointers and flags decide first and a package's verdict is kept by
// its pointer: only the first edit of a package builds a string.

#include "Core/Journal/McpChangeJournal.h"

#include "Core/Journal/McpChangeJournalState.h"

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace McpChangeJournal
{
namespace
{
constexpr int32 MaxJournalEntries = 4096;
constexpr int32 MaxJournalProperties = 8;
constexpr int32 MaxJournalActions = 4;

FDelegateHandle GModifiedHandle;
FDelegateHandle GPropertyHandle;
FDelegateHandle GActorAddedHandle;
FDelegateHandle GActorDeletedHandle;

// A full journal keeps its newer half; cursors from before the cut can no longer be answered.
void Trim(FJournalState& State)
{
    State.Entries.Sort([](const FJournalEntry& A, const FJournalEntry& B) { return A.LastSeq < B.LastSeq; });
    const int32 Drop = State.Entries.Num() / 2;
    State.DroppedThrough = State.Entries[Drop - 1].LastSeq;
    State.Entries.RemoveAt(0, Drop);
    Reindex(State);
}

int32 NewEntry(FJournalState& State)
{
    if (State.Entries.Num() >= MaxJournalEntries)
    {
        Trim(State);
    }
    return State.Entries.AddDefaulted();
}

// The change watch's filter: an actor of the editor level, not a template, a preview or a child actor's copy.
bool IsLevelActor(const AActor* Actor)
{
    const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    return World && World->WorldType == EWorldType::Editor && !Actor->IsTemplate() && !Actor->HasAnyFlags(RF_Transient) &&
           !Actor->IsChildActor() && !Actor->bIsEditorPreviewActor;
}

int32 ActorEntry(AActor* Actor)
{
    FJournalState& State = JournalState();
    if (const int32* Known = State.ByObject.Find(FObjectKey(Actor)))
    {
        return *Known;
    }
    const int32 Index = NewEntry(State);
    FJournalEntry& Entry = State.Entries[Index];
    Entry.bActor = true;
    Entry.Actor = Actor;
    Entry.Path = Actor->GetPathName();
    // Modify comes before the edit, so this is where it stood; read against where it is when listed.
    Entry.Start = Actor->GetActorTransform();
    Entry.bStart = true;
    State.ByObject.Add(FObjectKey(Actor), Index);
    return Index;
}

// The entry an edit inside Package belongs to, or INDEX_NONE, judged once per package. A level's own objects are left
// out: what changed in a level is its actors.
int32 EditedPackageEntry(UPackage* Package)
{
    FJournalState& State = JournalState();
    if (!Package || Package == GetTransientPackage())
    {
        return INDEX_NONE;
    }
    const FObjectKey Key(Package);
    if (const int32* Known = State.ByObject.Find(Key))
    {
        return *Known;
    }
    const bool bJournalled = !Package->HasAnyFlags(RF_Transient) &&
                             !Package->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ContainsMap) &&
                             IsJournalledPackage(Package->GetName());
    const int32 Index = bJournalled ? PackageEntry(Package->GetFName()) : INDEX_NONE;
    State.ByObject.Add(Key, Index);
    return Index;
}

// The engine edits on its own all the time (loading, compiling, rebuilding derived data): a dozen meshes a startup read as
// changed and buried every real one, so only an edit this server or the user made is kept. Asked before anything else,
// it is also what keeps the hot path cheap.
void OnModified(UObject* Object)
{
    const int32 Index = CurrentBy() == JournalByEditor ? INDEX_NONE : EditedEntry(Object);
    if (Index != INDEX_NONE)
    {
        Touch(Index, JournalModified);
    }
}

void OnPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
    const int32 Index = CurrentBy() == JournalByEditor ? INDEX_NONE : EditedEntry(Object);
    if (Index == INDEX_NONE)
    {
        return;
    }
    FJournalEntry& Entry = Touch(Index, JournalModified);
    // The outer member edited (a struct's field names its struct); GetMemberPropertyName is 5.1+.
    const FProperty* Member = Event.MemberProperty ? Event.MemberProperty : Event.Property;
    const FName Name = Member ? Member->GetFName() : NAME_None;
    if (!Name.IsNone() && Entry.Properties.Num() < MaxJournalProperties)
    {
        Entry.Properties.AddUnique(Name);
    }
    NoteValue(Index, Object, Member, false, Event.ChangeType);
}

// The engine adds and removes actors of its own too (a new level's world settings, navigation data).
void OnActorAdded(AActor* Actor)
{
    if (CurrentBy() != JournalByEditor && !GIsReinstancing && IsLevelActor(Actor))
    {
        Touch(ActorEntry(Actor), JournalAdded);
    }
}

void OnActorDeleted(AActor* Actor)
{
    if (CurrentBy() != JournalByEditor && !GIsReinstancing && IsLevelActor(Actor))
    {
        Touch(ActorEntry(Actor), JournalDeleted).Label = Actor->GetActorLabel();
    }
}
} // namespace

void Reindex(FJournalState& State)
{
    State.ByObject.Reset();
    State.ByPackage.Reset();
    for (int32 Index = 0; Index < State.Entries.Num(); ++Index)
    {
        const FJournalEntry& Entry = State.Entries[Index];
        if (!Entry.bActor)
        {
            State.ByPackage.Add(FName(*Entry.Path), Index);
        }
        else if (AActor* Actor = Entry.Actor.Get())
        {
            State.ByObject.Add(FObjectKey(Actor), Index);
        }
    }
}

int32 EditedEntry(UObject* Object)
{
    if (!Object || !IsInGameThread() || GIsReinstancing ||
        Object->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        return INDEX_NONE;
    }
    AActor* Actor = Cast<AActor>(Object);
    if (const UActorComponent* Component = Actor ? nullptr : Cast<UActorComponent>(Object))
    {
        Actor = Component->GetOwner();
    }
    if (Actor)
    {
        return IsLevelActor(Actor) ? ActorEntry(Actor) : INDEX_NONE;
    }
    return EditedPackageEntry(Object->GetOutermost());
}

FJournalState& JournalState()
{
    static FJournalState State;
    return State;
}

uint8 CurrentBy()
{
    if (!JournalState().RequestId.IsEmpty())
    {
        return JournalByMcp;
    }
    // Latent work that edits after its request ended reads as mcp only inside UEngineWireSubsystem::RunLatentReply.
    return (GUndo || GIsTransacting) ? JournalByUser : JournalByEditor;
}

int32 PackageEntry(FName PackageName)
{
    FJournalState& State = JournalState();
    if (const int32* Known = State.ByPackage.Find(PackageName))
    {
        return *Known;
    }
    const int32 Index = NewEntry(State);
    State.Entries[Index].Path = PackageName.ToString();
    State.ByPackage.Add(PackageName, Index);
    return Index;
}

FJournalEntry& Touch(int32 Index, uint8 Event)
{
    FJournalState& State = JournalState();
    FJournalEntry& Entry = State.Entries[Index];
    const uint8 By = CurrentBy();
    Entry.Events |= Event;
    Entry.By |= By;
    Entry.LastSeq = ++State.Seq;
    PublishSeq(State.Seq);
    if (By == JournalByUser)
    {
        Entry.LastUserSeq = State.Seq;
    }
    if (By == JournalByMcp && Entry.Actions.Num() < MaxJournalActions)
    {
        Entry.Actions.AddUnique(State.Action);
    }
    return Entry;
}

// A World Partition level keeps each actor in a package of its own: the actor's row says what changed.
bool IsJournalledPackage(const FString& Name)
{
    return Name.StartsWith(TEXT("/")) && !Name.StartsWith(TEXT("/Temp/")) && !Name.StartsWith(TEXT("/Script/")) &&
           !Name.StartsWith(TEXT("/Engine/")) && !Name.StartsWith(TEXT("/Memory/")) && !Name.Contains(TEXT("/__External"));
}

void ForgetActorsOf(const UWorld* World)
{
    FJournalState& State = JournalState();
    const auto InWorld = [World](const FJournalEntry& Entry)
    {
        const AActor* Actor = Entry.Actor.Get();
        return Actor && Actor->GetWorld() == World;
    };
    if (World && State.Entries.RemoveAll(InWorld) > 0)
    {
        Reindex(State);
    }
}

void BindObjectEvents()
{
    GModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddStatic(&OnModified);
    GPropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddStatic(&OnPropertyChanged);
    if (GEngine)
    {
        GActorAddedHandle = GEngine->OnLevelActorAdded().AddStatic(&OnActorAdded);
        GActorDeletedHandle = GEngine->OnLevelActorDeleted().AddStatic(&OnActorDeleted);
    }
}

void UnbindObjectEvents()
{
    FCoreUObjectDelegates::OnObjectModified.Remove(GModifiedHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(GPropertyHandle);
    if (GEngine)
    {
        GEngine->OnLevelActorAdded().Remove(GActorAddedHandle);
        GEngine->OnLevelActorDeleted().Remove(GActorDeletedHandle);
    }
}

void BeginRequest(const FString& RequestId, const FString& Action)
{
    FJournalState& State = JournalState();
    if (State.RequestDepth++ == 0)
    {
        State.RequestId = RequestId;
        State.Action = Action;
        State.WarnUntil = State.Seq;
    }
}

void EndRequest()
{
    FJournalState& State = JournalState();
    if (State.RequestDepth > 0 && --State.RequestDepth == 0)
    {
        State.RequestId.Reset();
        State.Action.Reset();
    }
}
} // namespace McpChangeJournal
