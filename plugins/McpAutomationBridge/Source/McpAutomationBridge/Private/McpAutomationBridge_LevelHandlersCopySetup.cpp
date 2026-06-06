
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
bool InitializeLevelCopyContext(const FString& SourcePackagePath,
                                const FString& DestinationPackagePath,
                                bool bOverwrite,
                                FLevelCopyContext& Context,
                                FString& ErrorMessage,
                                FString& ErrorCode) {
  Context = FLevelCopyContext{};
  Context.SourcePackagePath = SourcePackagePath;
  Context.DestinationPackagePath = DestinationPackagePath;
  Context.bOverwrite = bOverwrite;
  if (!TryGetAbsoluteMapFilename(SourcePackagePath, Context.SourceFilename)) {
    ErrorMessage = FString::Printf(TEXT("Could not convert source level to filename: %s"), *SourcePackagePath);
    ErrorCode = TEXT("INVALID_SOURCE_PATH");
    return false;
  }
  if (!TryGetAbsoluteMapFilename(DestinationPackagePath, Context.DestinationFilename)) {
    ErrorMessage = FString::Printf(TEXT("Could not convert destination level to filename: %s"), *DestinationPackagePath);
    ErrorCode = TEXT("INVALID_DESTINATION_PATH");
    return false;
  }
  if (!ValidateWritableGameMapPath(SourcePackagePath, Context.SourceFilename,
                                   TEXT("Source"), ErrorMessage, ErrorCode) ||
      !ValidateWritableGameMapPath(DestinationPackagePath,
                                   Context.DestinationFilename,
                                   TEXT("Destination"), ErrorMessage,
                                   ErrorCode)) {
    return false;
  }
  if (Context.SourceFilename.Equals(Context.DestinationFilename,
                                    ESearchCase::IgnoreCase)) {
    ErrorMessage = FString::Printf(
        TEXT("Source and destination levels are identical: %s"), *SourcePackagePath);
    ErrorCode = TEXT("SAME_PATH");
    return false;
  }

  IFileManager& FileManager = IFileManager::Get();
  if (!FileManager.FileExists(*Context.SourceFilename)) {
    ErrorMessage = FString::Printf(TEXT("Source level file not found: %s"), *SourcePackagePath);
    ErrorCode = TEXT("SOURCE_NOT_FOUND");
    return false;
  }

  Context.SourceBuiltDataPackagePath = SourcePackagePath + TEXT("_BuiltData");
  Context.DestinationBuiltDataPackagePath = DestinationPackagePath + TEXT("_BuiltData");
  if (FPackageName::TryConvertLongPackageNameToFilename(
          Context.SourceBuiltDataPackagePath, Context.SourceBuiltDataFilename,
          FPackageName::GetAssetPackageExtension()) &&
      FPackageName::TryConvertLongPackageNameToFilename(
          Context.DestinationBuiltDataPackagePath,
          Context.DestinationBuiltDataFilename,
          FPackageName::GetAssetPackageExtension())) {
    Context.SourceBuiltDataFilename = FPaths::ConvertRelativePathToFull(Context.SourceBuiltDataFilename);
    Context.DestinationBuiltDataFilename = FPaths::ConvertRelativePathToFull(Context.DestinationBuiltDataFilename);
    FPaths::NormalizeFilename(Context.SourceBuiltDataFilename);
    FPaths::NormalizeFilename(Context.DestinationBuiltDataFilename);
    Context.bSourceBuiltDataExists = FileManager.FileExists(*Context.SourceBuiltDataFilename);
    Context.bDestinationBuiltDataExists = FileManager.FileExists(*Context.DestinationBuiltDataFilename);
    if (Context.bSourceBuiltDataExists &&
        Context.SourceBuiltDataFilename.Equals(Context.DestinationBuiltDataFilename,
                                               ESearchCase::IgnoreCase)) {
      ErrorMessage = FString::Printf(
          TEXT("Source and destination built data are identical: %s"),
          *Context.SourceBuiltDataPackagePath);
      ErrorCode = TEXT("SAME_PATH");
      return false;
    }
    if (Context.bDestinationBuiltDataExists && !bOverwrite) {
      ErrorMessage = FString::Printf(
          TEXT("Destination built data already exists: %s"),
          *Context.DestinationBuiltDataPackagePath);
      ErrorCode = TEXT("DESTINATION_EXISTS");
      return false;
    }
  }

  if (!BuildExternalPackageDirectoryCopyPlan(
          SourcePackagePath, DestinationPackagePath, TEXT("__ExternalActors__"),
          bOverwrite, Context.ExternalActorsPlan, ErrorMessage, ErrorCode) ||
      !BuildExternalPackageDirectoryCopyPlan(
          SourcePackagePath, DestinationPackagePath, TEXT("__ExternalObjects__"),
          bOverwrite, Context.ExternalObjectsPlan, ErrorMessage, ErrorCode)) {
    return false;
  }

  Context.bDestinationMapExists = FileManager.FileExists(*Context.DestinationFilename);
  if (Context.bDestinationMapExists && !bOverwrite) {
    ErrorMessage = FString::Printf(TEXT("Destination level already exists: %s"), *DestinationPackagePath);
    ErrorCode = TEXT("DESTINATION_EXISTS");
    return false;
  }
  if (bOverwrite && FindPackage(nullptr, *DestinationPackagePath) != nullptr) {
    ErrorMessage = FString::Printf(
        TEXT("Destination level package is loaded and cannot be overwritten: %s"),
        *DestinationPackagePath);
    ErrorCode = TEXT("DESTINATION_LOADED");
    return false;
  }
  return true;
}
#endif
} // namespace McpLevelHandlers
