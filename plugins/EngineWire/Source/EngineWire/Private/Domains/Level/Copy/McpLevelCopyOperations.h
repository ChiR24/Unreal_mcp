#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpLevelHandlers {
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
bool DeleteExternalPackageDirectory(const FString& PackagePath, const FString& RootDirectoryName, bool& bSourceExists, bool& bDeleted, FString& ErrorMessage, FString& ErrorCode);
// A file (bDirectory false) or directory tree removed from disk.
bool DeleteLevelPath(const FString& Path, bool bDirectory);
// From copied to To (a file, or a directory tree when bDirectory), To's parent folder made first.
bool CopyPath(const FString& To, const FString& From, bool bDirectory);
// Moves an existing Path aside to BackupPath (empty when Path did not exist) so a copy can overwrite it.
bool BackupForOverwrite(const FString& Path, bool bDirectory, const TCHAR* Label, bool& bExisted, FString& BackupPath, FString& ErrorMessage, FString& ErrorCode);
// Puts BackupPath back at Path; true when there was nothing to restore.
bool RestoreBackup(const FString& Path, const FString& BackupPath, bool bDirectory);
bool CopyLevelMapPackageFile(const FString& SourcePackagePath, const FString& DestinationPackagePath, bool bOverwrite, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
bool InitializeLevelCopyContext(const FString& SourcePackagePath, const FString& DestinationPackagePath, bool bOverwrite, FLevelCopyContext& Context, FString& ErrorMessage, FString& ErrorCode);
bool BackupLevelCopyDestinations(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
bool RollbackCopiedDestinationArtifacts(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& RollbackError);
void DeleteLevelCopyDestinationBackups(FLevelCopyContext& Context);
bool CopyLevelMapAndArtifacts(FLevelCopyContext& Context, TSharedPtr<FJsonObject>& Result, FString& ErrorMessage, FString& ErrorCode);
// Source and Destination through the project sanitizer, then as level package paths. A path the sanitizer refuses
// (the source is judged first) is answered SECURITY_VIOLATION worded with its field text, and false returned.
bool SanitizeLevelPathPairOrReply(UEngineWireSubsystem& Subsystem, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
                                  FString& Source, FString& Destination, const TCHAR* SourceField, const TCHAR* DestinationField);
} // namespace McpLevelHandlers
