// Copyright (c) 2024 EngineWire Contributors
//
// The journal's package events: assets the registry hears created, removed or renamed, packages saved, levels opened;
// and the journal's Start and Stop, which bind them around the object events (McpChangeJournal.cpp).

#include "Core/Journal/McpChangeJournal.h"

#include "Core/Journal/McpChangeJournalState.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace McpChangeJournal
{
namespace
{
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

void Start()
{
    JournalState().Session = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToLower();
    BindObjectEvents();
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
    UPackage::PackageSavedWithContextEvent.Remove(GSavedHandle);
    FEditorDelegates::OnMapOpened.Remove(GMapOpenedHandle);
    if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
    {
        Module->Get().OnAssetAdded().Remove(GAssetAddedHandle);
        Module->Get().OnAssetRemoved().Remove(GAssetRemovedHandle);
        Module->Get().OnAssetRenamed().Remove(GAssetRenamedHandle);
    }
    JournalState() = FJournalState();
}
} // namespace McpChangeJournal
