// Copyright (c) 2024 EngineWire Contributors
//
// The journal's package events: assets the registry hears created, removed or renamed, packages saved, levels opened;
// the journal's Start and Stop, which bind them around the object events (McpChangeJournal.cpp); and what the
// transports' threads may read of it: the session under a lock, the latest number atomically.

#include "Core/Journal/McpChangeJournal.h"

#include "Core/Journal/McpChangeJournalState.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#include <atomic>

namespace McpChangeJournal
{
namespace
{
FCriticalSection GPublishedLock;
FString GPublishedSession;
std::atomic<uint64> GPublishedSeq{0};

void PublishSession(const FString& Session)
{
    FScopeLock Lock(&GPublishedLock);
    GPublishedSession = Session;
    GPublishedSeq.store(0, std::memory_order_relaxed);
}

FDelegateHandle GAssetAddedHandle;
FDelegateHandle GAssetRemovedHandle;
FDelegateHandle GAssetRenamedHandle;
FDelegateHandle GSavedHandle;
FDelegateHandle GMapOpenedHandle;

// The registry reports every asset it finds while it scans at startup: only what it hears after that is a change.
void OnAssetEvent(const FAssetData& Data, uint8 Event, const FString& OldPath)
{
    const FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if (!IsInGameThread() || !Module || Module->Get().IsLoadingAssets() || !IsJournalledPackage(Data.PackageName.ToString()))
    {
        return;
    }
    FJournalEntry& Entry = Touch(PackageEntry(Data.PackageName), Event);
    Entry.OldPath = OldPath.IsEmpty() ? Entry.OldPath : OldPath;
}

void OnAssetRenamed(const FAssetData& Data, const FString& OldObjectPath)
{
    OnAssetEvent(Data, JournalRenamed, OldObjectPath);
}

// An autosave writes a backup, not the asset.
void OnSaved(const FString& /*Filename*/, UPackage* Package, FObjectPostSaveContext Context)
{
    if (!IsInGameThread() || !Package || (Context.GetSaveFlags() & SAVE_FromAutosave) || Context.IsProceduralSave() ||
        Package->HasAnyPackageFlags(PKG_PlayInEditor) || !IsJournalledPackage(Package->GetName()))
    {
        return;
    }
    FJournalEntry& Entry = Touch(PackageEntry(Package->GetFName()), JournalSaved);
    Entry.bLevel = Entry.bLevel || Package->HasAnyPackageFlags(PKG_ContainsMap);
}

// Opening a level loads, fixes up and labels its actors, inside the request that opened it or none: no change of theirs
// is one yet. The event comes last in the editor's LoadMap, after every fix-up.
void OnMapOpened(const FString& Filename, bool bAsTemplate)
{
    ForgetActorsOf(GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
    FString PackageName;
    if (!bAsTemplate && FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName) &&
        IsJournalledPackage(PackageName))
    {
        Touch(PackageEntry(FName(*PackageName)), JournalOpened).bLevel = true;
    }
}
} // namespace

bool InRequest()
{
    return !JournalState().RequestId.IsEmpty();
}

void PublishSeq(uint64 Seq)
{
    GPublishedSeq.store(Seq, std::memory_order_relaxed);
}

uint64 LatestSeq()
{
    return GPublishedSeq.load(std::memory_order_relaxed);
}

FString LatestCursor(uint64* OutSeq)
{
    FScopeLock Lock(&GPublishedLock);
    const uint64 Seq = LatestSeq();
    if (OutSeq)
    {
        *OutSeq = Seq;
    }
    return GPublishedSession.IsEmpty() ? FString() : GPublishedSession + TEXT(".") + LexToString(Seq);
}

void Start()
{
    JournalState().Session = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToLower();
    PublishSession(JournalState().Session);
    BindObjectEvents();
    BindValueEvents();
    GSavedHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&OnSaved);
    GMapOpenedHandle = FEditorDelegates::OnMapOpened.AddStatic(&OnMapOpened);
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    GAssetAddedHandle = Registry.OnAssetAdded().AddLambda([](const FAssetData& Data) { OnAssetEvent(Data, JournalAdded, FString()); });
    GAssetRemovedHandle = Registry.OnAssetRemoved().AddLambda([](const FAssetData& Data) { OnAssetEvent(Data, JournalRemoved, FString()); });
    GAssetRenamedHandle = Registry.OnAssetRenamed().AddStatic(&OnAssetRenamed);
}

void Stop()
{
    UnbindObjectEvents();
    UnbindValueEvents();
    UPackage::PackageSavedWithContextEvent.Remove(GSavedHandle);
    FEditorDelegates::OnMapOpened.Remove(GMapOpenedHandle);
    if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
    {
        Module->Get().OnAssetAdded().Remove(GAssetAddedHandle);
        Module->Get().OnAssetRemoved().Remove(GAssetRemovedHandle);
        Module->Get().OnAssetRenamed().Remove(GAssetRenamedHandle);
    }
    JournalState() = FJournalState();
    PublishSession(FString());
}
} // namespace McpChangeJournal
