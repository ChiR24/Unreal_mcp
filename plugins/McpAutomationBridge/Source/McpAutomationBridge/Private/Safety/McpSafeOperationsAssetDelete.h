#pragma once

#include "Safety/McpSafeOperationsDeleteCompilation.h"
#include "Safety/McpSafeOperationsMaterial.h"

#include "EditorAssetLibrary.h"
#include "UObject/Linker.h"

namespace McpSafeOperations
{


/**
 * UEditorAssetLibrary::DeleteAsset can succeed without removing the file: for
 * a Blueprint loaded from disk the objects went, the .uasset stayed, the asset
 * left the registry until the next editor start and then came back.
 * (ObjectTools::CleanupAfterSuccessfulDelete skips the file of any package it
 * still finds referenced, and leaves that package loaded.)
 * McpRemoveLeftoverPackageFile finishes such a delete the way
 * DeleteWorldPackagesByPath does: unload the package, and only once it really
 * is unloaded, remove the file. True only when the file is gone.
 */
inline bool McpRemoveLeftoverPackageFile(const FString& PackageName)
{
    FString Filename;
    if (!FPackageName::DoesPackageExist(PackageName, &Filename))
    {
        return true;
    }

    UE_LOG(LogMcpSafeOperations, Warning,
        TEXT("McpRemoveLeftoverPackageFile: '%s' was deleted in memory but its file remained; unloading the package and removing the file"),
        *PackageName);
    if (UPackage* Leftover = FindObject<UPackage>(nullptr, *PackageName))
    {
        FText UnloadError;
        UPackageTools::UnloadPackages({Leftover}, UnloadError, true);
    }
    if (FindObject<UPackage>(nullptr, *PackageName))
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpRemoveLeftoverPackageFile: '%s' is still loaded, so its file was kept"), *PackageName);
        return false;
    }

    // A package destroyed by GC only queues its linker, which keeps the file
    // open until the queue is flushed; delete while it is open fails on Windows.
    DeleteLoaders();
    // Not quiet: a refusal logs the OS error code.
    const bool bDeleted = IFileManager::Get().Delete(*FPaths::ConvertRelativePathToFull(Filename), false, true, false);
    ScanPathSynchronous(FPaths::GetPath(PackageName), false);
    // A file something still holds open is removed when the last handle closes, so it can show a while longer: a
    // delete the system accepted is done. A struct reported "still on disk" was gone at the next editor start.
    return bDeleted || !FPackageName::DoesPackageExist(PackageName);
}

/** Runs the engine delete, then removes a file it left behind. */
inline bool McpDeleteAssetAndFile(const FString& AssetPath)
{
    return UEditorAssetLibrary::DeleteAsset(AssetPath)
        && McpRemoveLeftoverPackageFile(FPackageName::ObjectPathToPackageName(AssetPath));
}


}
