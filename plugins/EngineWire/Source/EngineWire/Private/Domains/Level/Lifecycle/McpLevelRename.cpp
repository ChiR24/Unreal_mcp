#include "Domains/Level/McpLevelActions.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Domains/Level/Copy/McpLevelCopyOperations.h"
#include "Domains/Level/Lifecycle/McpLevelDeletion.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "HAL/FileManager.h"

namespace McpLevelHandlers {
bool HandleRenameLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SourcePath = McpGetFirstStringField(Payload, {TEXT("levelPath"), TEXT("sourcePath")});
    FString DestinationPath = McpGetFirstStringField(Payload, {TEXT("destinationPath")});
    const FString NewName = McpGetFirstStringField(Payload, {TEXT("newName")});
    if (DestinationPath.IsEmpty() && !NewName.IsEmpty()) {
      DestinationPath = FPaths::GetPath(FPackageName::ObjectPathToPackageName(SourcePath)) / NewName;
    }

    if (SourcePath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("levelPath or sourcePath required for rename_level"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("destinationPath required for rename_level"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!SanitizeLevelPathPairOrReply(Subsystem, RequestingSocket, RequestId, SourcePath, DestinationPath,
                                      TEXT("source path"), TEXT("destination path"))) {
      return true;
    }
    // A level that is open in the editor (current world or a streaming sub-level) cannot have its source deleted after
    // the copy, which used to leave both files behind and report SOURCE_DELETE_FAILED. Judged on the sanitized path: one
    // the sanitizer rewrites (no leading slash) passed the test as given and was copied before it failed so.
    if (UWorld* EditorWorld = McpHandlerUtils::GetEditorWorld()) {
      bool bInUse = EditorWorld->GetOutermost()->GetName().Equals(SourcePath, ESearchCase::IgnoreCase);
      for (ULevelStreaming* Streaming : EditorWorld->GetStreamingLevels()) {
        if (Streaming && Streaming->GetWorldAssetPackageName().Equals(SourcePath, ESearchCase::IgnoreCase)) {
          bInUse = true;
        }
      }
      if (bInUse) {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                               FString::Printf(TEXT("Level %s is loaded in the editor (current level or streaming sub-level); unload it before renaming"), *SourcePath),
                               TEXT("LEVEL_IN_USE"));
        return true;
      }
    }

    const bool bOverwrite = GetJsonBoolField(Payload, TEXT("overwrite"), false);

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
    Result->SetBoolField(TEXT("deletedSourceBuiltData"), Deletion.bDeletedBuiltData);
    Result->SetBoolField(TEXT("deletedSourceExternalActors"), Deletion.bDeletedExternalActors);
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
