// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "Safety/McpSafeOperationsAssetSave.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "ISourceControlModule.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "SourceControlHelpers.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UObjectHash.h"

namespace McpAssetRename
{
namespace
{
struct FMcpRedirectorFix
{
    UObjectRedirector* Redirector = nullptr;
    TArray<UPackage*> Referencers;
    bool bKeep = false; // referenced from code, or by a package that did not load or did not save
};

// The same entries AssetTools maps: a Blueprint's generated class and class default move with it.
void AddRemap(TMap<FSoftObjectPath, FSoftObjectPath>& Remap, const UObjectRedirector* Redirector)
{
    const FSoftObjectPath Old(Redirector);
    const FSoftObjectPath New(Redirector->DestinationObject);
    Remap.Add(Old, New);
    if (Cast<UBlueprint>(Redirector->DestinationObject))
    {
        Remap.Add(FSoftObjectPath(Old.ToString() + TEXT("_C")), FSoftObjectPath(New.ToString() + TEXT("_C")));
        Remap.Add(FSoftObjectPath(FString::Printf(TEXT("%s.Default__%s_C"), *Old.GetLongPackageName(), *Old.GetAssetName())),
                  FSoftObjectPath(FString::Printf(TEXT("%s.Default__%s_C"), *New.GetLongPackageName(), *New.GetAssetName())));
    }
}
} // namespace

void FixupRedirectorsIn(const FString& Folder, int32& OutFound, int32& OutFixed, bool bCheckoutFiles)
{
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif
    Filter.PackagePaths.Add(FName(*Folder));
    Filter.bRecursivePaths = true;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets())
    {
        Registry.WaitForCompletion();
    }
    TArray<FAssetData> Found;
    Registry.GetAssets(Filter, Found);
    OutFound = Found.Num();
    OutFixed = 0;
    if (Found.Num() == 0)
    {
        return;
    }
    if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled())
    {
        TArray<FString> PackageNames;
        for (const FAssetData& Asset : Found)
        {
            PackageNames.Add(Asset.PackageName.ToString());
        }
        SourceControlHelpers::CheckOutFiles(PackageNames, true);
    }

    // AssetTools' FixupReferencers always ends on a modal report (SFixupRedirectorsReport) that nobody can
    // answer during an MCP call; unattended it asserted on an unset TOptional and took the editor down twice
    // in a folder move. These are its steps without the UI: resave everything that references a redirector
    // (loading resolves the hard references, soft paths are rewritten), then delete what nothing points at.
    TGuardValue<bool> NoSlowTaskWindows(GIsSilent, true);
    TGuardValue<bool> NoModals(GIsRunningUnattendedScript, true);
    TSet<FName> RedirectorPackages;
    for (const FAssetData& Asset : Found)
    {
        RedirectorPackages.Add(Asset.PackageName);
    }
    TArray<FMcpRedirectorFix> Fixes;
    TMap<FSoftObjectPath, FSoftObjectPath> Remap;
    TSet<UPackage*> Referencers;
    TArray<TWeakObjectPtr<UPackage>> Loaded;
    for (const FAssetData& Asset : Found)
    {
        UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset());
        if (!Redirector)
        {
            continue;
        }
        FMcpRedirectorFix& Fix = Fixes.AddDefaulted_GetRef();
        Fix.Redirector = Redirector;
        // A redirector to nothing has no new path; rewriting its soft references would null them.
        Fix.bKeep = !Redirector->DestinationObject;
        if (Fix.bKeep)
        {
            continue;
        }
        AddRemap(Remap, Redirector);
        TArray<FName> Names;
        Registry.GetReferencers(Asset.PackageName, Names);
        for (const FName& Name : Names)
        {
            if (RedirectorPackages.Contains(Name))
            {
                continue;
            }
            UPackage* Package = FindPackage(nullptr, *Name.ToString());
            if (!Package)
            {
                Package = LoadPackage(nullptr, *Name.ToString(), LOAD_None);
                if (Package)
                {
                    Loaded.Add(Package);
                }
            }
            if (!Package || Package->HasAnyPackageFlags(PKG_CompiledIn))
            {
                Fix.bKeep = true;
                continue;
            }
            Fix.Referencers.Add(Package);
            Referencers.Add(Package);
        }
    }

    const TArray<UPackage*> ToSave = Referencers.Array();
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameReferencingSoftObjectPaths(ToSave, Remap);
    TSet<UPackage*> Failed;
    TArray<FString> SavedFiles;
    for (UPackage* Package : ToSave)
    {
        FString File;
        if (McpSafeOperations::McpSafeAssetSave(Package) && FPackageName::DoesPackageExist(Package->GetName(), &File))
        {
            SavedFiles.Add(FPaths::ConvertRelativePathToFull(File));
        }
        else
        {
            Failed.Add(Package);
        }
    }
    // The delete refuses while the registry still lists an on-disk referencer; it sees the re-saves on a scan.
    Registry.ScanFilesSynchronous(SavedFiles, true);

    TSet<UPackage*> Kept;
    for (FMcpRedirectorFix& Fix : Fixes)
    {
        for (UPackage* Package : Fix.Referencers)
        {
            Fix.bKeep |= Failed.Contains(Package);
        }
        if (Fix.bKeep)
        {
            Kept.Add(Fix.Redirector->GetOutermost());
        }
    }
    TSet<UPackage*> Seen;
    TArray<UObject*> ToDelete;
    for (const FMcpRedirectorFix& Fix : Fixes)
    {
        UPackage* Package = Fix.Redirector->GetOutermost();
        bool bSeen = false;
        Seen.Add(Package, &bSeen);
        if (bSeen || Kept.Contains(Package))
        {
            continue;
        }
        bool bOtherObjects = false;
        ForEachObjectWithOuter(Package, [&ToDelete, &bOtherObjects](UObject* Object)
        {
            if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Object))
            {
                Redirector->RemoveFromRoot();
                ToDelete.Add(Redirector);
            }
            else
            {
                bOtherObjects = true;
            }
        });
        if (!bOtherObjects)
        {
            Package->RemoveFromRoot();
            ToDelete.Add(Package);
        }
    }
    if (ToDelete.Num() > 0)
    {
        ObjectTools::DeleteObjects(ToDelete, false);
    }

    TArray<UPackage*> ToUnload;
    for (const TWeakObjectPtr<UPackage>& Package : Loaded)
    {
        if (Package.IsValid())
        {
            ToUnload.Add(Package.Get());
        }
    }
    FText UnloadError;
    if (ToUnload.Num() > 0 && !UPackageTools::UnloadPackages(ToUnload, UnloadError, true))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("FixupRedirectorsIn: packages loaded for the fixup stayed loaded: %s"),
               *UnloadError.ToString());
    }
    TArray<FAssetData> Left;
    Registry.GetAssets(Filter, Left);
    OutFixed = FMath::Max(0, OutFound - Left.Num());
}
} // namespace McpAssetRename
