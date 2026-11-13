#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace McpSafeOperations
{
// Where the level at LevelPath (a long package name) is saved, as full file paths: Folder is <Path>/<Name>/<Name>.umap,
// the form the editor writes, Flat is <Path>.umap; both empty for a path no mount knows. LoadMap on a level with no
// file crashed the editor ("Pure virtual not implemented"), so a load looks for the file first.
struct FMcpLevelFile
{
    FString Folder;
    FString Flat;
    bool bFolder = false; // the folder form exists (it wins)
    bool bFlat = false;   // only the flat form exists
};

inline FMcpLevelFile McpFindLevelFile(const FString& LevelPath)
{
    FMcpLevelFile File;
    FString Flat;
    if (FPackageName::TryConvertLongPackageNameToFilename(LevelPath, Flat, FPackageName::GetMapPackageExtension()))
    {
        const FString Name = FPaths::GetBaseFilename(LevelPath);
        File.Folder = FPaths::ConvertRelativePathToFull(FPaths::GetPath(Flat) / Name / (Name + FPackageName::GetMapPackageExtension()));
        File.Flat = FPaths::ConvertRelativePathToFull(Flat);
        File.bFolder = IFileManager::Get().FileExists(*File.Folder);
        File.bFlat = !File.bFolder && IFileManager::Get().FileExists(*File.Flat);
    }
    return File;
}

// FILE_NOT_FOUND's message for a level McpFindLevelFile found no file for; levelPath, the files checked and a hint in
// OutDetails.
inline FString McpLevelFileNotFound(const FString& LevelPath, const FMcpLevelFile& File, TSharedPtr<FJsonObject>& OutDetails)
{
    OutDetails = MakeShared<FJsonObject>();
    OutDetails->SetStringField(TEXT("levelPath"), LevelPath);
    if (!File.Flat.IsEmpty())
    {
        OutDetails->SetStringField(TEXT("checkedFolderBased"), File.Folder);
        OutDetails->SetStringField(TEXT("checkedFlat"), File.Flat);
    }
    OutDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    return FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"), *File.Folder, *File.Flat);
}
} // namespace McpSafeOperations
