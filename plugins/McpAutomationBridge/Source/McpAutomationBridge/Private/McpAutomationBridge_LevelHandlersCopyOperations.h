#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
struct FExternalPackageDirectoryCopyPlan {
  FString SourceDirectory;
  FString DestinationDirectory;
  bool bSourceExists = false;
  bool bDestinationExists = false;
  bool bDeletedDestination = false;
  bool bCopied = false;
};

struct FLevelCopyContext {
  FString SourcePackagePath;
  FString DestinationPackagePath;
  FString SourceFilename;
  FString DestinationFilename;
  FString SourceBuiltDataPackagePath;
  FString DestinationBuiltDataPackagePath;
  FString SourceBuiltDataFilename;
  FString DestinationBuiltDataFilename;
  FString DestinationMapBackup;
  FString DestinationBuiltDataBackup;
  FString DestinationExternalActorsBackup;
  FString DestinationExternalObjectsBackup;
  FExternalPackageDirectoryCopyPlan ExternalActorsPlan;
  FExternalPackageDirectoryCopyPlan ExternalObjectsPlan;
  bool bOverwrite = false;
  bool bSourceBuiltDataExists = false;
  bool bDestinationBuiltDataExists = false;
  bool bDeletedDestinationBuiltData = false;
  bool bDestinationMapExists = false;
  bool bDeletedDestinationMap = false;
  bool bCopiedMap = false;
  bool bCopiedBuiltData = false;
};

bool GetExternalPackageDirectory(const FString& PackagePath, const FString& RootDirectoryName, FString& OutDirectory);
bool BuildExternalPackageDirectoryCopyPlan(const FString& SourcePackagePath, const FString& DestinationPackagePath, const FString& RootDirectoryName, bool bOverwrite, FExternalPackageDirectoryCopyPlan& Plan, FString& ErrorMessage, FString& ErrorCode);
bool CopyExternalPackageDirectory(FExternalPackageDirectoryCopyPlan& Plan, FString& ErrorMessage, FString& ErrorCode);
bool DeleteExternalPackageDirectory(const FString& PackagePath, const FString& RootDirectoryName, bool& bSourceExists, bool& bDeleted, FString& ErrorMessage, FString& ErrorCode);
bool BackupFileForOverwrite(const FString& Filename, const TCHAR* Label, bool& bExisted, FString& BackupFilename, FString& ErrorMessage, FString& ErrorCode);
bool RestoreFileBackup(const FString& Filename, const FString& BackupFilename);
void DeleteFileBackup(const FString& BackupFilename);
bool BackupDirectoryForOverwrite(const FString& Directory, const TCHAR* Label, bool& bExisted, FString& BackupDirectory, FString& ErrorMessage, FString& ErrorCode);
bool RestoreDirectoryBackup(const FString& Directory, const FString& BackupDirectory);
void DeleteDirectoryBackup(const FString& BackupDirectory);
bool CopyLevelMapPackageFile(const FString& SourcePackagePath, const FString& DestinationPackagePath, bool bOverwrite, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
bool InitializeLevelCopyContext(const FString& SourcePackagePath, const FString& DestinationPackagePath, bool bOverwrite, FLevelCopyContext& Context, FString& ErrorMessage, FString& ErrorCode);
bool BackupLevelCopyDestinations(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
bool RollbackCopiedDestinationArtifacts(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& RollbackError);
void DeleteLevelCopyDestinationBackups(FLevelCopyContext& Context);
bool CopyLevelMapAndArtifacts(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
void PopulateLevelCopyResult(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result);
#endif
} // namespace McpLevelHandlers
