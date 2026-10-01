#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Editor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Domains/Level/Copy/McpAutomationBridge_LevelHandlersCopyOperations.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDeletion.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "HAL/FileManager.h"

namespace McpLevelHandlers {
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
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath or sourcePath required for rename_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for rename_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // A level that is open in the editor (current world or a streaming
    // sub-level) cannot have its source deleted after the copy, which used to
    // leave both files behind and report SOURCE_DELETE_FAILED (dogfood #154).
    if (GEditor) {
      if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World()) {
        const FString SourcePackage = NormalizeLevelPackagePath(SourcePath);
        bool bInUse = EditorWorld->GetOutermost()->GetName().Equals(SourcePackage, ESearchCase::IgnoreCase);
        for (ULevelStreaming* Streaming : EditorWorld->GetStreamingLevels()) {
          if (Streaming && Streaming->GetWorldAssetPackageName().Equals(SourcePackage, ESearchCase::IgnoreCase)) {
            bInUse = true;
          }
        }
        if (bInUse) {
          Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                                 FString::Printf(TEXT("Level %s is loaded in the editor (current level or streaming sub-level); unload it before renaming"), *SourcePackage),
                                 nullptr, TEXT("LEVEL_IN_USE"));
          return true;
        }
      }
    }

    // Issue #8: Sanitize paths to prevent traversal attacks
    FString SanitizedSource = SanitizeProjectRelativePath(SourcePath);
    if (SanitizedSource.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             McpPathRefusalMessage(TEXT("source path"), SourcePath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    FString SanitizedDest = SanitizeProjectRelativePath(DestinationPath);
    if (SanitizedDest.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             McpPathRefusalMessage(TEXT("destination path"), DestinationPath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    SourcePath = NormalizeLevelPackagePath(SanitizedSource);
    DestinationPath = NormalizeLevelPackagePath(SanitizedDest);

    if (IsCurrentEditorWorldPackage(SourcePath)) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
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
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMessage, Result, ErrorCode);
      return true;
    }

    FLevelFileDeletion Deletion;
    DeleteLevelFiles(SourcePath, true, Deletion);
    if (!Deletion.SidecarErrorMessage.IsEmpty()) {
      Result->SetStringField(TEXT("externalDeleteError"), Deletion.SidecarErrorMessage);
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Deletion.SidecarErrorMessage,
                             Result, Deletion.SidecarErrorCode);
      return true;
    }

    ScanLevelPackagePath(SourcePath, Deletion.MapFilename);
    const bool bSourceFileExistsAfter = !Deletion.MapFilename.IsEmpty() &&
                                        IFileManager::Get().FileExists(*Deletion.MapFilename);
    const bool bRenamed = Deletion.bDeletedMap && !bSourceFileExistsAfter && Deletion.SidecarsRemoved();
    Result->SetBoolField(TEXT("renamed"), bRenamed);
    Result->SetBoolField(TEXT("deletedSourceFile"), Deletion.bDeletedMap);
    Result->SetBoolField(TEXT("sourceBuiltDataExists"), Deletion.bBuiltDataExists);
    Result->SetBoolField(TEXT("deletedSourceBuiltData"), Deletion.bDeletedBuiltData);
    Result->SetBoolField(TEXT("sourceExternalActorsExists"), Deletion.bExternalActorsExists);
    Result->SetBoolField(TEXT("deletedSourceExternalActors"), Deletion.bDeletedExternalActors);
    Result->SetBoolField(TEXT("sourceExternalObjectsExists"), Deletion.bExternalObjectsExists);
    Result->SetBoolField(TEXT("deletedSourceExternalObjects"), Deletion.bDeletedExternalObjects);
    Result->SetBoolField(TEXT("sourceFileExistsAfter"), bSourceFileExistsAfter);

    if (bRenamed) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level renamed to: %s"), *DestinationPath), Result);
    } else {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to delete source level after copy: %s"), *SourcePath),
                             Result, TEXT("SOURCE_DELETE_FAILED"));
    }
    return true;
}
} // namespace McpLevelHandlers
