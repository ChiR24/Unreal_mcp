#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelDirtyPackageLoad.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"
#include "Domains/ControlEditor/McpEditorSupport.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RenderingThread.h"

#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Safety/McpSafeOperationsLevelFile.h"
#include "Safety/McpSafeOperationsMapLoad.h"

using McpSafeOperations::McpSafeLoadMap;

namespace McpLevelHandlers {
bool HandleLoadLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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
      // An object path (/Game/Maps/Demo.Demo) or a .umap name reads as its package; it answered FILE_NOT_FOUND.
      LevelPath = FPackageName::ObjectPathToPackageName(SanitizedLevelPath);

      if (!LevelPath.StartsWith(TEXT("/")) && !FPaths::FileExists(LevelPath)) {
        FString TryPath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelPath);
        if (FPackageName::DoesPackageExist(TryPath)) {
          LevelPath = TryPath;
        }
      }

      // The folder form is loaded when it exists, else the flat one; a package with neither file is loaded by name
      // when the engine still knows it.
      const McpSafeOperations::FMcpLevelFile LevelFile = McpSafeOperations::McpFindLevelFile(LevelPath);
      const FString ResolvedFileToLoad = LevelFile.bFolder ? LevelFile.Folder : LevelFile.Flat.IsEmpty() ? LevelPath : LevelFile.Flat;
      const FString LevelName = FPaths::GetBaseFilename(LevelPath);
      const FString ExpectedLoadedPath = LevelFile.bFolder ? FPaths::GetPath(LevelPath) / LevelName / LevelName : LevelPath;
      if (!LevelFile.bFolder && !LevelFile.bFlat && !FPackageName::DoesPackageExist(LevelPath)) {
        TSharedPtr<FJsonObject> ErrorDetails;
        const FString Missing = McpSafeOperations::McpLevelFileNotFound(LevelPath, LevelFile, ErrorDetails);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Missing, ErrorDetails, TEXT("FILE_NOT_FOUND"));
        return true;
      }

      FlushRenderingCommands();

      // The counts are the truth in every mode. They read 0 in an interactive editor, because only the headless path
      // counted, while get_summary listed the open level under unsavedPackages.
      int32 DirtyWorldPackagesBeforeLoad = 0;
      int32 DirtyContentPackagesBeforeLoad = 0;
      McpSafeOperations::McpCountBlockingDirtyPackages(DirtyWorldPackagesBeforeLoad, DirtyContentPackagesBeforeLoad);

      // The level that is already open is not loaded again (McpSafeLoadMap skips it as "already loaded"). The reply used to
      // say "Level loaded" for a call that did nothing, with the unsaved changes of that very level still pending.
      UWorld* OpenWorld = GEditor->GetEditorWorldContext().World();
      const bool bAlreadyOpen = OpenWorld && OpenWorld->GetOutermost()->GetName().Equals(ExpectedLoadedPath, ESearchCase::IgnoreCase);

      bool bSavedDirtyPackagesBeforeLoad = false;
      int32 DirtyWorldPackagesAfterSave = 0;
      int32 DirtyContentPackagesAfterSave = 0;
      int32 FailedDirtyPackageSaves = 0;
      const bool bHeadless = McpSafeOperations::McpIsHeadlessEditor();
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
        // requestedPath only when the path asked for resolved to another (a short name, an object path).
        if (!LevelPath.Equals(ExpectedLoadedPath, ESearchCase::CaseSensitive)) {
          Resp->SetStringField(TEXT("requestedPath"), LevelPath);
        }
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
            bUnsaved ? TEXT("Level already open: nothing was reloaded, and its unsaved changes were kept (save them with manage_level save, or load another level with discardUnsaved true to drop them)")
                     : TEXT("Level already open: nothing was reloaded"),
            Resp, FString());
        return true;
      }

      bool bDiscardUnsaved = false;
      Payload->TryGetBoolField(TEXT("discardUnsaved"), bDiscardUnsaved);
      TSharedPtr<FJsonObject> LossDetails;
      // The level's /Game path, which the reply names: the file path it loads from is not the caller's to see.
      const FString Loss = bDiscardUnsaved ? FString() : McpSafeOperations::McpRefuseLoadOverUnsavedLevels(
          ExpectedLoadedPath, TEXT("Pass saveDirtyPackages true to save them first, or discardUnsaved true to drop them."), LossDetails);
      if (!Loss.IsEmpty()) {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Loss, LossDetails, TEXT("DIRTY_PACKAGES"));
        return true;
      }

      const FEditorViewportClient* ViewBefore = GetActiveEditorViewportClientForMcp();
      const bool bWasGameView = ViewBefore && ViewBefore->IsInGameView();
      const bool bLoaded = McpSafeLoadMap(ResolvedFileToLoad);

      if (bLoaded) {
        UWorld* LoadedWorld = McpHandlerUtils::GetEditorWorld();
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

        McpReplyWhenLevelSettles(TWeakObjectPtr<UEngineWireSubsystem>(&Subsystem), RequestingSocket, RequestId,
                                 TEXT("Level loaded"), LoadReply(true), bWasGameView);
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
