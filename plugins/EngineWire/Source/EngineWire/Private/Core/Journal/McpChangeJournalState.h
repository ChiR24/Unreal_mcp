// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class FProperty;
class UObject;
class UWorld;

// The journal's state, shared by its recorder (McpChangeJournal.cpp) and its reader (McpChangeJournalRead.cpp).
namespace McpChangeJournal
{
enum EJournalEvent : uint8
{
    JournalAdded = 1 << 0,
    JournalModified = 1 << 1,
    JournalDeleted = 1 << 2,
    JournalSaved = 1 << 3,
    JournalRenamed = 1 << 4,
    JournalRemoved = 1 << 5,
    JournalOpened = 1 << 6,
};

enum EJournalBy : uint8
{
    JournalByMcp = 1 << 0,
    JournalByUser = 1 << 1,
    JournalByEditor = 1 << 2,
};

/** A property's value as exported text: before the first change the journal saw, and after the latest one. */
struct FJournalValue
{
    FName Property;
    FString From;
    FString To;
    bool bFrom = false;
    bool bTo = false;
};

/** One changed object: a level actor, or an asset or level package by name. Every change to it merges here. */
struct FJournalEntry
{
    TWeakObjectPtr<AActor> Actor;
    bool bActor = false;
    bool bLevel = false;
    FString Path;
    /** A deleted actor's label, kept as it went; a live actor's name is read when the entry is listed. */
    FString Label;
    FString OldPath;
    uint8 Events = 0;
    uint8 By = 0;
    TArray<FName> Properties;
    TArray<FJournalValue> Values;
    TArray<FString> Actions;
    /** An actor's transform when the journal first saw it, compared with where it is when the entry is listed. */
    FTransform Start;
    bool bStart = false;
    uint64 LastSeq = 0;
    uint64 LastUserSeq = 0;
};

struct FJournalState
{
    FString Session;
    uint64 Seq = 0;
    /** Cursors below this lost changes when the oldest entries were dropped. */
    uint64 DroppedThrough = 0;
    TArray<FJournalEntry> Entries;
    /** Level actors, and every package already judged (INDEX_NONE: not journalled), by object. */
    TMap<FObjectKey, int32> ByObject;
    TMap<FName, int32> ByPackage;
    int32 RequestDepth = 0;
    FString RequestId;
    FString Action;
    /** The user's changes a reply has not warned of: after WarnedThrough, up to WarnUntil (when the request began). */
    uint64 WarnedThrough = 0;
    uint64 WarnUntil = 0;
};

FJournalState& JournalState();
/** The indexes after entries were removed; each package's verdict is judged again. */
void Reindex(FJournalState& State);

/** Who a change made now is by: mcp inside a request, user inside a transaction no request opened, else editor. */
uint8 CurrentBy();
/** The entry of the package Name (an asset or a level), made on first use. */
int32 PackageEntry(FName PackageName);
/** Merges one change into the entry at Index, marked by who made it; the reference holds until the next new entry. */
FJournalEntry& Touch(int32 Index, uint8 Event);
/** Content a caller can name: not the engine's, a script, a temporary or an in-memory package, nor one actor's package. */
bool IsJournalledPackage(const FString& Name);
/** Drops the rows of World's actors: every one of them was made by the level opening. */
void ForgetActorsOf(const UWorld* World);
/** Hands Seq to the transports' threads (LatestSeq); the journal itself stays game-thread state. */
void PublishSeq(uint64 Seq);
/** The object and level-actor events (McpChangeJournal.cpp); Start and Stop add the package events around them. */
void BindObjectEvents();
void UnbindObjectEvents();
/** The entry an edit of Object belongs to (its actor's or its package's), made on first use; INDEX_NONE if none is kept. */
int32 EditedEntry(UObject* Object);
/**
 * Keeps Member's value on Object, before (bBefore) or after a change, in the entry at Index (McpChangeJournalValues.cpp);
 * ChangeType is the change event's EPropertyChangeType flags (an interactive change is skipped).
 */
void NoteValue(int32 Index, UObject* Object, const FProperty* Member, bool bBefore, uint32 ChangeType);
/** The event announcing a property about to change (McpChangeJournalValues.cpp), bound beside the object events. */
void BindValueEvents();
void UnbindValueEvents();
} // namespace McpChangeJournal
