#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"

#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RenderingThread.h"

#include "McpAutomationBridgeHelpersResponseVerification.h"
#include "McpSafeOperationsMapLoad.h"

using McpSafeOperations::McpSafeLoadMap;

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleLoadLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
      // Map to Open command
      FString LevelPath;
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      bool bSaveDirtyPackages = false;
      Payload->TryGetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);

      // Determine invalid characters for checks
      if (LevelPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("levelPath required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      // SECURITY: Sanitize LevelPath to prevent path traversal attacks
      FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
      if (SanitizedLevelPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid levelPath: contains path traversal (..) or invalid characters"),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      LevelPath = SanitizedLevelPath;

      // Auto-resolve short names
      if (!LevelPath.StartsWith(TEXT("/")) && !FPaths::FileExists(LevelPath)) {
        FString TryPath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelPath);
        if (FPackageName::DoesPackageExist(TryPath)) {
          LevelPath = TryPath;
        }
      }

#if WITH_EDITOR
      if (!GEditor) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), nullptr,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }

      // Try to resolve package path to filename
      FString Filename;
      bool bGotFilename = false;
      if (FPackageName::IsPackageFilename(LevelPath)) {
        Filename = LevelPath;
        bGotFilename = true;
      } else {
        // Assume package path
        if (FPackageName::TryConvertLongPackageNameToFilename(
                LevelPath, Filename, FPackageName::GetMapPackageExtension())) {
          bGotFilename = true;
        }
      }

      // If conversion failed, it might be a short name? But LoadMap usually
      // needs full path. Let's try to load what we have if conversion returned
      // something, else fallback to input.
      const FString FileToLoad = bGotFilename ? Filename : LevelPath;
      FString ResolvedFileToLoad = FileToLoad;
      FString ExpectedLoadedPath = LevelPath;

      // Verify file exists before attempting load to avoid false positives
      // CRITICAL: Unreal stores levels in TWO possible path patterns:
      // 1. Folder-based (standard UE 5.x): /Game/Path/LevelName/LevelName.umap
      // 2. Flat (legacy): /Game/Path/LevelName.umap
      // We must check BOTH paths before returning FILE_NOT_FOUND to prevent
      // the "Pure virtual not implemented" crash when LoadMap fails.

      FString FilenameToCheck;
      bool bFileExists = false;

      // Build both possible paths
      FString FlatMapPath, FullFlatMapPath, FolderMapPath, FullFolderMapPath;
      if (FPackageName::TryConvertLongPackageNameToFilename(
              LevelPath, FlatMapPath, FPackageName::GetMapPackageExtension())) {
        FullFlatMapPath = FPaths::ConvertRelativePathToFull(FlatMapPath);

        // Also build folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
        FString LevelName = FPaths::GetBaseFilename(LevelPath);
        FolderMapPath = FPaths::GetPath(FlatMapPath) / LevelName / (LevelName + FPackageName::GetMapPackageExtension());
        FullFolderMapPath = FPaths::ConvertRelativePathToFull(FolderMapPath);
      }

      // Check both paths - prefer folder-based (UE 5.x standard)
      if (!FullFolderMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFolderMapPath)) {
        bFileExists = true;
        ResolvedFileToLoad = FolderMapPath;
        const FString LevelName = FPaths::GetBaseFilename(LevelPath);
        ExpectedLoadedPath = FPaths::GetPath(LevelPath) / LevelName / LevelName;
        UE_LOG(LogTemp, Log, TEXT("load: Found level at folder-based path: %s"), *FullFolderMapPath);
      } else if (!FullFlatMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFlatMapPath)) {
        bFileExists = true;
        ResolvedFileToLoad = FlatMapPath;
        ExpectedLoadedPath = LevelPath;
        UE_LOG(LogTemp, Log, TEXT("load: Found level at flat path: %s"), *FullFlatMapPath);
      }

      // Also check if it's a valid package path (for levels in memory but not on disk yet)
      if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
        TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
        ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
        if (!FullFolderMapPath.IsEmpty()) {
          ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
        }
        if (!FullFlatMapPath.IsEmpty()) {
          ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
        }
        ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"),
                          *FullFolderMapPath, *FullFlatMapPath),
            ErrorDetails, TEXT("FILE_NOT_FOUND"));
        return true;
      }

      // Force any pending work to complete
      FlushRenderingCommands();

      bool bSavedDirtyPackagesBeforeLoad = false;
      int32 DirtyWorldPackagesBeforeLoad = 0;
      int32 DirtyContentPackagesBeforeLoad = 0;
      int32 DirtyWorldPackagesAfterSave = 0;
      int32 DirtyContentPackagesAfterSave = 0;
      int32 FailedDirtyPackageSaves = 0;
      if (FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) {
        CountBlockingDirtyPackages(DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad);
        if (DirtyWorldPackagesBeforeLoad + DirtyContentPackagesBeforeLoad > 0 && !bSaveDirtyPackages) {
          TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
          ErrorDetails->SetNumberField(TEXT("dirtyWorldPackages"), DirtyWorldPackagesBeforeLoad);
          ErrorDetails->SetNumberField(TEXT("dirtyContentPackages"), DirtyContentPackagesBeforeLoad);
          ErrorDetails->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
          ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
          SendAutomationResponse(
              RequestingSocket, RequestId, false,
              TEXT("Cannot load a level in unattended/headless mode while packages are dirty. Pass saveDirtyPackages=true to save them before loading."),
              ErrorDetails, TEXT("DIRTY_PACKAGES"));
          return true;
        }

        if (bSaveDirtyPackages) {
        bSavedDirtyPackagesBeforeLoad = SaveBlockingDirtyPackagesForLevelLoad(
            DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad,
            DirtyWorldPackagesAfterSave, DirtyContentPackagesAfterSave,
            FailedDirtyPackageSaves);
        if (!bSavedDirtyPackagesBeforeLoad) {
          TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
          ErrorDetails->SetNumberField(TEXT("dirtyWorldPackagesBeforeSave"), DirtyWorldPackagesBeforeLoad);
          ErrorDetails->SetNumberField(TEXT("dirtyContentPackagesBeforeSave"), DirtyContentPackagesBeforeLoad);
          ErrorDetails->SetNumberField(TEXT("dirtyWorldPackages"), DirtyWorldPackagesAfterSave);
          ErrorDetails->SetNumberField(TEXT("dirtyContentPackages"), DirtyContentPackagesAfterSave);
          ErrorDetails->SetNumberField(TEXT("failedPackageSaves"), FailedDirtyPackageSaves);
          ErrorDetails->SetBoolField(TEXT("saveDirtyPackagesSucceeded"), bSavedDirtyPackagesBeforeLoad);
          ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
          SendAutomationResponse(
              RequestingSocket, RequestId, false,
              TEXT("Cannot load a level in unattended/headless mode while packages remain dirty after non-interactive save."),
              ErrorDetails, TEXT("DIRTY_PACKAGES"));
          return true;
        }
        }
      }

      const bool bLoaded = McpSafeLoadMap(ResolvedFileToLoad);

      // Post-load verification: check that the loaded world matches the requested path
      if (bLoaded) {
        UWorld* LoadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (LoadedWorld) {
          FString LoadedPath = LoadedWorld->GetOutermost()->GetName();
          // Normalize paths for comparison (handle case differences)
          if (LoadedPath.ToLower() != ExpectedLoadedPath.ToLower()) {
            // The requested level was not actually loaded - engine fell back to default
            SendAutomationResponse(
                RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Level path mismatch: requested %s but loaded %s"), *ExpectedLoadedPath, *LoadedPath),
                nullptr, TEXT("LOAD_MISMATCH"));
            return true;
          }
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("requestedPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), ExpectedLoadedPath);
        Resp->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
        Resp->SetBoolField(TEXT("savedDirtyPackagesBeforeLoad"), bSavedDirtyPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyWorldPackagesBeforeLoad"), DirtyWorldPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyContentPackagesBeforeLoad"), DirtyContentPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyWorldPackagesAfterSave"), DirtyWorldPackagesAfterSave);
        Resp->SetNumberField(TEXT("dirtyContentPackagesAfterSave"), DirtyContentPackagesAfterSave);
        Resp->SetNumberField(TEXT("failedDirtyPackageSaves"), FailedDirtyPackageSaves);
        VerifyAssetExists(Resp, ExpectedLoadedPath);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level loaded"), Resp, FString());
        return true;
      } else {
        // Fallback to ExecuteConsoleCommand "Open" if LoadMap failed (e.g.
        // maybe it was a raw asset path or something) But actually if LoadMap
        // fails, Open likely fails too.
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Failed to load map: %s"), *LevelPath),
            nullptr, TEXT("LOAD_FAILED"));
        return true;
      }
#else
      return false;
#endif
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
