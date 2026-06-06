#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"

#include "McpAutomationBridgeHelpersResponseVerification.h"
#include "McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleSaveCurrentLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // CRITICAL: Check if the current level is transient (unsaved/Untitled)
    // Saving a transient level causes fatal error: "Attempted to create a package
    // with name containing double slashes" when HLOD/Instancing generates paths
    // like /Game//Temp/Untitled_1_HLOD0_Instancing
    FString PackageName = World->GetOutermost()->GetName();
    bool bIsTransient = PackageName.StartsWith(TEXT("/Temp/")) ||
                        PackageName.StartsWith(TEXT("/Engine/Transient")) ||
                        PackageName.Contains(TEXT("Untitled"));

    if (bIsTransient) {
      TSharedPtr<FJsonObject> ErrorDetail = McpHandlerUtils::CreateResultObject();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);
      ErrorDetail->SetStringField(TEXT("reason"),
          TEXT("Level is unsaved/temporary. Use save_level_as with a valid path first."));
      ErrorDetail->SetStringField(TEXT("hint"),
          TEXT("Use manage_level with action='save_as' and provide savePath parameter"));
      SendAutomationResponse(
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
      VerifyAssetExists(Resp, LevelPath);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level saved"), Resp, FString());
    } else {
      // Provide detailed error information
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
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to save level: %s"), *ErrorReason),
          ErrorDetail, TEXT("SAVE_FAILED"));
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
