#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"

#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Safety/McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
bool HandleSaveCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    // "Save, optionally to a target path": a savePath is a save-as. It used to
    // be ignored, saving the open level in place under its old name.
    if (!GetJsonStringField(Payload, TEXT("savePath")).IsEmpty()) {
      return HandleSaveLevelAsAction(Subsystem, RequestId, Payload, RequestingSocket);
    }
    const FString LevelPathError = CheckLevelPathIsOpenLevel(Payload);
    if (!LevelPathError.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, LevelPathError, nullptr, TEXT("LEVEL_NOT_LOADED"));
      return true;
    }

    UWorld *World = McpHandlerUtils::GetEditorWorld();
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // A transient level (unsaved/Untitled) is refused:
    // Saving a transient level causes fatal error: "Attempted to create a package
    // with name containing double slashes" when HLOD/Instancing generates paths
    // like /Game//Temp/Untitled_1_HLOD0_Instancing
    FString PackageName = World->GetOutermost()->GetName();
    const bool bIsTransient = McpSafeOperations::McpIsUnsavedLevelPath(PackageName);

    if (bIsTransient) {
      TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);
      ErrorDetail->SetStringField(TEXT("reason"),
          TEXT("Level is unsaved/temporary. Use save_level_as with a valid path first."));
      ErrorDetail->SetStringField(TEXT("hint"),
          TEXT("Use manage_level with action='save_as' and provide savePath parameter"));
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Cannot save transient level: Level must be saved with 'save_as' first"),
          ErrorDetail, TEXT("TRANSIENT_LEVEL"));
      return true;
    }

    // Use McpSafeLevelSave to prevent Intel GPU driver crashes during save
    // FlushRenderingCommands prevents MONZA DdiThreadingContext exceptions
    // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
    bool bSaved = McpSafeLevelSave(World->PersistentLevel, PackageName);
    if (bSaved) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      FString LevelPath = World->GetOutermost()->GetName();
      // Names the level it saved, so the receipt has its handle and lists it as changed.
      Resp->SetStringField(TEXT("savedAssetPath"), LevelPath);
      VerifyAssetExists(Resp, LevelPath);
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level saved"), Resp, FString());
    } else {
      TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);

      FString Filename;
      FString ErrorReason = TEXT("Unknown save failure");

      // Transient level check already handled above, so this is for other save failures
      if (FPackageName::TryConvertLongPackageNameToFilename(
                     PackageName, Filename,
                     FPackageName::GetMapPackageExtension())) {
        if (IFileManager::Get().IsReadOnly(*Filename)) {
          ErrorReason = TEXT("File is read-only or locked by another process");
          ErrorDetail->SetStringField(TEXT("filename"), Filename);
        } else if (!IFileManager::Get().DirectoryExists(
                       *FPaths::GetPath(Filename))) {
          ErrorReason = TEXT("Target directory does not exist");
          ErrorDetail->SetStringField(TEXT("directory"),
                                      FPaths::GetPath(Filename));
        } else {
          ErrorReason =
              TEXT("Save operation failed - check Output Log for details");
          ErrorDetail->SetStringField(TEXT("filename"), Filename);
        }
      } else {
        ErrorReason = TEXT("Invalid package path");
      }

      ErrorDetail->SetStringField(TEXT("reason"), ErrorReason);
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to save level: %s"), *ErrorReason),
          ErrorDetail, TEXT("SAVE_FAILED"));
    }
    return true;
}
} // namespace McpLevelHandlers
