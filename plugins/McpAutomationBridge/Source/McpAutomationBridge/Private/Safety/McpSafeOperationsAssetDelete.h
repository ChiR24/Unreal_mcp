#pragma once

#include "Safety/McpSafeOperationsDeleteCompilation.h"
#include "Safety/McpSafeOperationsMaterial.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "UObject/Linker.h"
#endif

namespace McpSafeOperations
{

#if WITH_EDITOR

/**
 * UEditorAssetLibrary::DeleteAsset can succeed without removing the file: for
 * a Blueprint loaded from disk the objects went, the .uasset stayed, the asset
 * left the registry until the next editor start and then came back.
 * (ObjectTools::CleanupAfterSuccessfulDelete skips the file of any package it
 * still finds referenced, and leaves that package loaded.)
 * This runs the engine delete and, when the package file survives it, finishes
 * the job the way DeleteWorldPackagesByPath does: unload the package, and only
 * once it really is unloaded, remove the file. True only when the file is gone.
 */
inline bool McpDeleteAssetAndFile(const FString& AssetPath)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    if (!UEditorAssetLibrary::DeleteAsset(AssetPath))
    {
        return false;
    }

    FString Filename;
    if (!FPackageName::DoesPackageExist(PackageName, &Filename))
    {
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Warning,
        TEXT("McpDeleteAssetAndFile: '%s' was deleted in memory but its file remained; unloading the package and removing the file"),
        *PackageName);
#if MCP_HAS_PACKAGE_TOOLS
    if (UPackage* Leftover = FindObject<UPackage>(nullptr, *PackageName))
    {
        FText UnloadError;
        UPackageTools::UnloadPackages({Leftover}, UnloadError, true);
    }
#endif
    if (FindObject<UPackage>(nullptr, *PackageName))
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpDeleteAssetAndFile: '%s' is still loaded, so its file was kept"), *PackageName);
        return false;
    }

    // A package destroyed by GC only queues its linker, which keeps the file
    // open until the queue is flushed; delete while it is open fails on Windows.
    DeleteLoaders();
    // Not quiet: a refusal logs the OS error code.
    IFileManager::Get().Delete(*FPaths::ConvertRelativePathToFull(Filename), false, true, false);
    ScanPathSynchronous(FPaths::GetPath(PackageName), false);
    return !FPackageName::DoesPackageExist(PackageName);
}

#endif

}
