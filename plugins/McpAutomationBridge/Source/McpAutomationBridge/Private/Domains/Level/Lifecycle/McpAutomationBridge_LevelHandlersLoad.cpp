#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RenderingThread.h"

#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h"
#include "Safety/McpSafeOperationsMapLoad.h"

using McpSafeOperations::McpSafeLoadMap;

namespace McpLevelHandlers {
bool HandleLoadLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
      // streaming=true streams the level into the open world as a sub-level
      // instead of replacing it: exactly what add_sublevel does with levelPath.
      bool bStreaming = false;
      Payload->TryGetBoolField(TEXT("streaming"), bStreaming);
      if (bStreaming) {
        return HandleAddSublevelAction(Subsystem, RequestId, Payload, RequestingSocket);
      }
      FString LevelPath;
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      bool bSaveDirtyPackages = false;
      Payload->TryGetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);

      if (LevelPath.IsEmpty()) {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                            TEXT("levelPath required"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
      }

      // SECURITY: Sanitize LevelPath to prevent path traversal attacks
      FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
      if (SanitizedLevelPath.IsEmpty()) {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
                            McpPathRefusalMessage(TEXT("levelPath"), LevelPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      LevelPath = SanitizedLevelPath;

      if (!LevelPath.StartsWith(TEXT("/")) && !FPaths::FileExists(LevelPath)) {
        FString TryPath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelPath);
        if (FPackageName::DoesPackageExist(TryPath)) {
          LevelPath = TryPath;
        }
      }

      if (!GEditor) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Editor not available"), nullptr,
                               TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
      }

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

      bool bFileExists = false;

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
      } else if (!FullFlatMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFlatMapPath)) {
        bFileExists = true;
        ResolvedFileToLoad = FlatMapPath;
        ExpectedLoadedPath = LevelPath;
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
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"),
                          *FullFolderMapPath, *FullFlatMapPath),
            ErrorDetails, TEXT("FILE_NOT_FOUND"));
        return true;
      }

      FlushRenderingCommands();

      // The counts are the truth in every mode. They read 0 in an interactive editor, because only the headless path
      // counted, while get_summary listed the open level under unsavedPackages.
      int32 DirtyWorldPackagesBeforeLoad = 0;
      int32 DirtyContentPackagesBeforeLoad = 0;
      CountBlockingDirtyPackages(DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad);

      // The level that is already open is not loaded again (McpSafeLoadMap skips it as "already loaded"). The reply used to
      // say "Level loaded" for a call that did nothing, with the unsaved changes of that very level still pending.
      UWorld* OpenWorld = GEditor->GetEditorWorldContext().World();
      const bool bAlreadyOpen = OpenWorld && OpenWorld->GetOutermost()->GetName().Equals(ExpectedLoadedPath, ESearchCase::IgnoreCase);

      bool bSavedDirtyPackagesBeforeLoad = false;
      int32 DirtyWorldPackagesAfterSave = 0;
      int32 DirtyContentPackagesAfterSave = 0;
      int32 FailedDirtyPackageSaves = 0;
      const bool bHeadless = FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
      if (bHeadless && !bAlreadyOpen && DirtyWorldPackagesBeforeLoad + DirtyContentPackagesBeforeLoad > 0 && !bSaveDirtyPackages) {
        TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
        ErrorDetails->SetNumberField(TEXT("dirtyWorldPackages"), DirtyWorldPackagesBeforeLoad);
        ErrorDetails->SetNumberField(TEXT("dirtyContentPackages"), DirtyContentPackagesBeforeLoad);
        ErrorDetails->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
        ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            TEXT("Cannot load a level in unattended/headless mode while packages are dirty. Pass saveDirtyPackages=true to save them before loading."),
            ErrorDetails, TEXT("DIRTY_PACKAGES"));
        return true;
      }

      // saveDirtyPackages is honoured in every mode: an interactive editor ignored it and loaded over the dirty packages.
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
          Subsystem.SendAutomationResponse(
              RequestingSocket, RequestId, false,
              TEXT("Cannot load a level while packages remain dirty after the save saveDirtyPackages asked for."),
              ErrorDetails, TEXT("DIRTY_PACKAGES"));
          return true;
        }
      }

      // What every success reply says about the paths and the dirty packages; reloaded tells a load from a no-op.
      const auto LoadReply = [&](const bool bReloaded) {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("requestedPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), ExpectedLoadedPath);
        Resp->SetBoolField(TEXT("alreadyLoaded"), !bReloaded);
        Resp->SetBoolField(TEXT("reloaded"), bReloaded);
        Resp->SetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
        Resp->SetBoolField(TEXT("savedDirtyPackagesBeforeLoad"), bSavedDirtyPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyWorldPackagesBeforeLoad"), DirtyWorldPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyContentPackagesBeforeLoad"), DirtyContentPackagesBeforeLoad);
        Resp->SetNumberField(TEXT("dirtyWorldPackagesAfterSave"), DirtyWorldPackagesAfterSave);
        Resp->SetNumberField(TEXT("dirtyContentPackagesAfterSave"), DirtyContentPackagesAfterSave);
        Resp->SetNumberField(TEXT("failedDirtyPackageSaves"), FailedDirtyPackageSaves);
        VerifyAssetExists(Resp, ExpectedLoadedPath);
        return Resp;
      };

      if (bAlreadyOpen) {
        // Nothing is reloaded and a running PIE session is left alone. The unsaved state is the open level's own, read after
        // any save the caller asked for, so a caller that wants the changes gone loads another level first.
        TSharedPtr<FJsonObject> Resp = LoadReply(false);
        AddUnsavedState(Resp, OpenWorld->PersistentLevel);
        bool bUnsaved = false;
        Resp->TryGetBoolField(TEXT("unsaved"), bUnsaved);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
            bUnsaved ? TEXT("Level already open: nothing was reloaded, and its unsaved changes were kept (save them with manage_level save, or load another level first to drop them)")
                     : TEXT("Level already open: nothing was reloaded"),
            Resp, FString());
        return true;
      }

      const bool bLoaded = McpSafeLoadMap(ResolvedFileToLoad);

      if (bLoaded) {
        UWorld* LoadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (LoadedWorld) {
          FString LoadedPath = LoadedWorld->GetOutermost()->GetName();
          if (!LoadedPath.Equals(ExpectedLoadedPath, ESearchCase::IgnoreCase)) {
            // The requested level was not actually loaded - engine fell back to default
            Subsystem.SendAutomationResponse(
                RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Level path mismatch: requested %s but loaded %s"), *ExpectedLoadedPath, *LoadedPath),
                nullptr, TEXT("LOAD_MISMATCH"));
            return true;
          }
        }

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level loaded"), LoadReply(true), FString());
        return true;
      } else {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Failed to load map: %s"), *LevelPath),
            nullptr, TEXT("LOAD_FAILED"));
        return true;
      }
}
} // namespace McpLevelHandlers
