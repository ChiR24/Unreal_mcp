#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Safety/McpSafeOperationsLog.h"
#include "PackageTools.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CoreGlobals.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"

namespace McpSafeOperations
{
// Refreshes the Asset Registry for one package or folder path right away.
inline void ScanPathSynchronous(const FString& InPath, bool bRecursive = true)
{
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().ScanPathsSynchronous({InPath}, bRecursive);
}

// Whether PackageName has a .uasset or .umap file on disk.
inline bool McpPackageHasBackingFile(const FString& PackageName)
{
    FString Filename;
    for (const FString& Extension : {FPackageName::GetAssetPackageExtension(), FPackageName::GetMapPackageExtension()})
    {
        if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, Extension) &&
            IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(Filename)))
        {
            return true;
        }
    }
    return false;
}

inline bool McpSafeAssetSave(UObject* Asset)
{
    if (!Asset)
    {
        return false;
    }

    UObject* AssetToSave = Asset;
    UPackage* Package = Cast<UPackage>(Asset);
    if (Package)
    {
        AssetToSave = nullptr;
        ForEachObjectWithPackage(Package, [&AssetToSave](UObject* Object) -> bool
        {
            if (Object && !Object->IsA<UPackage>() && Object->HasAnyFlags(RF_Public | RF_Standalone))
            {
                AssetToSave = Object;
                return false;
            }
            return true;
        }, MCP_GET_OBJECTS_NO_NESTED);
    }
    else
    {
        Package = Asset->GetOutermost();
    }
    if (!Package)
    {
        return false;
    }

    const FString PackageName = Package->GetName();
    if (PackageName.StartsWith(TEXT("/Temp/")) ||
        PackageName.StartsWith(TEXT("/Transient/")) ||
        PackageName.StartsWith(TEXT("/Engine/Transient")) ||
        Package->HasAnyFlags(RF_Transient))
    {
        return false;
    }

    Package->SetDirtyFlag(true);
    if (AssetToSave && AssetToSave != Package)
    {
        AssetToSave->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(AssetToSave);
    }

    // Nobody can answer a modal during an MCP call. A save that failed opened the
    // editor's message or checkout dialog and blocked the game thread until the
    // process was killed (a build_graph batch on BP_LaserGate, 2026-09-25).
    // Unattended, PromptForCheckoutAndSave saves directly and a dialog only logs.
    TGuardValue<bool> UnattendedSave(GIsRunningUnattendedScript, true);
    if (AssetToSave && AssetToSave != Package)
    {
        TArray<UObject*> ObjectsToSave;
        ObjectsToSave.Add(AssetToSave);

        FlushRenderingCommands();

        const bool bSaved = UPackageTools::SavePackagesForObjects(ObjectsToSave);
        if (bSaved && McpPackageHasBackingFile(PackageName))
        {
            ScanPathSynchronous(FPaths::GetPath(PackageName), false);
            return true;
        }

        if (bSaved)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeAssetSave: SavePackagesForObjects reported success but no package file exists for %s; trying package save fallback"),
                *PackageName);
        }
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);
    const FEditorFileUtils::EPromptReturnCode PromptSaveResult =
        FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    const bool bPromptSaveSucceeded =
        PromptSaveResult == FEditorFileUtils::PR_Success;
    const bool bEditorSaveSucceeded =
        !bPromptSaveSucceeded && UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
    const bool bExistsOnDisk = McpPackageHasBackingFile(PackageName);

    if ((bPromptSaveSucceeded || bEditorSaveSucceeded) && bExistsOnDisk)
    {
        ScanPathSynchronous(FPaths::GetPath(PackageName), false);
        return true;
    }

    return false;
}


}
