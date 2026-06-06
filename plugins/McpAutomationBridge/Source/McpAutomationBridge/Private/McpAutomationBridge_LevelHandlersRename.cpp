#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleRenameLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SourcePath);
    if (SourcePath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
    if (DestinationPath.IsEmpty() && Payload.IsValid()) {
      FString NewName;
      Payload->TryGetStringField(TEXT("newName"), NewName);
      if (!NewName.IsEmpty()) {
        DestinationPath = FPaths::GetPath(NormalizeLevelPackagePath(SourcePath)) / NewName;
      }
    }

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath or sourcePath required for rename_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for rename_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Issue #8: Sanitize paths to prevent traversal attacks
    FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
    if (SanitizedSource.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid source path (traversal/security violation): %s"), *SourcePath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FString SanitizedDest = SanitizeProjectRelativePath(DestinationPath);
    if (SanitizedDest.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid destination path (traversal/security violation): %s"), *DestinationPath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    SourcePath = NormalizeLevelPackagePath(SanitizedSource);
    DestinationPath = NormalizeLevelPackagePath(SanitizedDest);

    if (IsCurrentEditorWorldPackage(SourcePath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Cannot rename the current loaded level: %s"), *SourcePath),
                             nullptr, TEXT("LEVEL_LOADED"));
      return true;
    }

    bool bOverwrite = false;
    if (Payload.IsValid()) {
      Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
    }

    TSharedPtr<FJsonObject> Result;
    FString ErrorMessage;
    FString ErrorCode;
    const bool bCopied = CopyLevelMapPackageFile(SourcePath, DestinationPath, bOverwrite, Result, ErrorMessage, ErrorCode);
    if (!bCopied) {
      SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage, Result, ErrorCode);
      return true;
    }

    FString SourceFilename;
    TryGetAbsoluteMapFilename(SourcePath, SourceFilename);
    bool bDeletedSource = false;
    if (!SourceFilename.IsEmpty()) {
      bDeletedSource = IFileManager::Get().Delete(*SourceFilename, false, true, true);
    }

    const FString SourceBuiltDataPackagePath = SourcePath + TEXT("_BuiltData");
    FString SourceBuiltDataFilename;
    bool bSourceBuiltDataExists = false;
    bool bDeletedBuiltData = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            SourceBuiltDataPackagePath, SourceBuiltDataFilename, FPackageName::GetAssetPackageExtension())) {
      SourceBuiltDataFilename = FPaths::ConvertRelativePathToFull(SourceBuiltDataFilename);
      FPaths::NormalizeFilename(SourceBuiltDataFilename);
      bSourceBuiltDataExists = IFileManager::Get().FileExists(*SourceBuiltDataFilename);
      if (bSourceBuiltDataExists) {
        bDeletedBuiltData = IFileManager::Get().Delete(*SourceBuiltDataFilename, false, true, true);
      }
    }

    bool bSourceExternalActorsExists = false;
    bool bDeletedSourceExternalActors = false;
    bool bSourceExternalObjectsExists = false;
    bool bDeletedSourceExternalObjects = false;
    if (bDeletedSource) {
      if (!DeleteExternalPackageDirectory(SourcePath, TEXT("__ExternalActors__"),
                                          bSourceExternalActorsExists,
                                          bDeletedSourceExternalActors,
                                          ErrorMessage, ErrorCode) ||
          !DeleteExternalPackageDirectory(SourcePath, TEXT("__ExternalObjects__"),
                                          bSourceExternalObjectsExists,
                                          bDeletedSourceExternalObjects,
                                          ErrorMessage, ErrorCode)) {
        Result->SetStringField(TEXT("externalDeleteError"), ErrorMessage);
        SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage,
                               Result, ErrorCode);
        return true;
      }
    }

    ScanLevelPackagePath(SourcePath, SourceFilename);
    const bool bSourceFileExistsAfter = !SourceFilename.IsEmpty() && IFileManager::Get().FileExists(*SourceFilename);
    const bool bRemovedSourceBuiltData = !bSourceBuiltDataExists || bDeletedBuiltData;
    const bool bRemovedSourceExternalActors = !bSourceExternalActorsExists || bDeletedSourceExternalActors;
    const bool bRemovedSourceExternalObjects = !bSourceExternalObjectsExists || bDeletedSourceExternalObjects;
    const bool bRenamed = bDeletedSource && !bSourceFileExistsAfter &&
        bRemovedSourceBuiltData && bRemovedSourceExternalActors &&
        bRemovedSourceExternalObjects;
    Result->SetBoolField(TEXT("renamed"), bRenamed);
    Result->SetBoolField(TEXT("deletedSourceFile"), bDeletedSource);
    Result->SetBoolField(TEXT("sourceBuiltDataExists"), bSourceBuiltDataExists);
    Result->SetBoolField(TEXT("deletedSourceBuiltData"), bDeletedBuiltData);
    Result->SetBoolField(TEXT("sourceExternalActorsExists"), bSourceExternalActorsExists);
    Result->SetBoolField(TEXT("deletedSourceExternalActors"), bDeletedSourceExternalActors);
    Result->SetBoolField(TEXT("sourceExternalObjectsExists"), bSourceExternalObjectsExists);
    Result->SetBoolField(TEXT("deletedSourceExternalObjects"), bDeletedSourceExternalObjects);
    Result->SetBoolField(TEXT("sourceFileExistsAfter"), bSourceFileExistsAfter);

    if (bRenamed) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level renamed to: %s"), *DestinationPath), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to delete source level after copy: %s"), *SourcePath),
                             Result, TEXT("SOURCE_DELETE_FAILED"));
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
