#pragma once

#include "McpSafeOperationsLog.h"
#include "McpSafeOperationsPackageTools.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#endif

namespace McpSafeOperations
{

#if WITH_EDITOR

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
        }, false);
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

    auto ScanSavedPackage = [&PackageName]()
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(FPaths::GetPath(PackageName));
        FAssetRegistryModule& AssetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistryModule.Get().ScanPathsSynchronous(PathsToScan, false);
    };

    auto PackageExistsOnDisk = [&PackageName]()
    {
        FString AssetFilename;
        FString MapFilename;
        const bool bHasAssetFilename = FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, AssetFilename, FPackageName::GetAssetPackageExtension());
        const bool bHasMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, MapFilename, FPackageName::GetMapPackageExtension());

        return
            (bHasAssetFilename && IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(AssetFilename))) ||
            (bHasMapFilename && IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(MapFilename)));
    };

#if MCP_HAS_PACKAGE_TOOLS
    if (AssetToSave && AssetToSave != Package)
    {
        TArray<UObject*> ObjectsToSave;
        ObjectsToSave.Add(AssetToSave);

        FlushRenderingCommands();

        const bool bSaved = UPackageTools::SavePackagesForObjects(ObjectsToSave);
        if (bSaved && PackageExistsOnDisk())
        {
            ScanSavedPackage();
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
    const bool bExistsOnDisk = PackageExistsOnDisk();

    if (bPromptSaveSucceeded || bEditorSaveSucceeded || bExistsOnDisk)
    {
        ScanSavedPackage();
        return true;
    }

    return false;
#else
    return false;
#endif
}

#endif

}
