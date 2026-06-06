#pragma once

#include "McpSafeOperationsLog.h"

#if WITH_EDITOR
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#endif

namespace McpSafeOperations
{

#if WITH_EDITOR

inline bool McpSafeLevelSave(ULevel* Level, const FString& FullPath, int32 MaxRetries = 5)
{
    if (!Level)
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLevelSave: Level is null"));
        return false;
    }

    if (FullPath.StartsWith(TEXT("/Temp/")) ||
        FullPath.StartsWith(TEXT("/Engine/Transient")) ||
        FullPath.Contains(TEXT("Untitled")))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeLevelSave: Cannot save transient level: %s. Use save_as with a valid path."),
            *FullPath);
        return false;
    }

    FString PackagePath = FullPath;
    if (!PackagePath.StartsWith(TEXT("/Game/")))
    {
        if (!PackagePath.StartsWith(TEXT("/")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeLevelSave: Invalid path (not under /Game/): %s"), *PackagePath);
            return false;
        }
    }

    if (PackagePath.Contains(TEXT("//")))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeLevelSave: Path contains double slashes: %s"), *PackagePath);
        return false;
    }

    if (PackagePath.Contains(TEXT(".")))
    {
        PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
    }

    {
        FString AbsoluteFilePath;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AbsoluteFilePath,
            FPackageName::GetMapPackageExtension()))
        {
            AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
            const int32 SafePathLength = 240;
            if (AbsoluteFilePath.Len() > SafePathLength)
            {
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeLevelSave: Path too long (%d chars, max %d): %s"),
                    AbsoluteFilePath.Len(), SafePathLength, *AbsoluteFilePath);
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeLevelSave: Use a shorter path or enable Windows long paths"));
                return false;
            }
        }
    }

    {
        FString ExistingLevelFilename;
        bool bLevelExists = false;

        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, ExistingLevelFilename,
            FPackageName::GetMapPackageExtension()))
        {
            FString AbsolutePath = FPaths::ConvertRelativePathToFull(ExistingLevelFilename);
            bLevelExists = IFileManager::Get().FileExists(*AbsolutePath);

            if (!bLevelExists)
            {
                FString LevelName = FPaths::GetBaseFilename(PackagePath);
                FString FolderPath = FPaths::GetPath(AbsolutePath) / LevelName + FPackageName::GetMapPackageExtension();
                bLevelExists = IFileManager::Get().FileExists(*FolderPath);
            }
        }

        if (!bLevelExists)
        {
            bLevelExists = FPackageName::DoesPackageExist(PackagePath);
        }

        if (bLevelExists)
        {
            UWorld* LevelWorld = Level ? Level->GetWorld() : nullptr;
            if (LevelWorld)
            {
                FString CurrentLevelPath = LevelWorld->GetOutermost()->GetName();
                if (CurrentLevelPath.Equals(PackagePath, ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogMcpSafeOperations, Log,
                        TEXT("McpSafeLevelSave: Overwriting existing level: %s"), *PackagePath);
                }
                else
                {
                    UE_LOG(LogMcpSafeOperations, Warning,
                        TEXT("McpSafeLevelSave: Level already exists at %s (current level is %s)"),
                        *PackagePath, *CurrentLevelPath);
                    return false;
                }
            }
        }
    }

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.050f);

    FString SaveFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, SaveFilename,
        FPackageName::GetMapPackageExtension()))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeLevelSave: Failed to convert package path to filename: %s"), *PackagePath);
        return false;
    }

    bool bSaveSucceeded = FEditorFileUtils::SaveLevel(Level, *SaveFilename);
    if (bSaveSucceeded)
    {
        FString VerifyFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, VerifyFilename,
            FPackageName::GetMapPackageExtension()))
        {
            FString AbsoluteVerifyFilename = FPaths::ConvertRelativePathToFull(VerifyFilename);

            const int32 ActualRetries = FMath::Clamp(MaxRetries, 1, 10);
            const float RetryDelays[] = { 0.05f, 0.10f, 0.25f, 0.50f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f };

            for (int32 Retry = 0; Retry < ActualRetries; ++Retry)
            {
                FPlatformProcess::Sleep(RetryDelays[Retry]);

                if (IFileManager::Get().FileExists(*VerifyFilename) ||
                    IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
                {
                    UE_LOG(LogMcpSafeOperations, Log,
                        TEXT("McpSafeLevelSave: Successfully saved level: %s"), *PackagePath);
                    return true;
                }

                if (FPackageName::DoesPackageExist(PackagePath))
                {
                    UE_LOG(LogMcpSafeOperations, Log,
                        TEXT("McpSafeLevelSave: Package exists in UE system: %s"), *PackagePath);
                    return true;
                }
            }

            FlushRenderingCommands();
            FPlatformProcess::Sleep(0.5f);
            if (IFileManager::Get().FileExists(*VerifyFilename) ||
                IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpSafeLevelSave: Successfully saved level after final flush: %s"), *PackagePath);
                return true;
            }

            if (FPackageName::DoesPackageExist(PackagePath))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpSafeLevelSave: Package exists in UE system after final flush: %s"), *PackagePath);
                return true;
            }

            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeLevelSave: Save reported success but file not found after %d retries: %s"),
                ActualRetries, *VerifyFilename);
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeLevelSave: Failed to convert package path to filename: %s"), *PackagePath);
        }
    }

    UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLevelSave: Failed to save level: %s"), *PackagePath);
    return false;
}

#endif

}
