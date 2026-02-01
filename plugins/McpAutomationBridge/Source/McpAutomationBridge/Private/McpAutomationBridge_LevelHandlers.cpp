#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "LevelEditor.h"
#include "RenderingThread.h"
#include "RenderCommandFence.h"
#include "Exporters/Exporter.h"

// CRITICAL FIX: Add headers for thread checking and streaming
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/FileManager.h"      // For file operations
#include "HAL/PlatformFileManager.h"  // For FPlatformFileManager
#include "RenderAssetUpdate.h"
#include "ContentStreaming.h"  // For IStreamingManager in UE 5.7

// Check for LevelEditorSubsystem
#if defined(__has_include)
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 0
#endif

// Helper to fully synchronize GPU before World Partition saves.
// Intel Gen12 drivers crash when SaveMap triggers recursive FlushRenderingCommands
// while async thumbnail generation is in flight. This ensures GPU is truly idle.
static void McpSyncGPUForWorldPartitionSave()
{
	// CRITICAL FIX: Check if we're already in a rendering flush or on the render thread
	// to prevent TaskGraph recursion guard failures
	if (IsInRenderingThread())
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("McpSyncGPUForWorldPartitionSave: Called from render thread, skipping"));
		return;
	}

	// Check if async loading is suspended - if so, skip the flush to avoid ensure failures
	// This happens during World Partition saves when the engine is already managing rendering
	if (IsAsyncLoadingSuspended())
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("McpSyncGPUForWorldPartitionSave: Async loading is suspended, skipping GPU sync to avoid recursive flush"));
		return;
	}

	// CRITICAL FIX: Check if asset streaming is suspended to prevent ensure failures
	// in StreamingManagerTexture.cpp line 2099
	if (IsAssetStreamingSuspended())
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("McpSyncGPUForWorldPartitionSave: Asset streaming is suspended, skipping GPU sync"));
		return;
	}
	
	// Issue a fence on the render thread
	FRenderCommandFence Fence;
	Fence.BeginFence();
	
	// Wait for all current rendering commands to complete
	// Intel Gen12 drivers crash when SaveMap triggers recursive FlushRenderingCommands.
	// We use a single flush and a fence to ensure GPU work is complete safely.
	// CRITICAL FIX: Only flush if we're on the game thread and not already flushing
	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}
	
	// Wait for the fence to signal (ensures GPU work is complete)
	Fence.Wait();
}

/**
 * Cleans up World Partition external actor folders before saving.
 * This prevents "Unable to delete existing actor packages" errors.
 * 
 * @param SavePath The target package path (e.g., /Game/Maps/MyLevel)
 * @return true if cleanup succeeded or wasn't needed, false if cleanup failed
 */
static bool McpCleanupWorldPartitionExternalActors(const FString& SavePath)
{
    FString TargetFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            SavePath, TargetFilename, FPackageName::GetMapPackageExtension())) {
        return true; // Not a valid package path, nothing to clean
    }

    const FString BaseDir = FPaths::GetPath(TargetFilename);
    const FString BaseName = FPaths::GetBaseFilename(TargetFilename);
    bool bAllSucceeded = true;
    
    // Get the platform file interface
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // Clean up __ExternalActors__
    FString ExternalActorsPath = BaseDir / TEXT("__ExternalActors__") / BaseName;
    if (PlatformFile.DirectoryExists(*ExternalActorsPath)) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
               TEXT("McpCleanupWorldPartitionExternalActors: Removing %s"), 
               *ExternalActorsPath);
        if (!PlatformFile.DeleteDirectoryRecursively(*ExternalActorsPath)) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
                   TEXT("McpCleanupWorldPartitionExternalActors: Failed to delete %s"), 
                   *ExternalActorsPath);
            bAllSucceeded = false;
        }
    }

    // Clean up __ExternalObjects__ (the problematic folder for saves)
    FString ExternalObjectsPath = BaseDir / TEXT("__ExternalObjects__") / BaseName;
    if (PlatformFile.DirectoryExists(*ExternalObjectsPath)) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
               TEXT("McpCleanupWorldPartitionExternalActors: Removing %s"), 
               *ExternalObjectsPath);
        if (!PlatformFile.DeleteDirectoryRecursively(*ExternalObjectsPath)) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
                   TEXT("McpCleanupWorldPartitionExternalActors: Failed to delete %s"), 
                   *ExternalObjectsPath);
            bAllSucceeded = false;
        }
    }

    return bAllSucceeded;
}
#endif

// Cycle stats for Level handlers.
// Use `stat McpBridge` in the UE console to view these stats.
DECLARE_CYCLE_STAT(TEXT("Level:Action"), STAT_MCP_LevelAction, STATGROUP_McpBridge);

bool UMcpAutomationBridgeSubsystem::HandleLevelAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  SCOPE_CYCLE_COUNTER(STAT_MCP_LevelAction);
  
  const FString Lower = Action.ToLower();
  const bool bIsLevelAction =
      (Lower == TEXT("manage_level") || Lower == TEXT("save_current_level") ||
        Lower == TEXT("create_new_level") || Lower == TEXT("stream_level") ||
        Lower == TEXT("spawn_light") || Lower == TEXT("build_lighting") ||
        Lower == TEXT("spawn_light") || Lower == TEXT("build_lighting") ||
        Lower == TEXT("bake_lightmap") || Lower == TEXT("list_levels") ||
        Lower == TEXT("export_level") || Lower == TEXT("import_level") ||
        Lower == TEXT("add_sublevel") || Lower == TEXT("create_sublevel") ||
       Lower == TEXT("configure_world_partition") ||
       Lower == TEXT("create_streaming_volume") ||
       Lower == TEXT("configure_large_world_coordinates") ||
       Lower == TEXT("create_world_partition_cell") ||
       Lower == TEXT("configure_runtime_loading") ||
       Lower == TEXT("configure_world_settings") ||
       Lower == TEXT("get_world_partition_cells") ||
       Lower == TEXT("configure_hlod_settings") ||
       Lower == TEXT("build_hlod_for_level"));
  if (!bIsLevelAction)
    return false;

  FString EffectiveAction = Lower;

  // Unpack manage_level
  if (Lower == TEXT("manage_level")) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("manage_level payload missing"),
                          TEXT("INVALID_PAYLOAD"));
      return true;
    }
    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    if (LowerSub == TEXT("load") || LowerSub == TEXT("load_level")) {
      // Map to Open command
      FString LevelPath;
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

      // Determine invalid characters for checks
      if (LevelPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("levelPath required"),
                            TEXT("INVALID_ARGUMENT"));
      }

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

      // CRITICAL FIX: Only flush rendering commands if we're on the game thread
      // and not already in a flush to prevent TaskGraph recursion guard failures
      if (IsInGameThread() && !IsAsyncLoadingSuspended() && !IsAssetStreamingSuspended())
      {
        FlushRenderingCommands();
      }

      // LoadMap prompts for save if dirty. To avoid blocking automation, we
      // should carefuly consider. But for now, we assume user wants standard
      // behavior or has saved. There isn't a simple "Force Load" via FileUtils
      // without clearing dirty flags manually. We will proceed with LoadMap.
      const bool bLoaded = FEditorFileUtils::LoadMap(FileToLoad);

      if (bLoaded) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("levelPath"), LevelPath);
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
    } else if (LowerSub == TEXT("save")) {
      EffectiveAction = TEXT("save_current_level");
    } else if (LowerSub == TEXT("save_as") ||
               LowerSub == TEXT("save_level_as")) {
      EffectiveAction = TEXT("save_level_as");
    } else if (LowerSub == TEXT("create_level")) {
      EffectiveAction = TEXT("create_new_level");
    } else if (LowerSub == TEXT("stream")) {
      EffectiveAction = TEXT("stream_level");
    } else if (LowerSub == TEXT("create_light")) {
      EffectiveAction = TEXT("spawn_light");
    } else if (LowerSub == TEXT("list") || LowerSub == TEXT("list_levels")) {
      EffectiveAction = TEXT("list_levels");
    } else if (LowerSub == TEXT("export_level")) {
      EffectiveAction = TEXT("export_level");
    } else if (LowerSub == TEXT("import_level")) {
      EffectiveAction = TEXT("import_level");
    } else if (LowerSub == TEXT("add_sublevel")) {
      EffectiveAction = TEXT("add_sublevel");
    } else if (LowerSub == TEXT("configure_world_partition")) {
      EffectiveAction = TEXT("configure_world_partition");
    } else if (LowerSub == TEXT("create_streaming_volume")) {
      EffectiveAction = TEXT("create_streaming_volume");
    } else if (LowerSub == TEXT("configure_large_world_coordinates")) {
      EffectiveAction = TEXT("configure_large_world_coordinates");
    } else if (LowerSub == TEXT("create_world_partition_cell")) {
      EffectiveAction = TEXT("create_world_partition_cell");
    } else if (LowerSub == TEXT("configure_runtime_loading")) {
      EffectiveAction = TEXT("configure_runtime_loading");
    } else if (LowerSub == TEXT("configure_world_settings")) {
      EffectiveAction = TEXT("configure_world_settings");
    } else if (LowerSub == TEXT("get_world_partition_cells")) {
      EffectiveAction = TEXT("get_world_partition_cells");
    } else if (LowerSub == TEXT("configure_hlod_settings")) {
      EffectiveAction = TEXT("configure_hlod_settings");
    } else if (LowerSub == TEXT("build_hlod_for_level")) {
      EffectiveAction = TEXT("build_hlod_for_level");
    } else if (LowerSub == TEXT("delete")) {
      // Handle level deletion
      const TArray<TSharedPtr<FJsonValue>>* LevelPathsArray = nullptr;
      FString SingleLevelPath;
      TArray<FString> LevelPaths;
      
      if (Payload->TryGetArrayField(TEXT("levelPaths"), LevelPathsArray)) {
        LevelPaths.Reserve(LevelPathsArray->Num());
        for (const auto& Val : *LevelPathsArray) {
          FString PathStr;
          if (Val->TryGetString(PathStr) && !PathStr.IsEmpty()) {
            LevelPaths.Add(PathStr);
          }
        }
      } else if (Payload->TryGetStringField(TEXT("levelPath"), SingleLevelPath) && !SingleLevelPath.IsEmpty()) {
        LevelPaths.Add(SingleLevelPath);
      }
      
      if (LevelPaths.Num() == 0) {
        SendAutomationError(RequestingSocket, RequestId,
                           TEXT("levelPath or levelPaths required for delete"),
                           TEXT("INVALID_ARGUMENT"));
        return true;
      }
      
#if WITH_EDITOR
      TArray<FString> DeletedLevels;
      TArray<FString> FailedLevels;
      DeletedLevels.Reserve(LevelPaths.Num());
      FailedLevels.Reserve(LevelPaths.Num());
      
      for (const FString& LevelPath : LevelPaths) {
        // Normalize path to package name
        FString PackagePath = LevelPath;
        if (!PackagePath.StartsWith(TEXT("/"))) {
          PackagePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelPath);
        }
        
        // Check if package exists
        if (!FPackageName::DoesPackageExist(PackagePath)) {
          FailedLevels.Add(FString::Printf(TEXT("%s (not found)"), *LevelPath));
          continue;
        }
        
        // Use UEditorAssetLibrary to delete the level asset
        bool bDeleted = UEditorAssetLibrary::DeleteAsset(PackagePath);
        if (bDeleted) {
          DeletedLevels.Add(LevelPath);
        } else {
          FailedLevels.Add(FString::Printf(TEXT("%s (delete failed)"), *LevelPath));
        }
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      
      TArray<TSharedPtr<FJsonValue>> DeletedArr;
      for (const FString& D : DeletedLevels) {
        DeletedArr.Add(MakeShared<FJsonValueString>(D));
      }
      Resp->SetArrayField(TEXT("deleted"), DeletedArr);
      
      if (FailedLevels.Num() > 0) {
        TArray<TSharedPtr<FJsonValue>> FailedArr;
        for (const FString& F : FailedLevels) {
          FailedArr.Add(MakeShared<FJsonValueString>(F));
        }
        Resp->SetArrayField(TEXT("failed"), FailedArr);
      }
      
      Resp->SetNumberField(TEXT("deletedCount"), DeletedLevels.Num());
      
      if (DeletedLevels.Num() > 0) {
        SendAutomationResponse(RequestingSocket, RequestId, true,
                              FString::Printf(TEXT("Deleted %d level(s)"), DeletedLevels.Num()),
                              Resp, FString());
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                              TEXT("No levels deleted"), Resp, TEXT("DELETE_FAILED"));
      }
      return true;
#else
      SendAutomationError(RequestingSocket, RequestId,
                         TEXT("Level deletion requires editor"),
                         TEXT("EDITOR_REQUIRED"));
      return true;
#endif
    } else {
      // Try to forward to level structure handlers (configure_world_settings, etc.)
      // Create a modified payload with subAction field for the structure handler
      TSharedPtr<FJsonObject> StructurePayload = MakeShared<FJsonObject>();
      if (Payload.IsValid()) {
        for (const auto& Field : Payload->Values) {
          StructurePayload->Values.Add(Field.Key, Field.Value);
        }
      }
      StructurePayload->SetStringField(TEXT("subAction"), LowerSub);
      
      if (HandleManageLevelStructureAction(RequestId, Action, StructurePayload, RequestingSocket)) {
        return true;
      }
      
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown manage_level action: %s"), *SubAction),
          TEXT("UNKNOWN_ACTION"));
      return true;
    }
  }

#if WITH_EDITOR
  if (EffectiveAction == TEXT("save_current_level")) {
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *World = GetActiveWorld();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Check if this is an unsaved/temporary level first
    FString PackageName = World->GetOutermost()->GetName();
    if (PackageName.Contains(TEXT("Untitled")) || PackageName.StartsWith(TEXT("/Temp/"))) {
      FString SavePath;
      if (Payload.IsValid() && Payload->TryGetStringField(TEXT("savePath"), SavePath) && !SavePath.IsEmpty()) {
        // Use save_level_as logic instead of failing
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Level is Untitled/Temp, but savePath provided. Redirecting to save_level_as."));
        
        McpSyncGPUForWorldPartitionSave();
        
        // CRITICAL FIX: Suppress modal dialogs and clean up external actors
        FModalDialogSuppressor DialogSuppressor;
        McpCleanupWorldPartitionExternalActors(SavePath);
        
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
        if (ULevelEditorSubsystem *LevelEditorSS = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
          bool bSaved = false;
#if __has_include("FileHelpers.h")
          // Force garbage collection to release file handles before save
          GEditor->ForceGarbageCollection(true);
          FPlatformProcess::Sleep(0.1f);
          
          bSaved = FEditorFileUtils::SaveMap(World, SavePath);
#endif
          McpSyncGPUForWorldPartitionSave();
          if (bSaved) {
            TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
            Resp->SetStringField(TEXT("levelPath"), SavePath);
            SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Untitled level saved as %s"), *SavePath), Resp);
            return true;
          }
        }
#endif
      }
      
      TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);
      ErrorDetail->SetStringField(TEXT("reason"), TEXT("Level is unsaved/temporary. Use save_level_as with a path first."));
      ErrorDetail->SetStringField(TEXT("hint"), TEXT("Use manage_level with action='save_as' and provide savePath"));
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Cannot save unsaved level - use save_as first or provide savePath"), ErrorDetail, TEXT("SAVE_FAILED"));
      return true;
    }


    // Use robust GPU sync for World Partition saves (Intel GPU crash fix)
    McpSyncGPUForWorldPartitionSave();

    // Use McpSafeAssetSave which handles dialogs silently
    bool bSaved = McpSafeAssetSave(World);
    
    // POST-SAVE GPU sync: Save triggers async thumbnail generation which
    // causes recursive FlushRenderingCommands on Intel Gen12 drivers.
    // Flush GPU again AFTER save to catch any pending thumbnail work.
    McpSyncGPUForWorldPartitionSave();
    
    // UE 5.7 + Intel GPU Workaround: World Partition saves trigger massive
    // recursive FlushRenderingCommands which can exhaust GPU resources.
    // Defer the response by ~200ms to let rendering thread stabilize,
    // preventing SlateRHIRenderer crashes during thumbnail generation.
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("levelPath"), PackageName);
    Resp->SetBoolField(TEXT("success"), bSaved);
    
    // Capture for deferred response
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf = this;
    const FString CapturedRequestId = RequestId;
    const bool bCapturedSaved = bSaved;
    const FString CapturedPackageName = PackageName;
    TSharedPtr<FMcpBridgeWebSocket> CapturedSocket = RequestingSocket;
    TSharedPtr<FJsonObject> CapturedResp = Resp;
    TSharedPtr<FJsonObject> CapturedErrorDetail = nullptr;
    FString CapturedErrorReason;
    
    if (!bSaved) {
      // Prepare error details for deferred response
      CapturedErrorDetail = MakeShared<FJsonObject>();
      CapturedErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);
      
      FString Filename;
      CapturedErrorReason = TEXT("Unknown save failure");
      
      if (FPackageName::TryConvertLongPackageNameToFilename(
                     PackageName, Filename,
                     FPackageName::GetMapPackageExtension())) {
        if (IFileManager::Get().IsReadOnly(*Filename)) {
          CapturedErrorReason = TEXT("File is read-only or locked by another process");
          CapturedErrorDetail->SetStringField(TEXT("filename"), Filename);
        } else if (!IFileManager::Get().DirectoryExists(
                       *FPaths::GetPath(Filename))) {
          CapturedErrorReason = TEXT("Target directory does not exist");
          CapturedErrorDetail->SetStringField(TEXT("directory"),
                                      FPaths::GetPath(Filename));
        } else {
          CapturedErrorReason =
              TEXT("Save operation failed - check Output Log for details");
          CapturedErrorDetail->SetStringField(TEXT("filename"), Filename);
        }
      }
      CapturedErrorDetail->SetStringField(TEXT("reason"), CapturedErrorReason);
    }
    
    // Defer response to allow rendering commands to settle after save
    if (GEditor)
    {
      FTimerDelegate ResponseDelegate;
      ResponseDelegate.BindLambda([WeakSelf, CapturedSocket, CapturedRequestId, bCapturedSaved, CapturedPackageName, CapturedResp, CapturedErrorDetail, CapturedErrorReason]()
      {
        if (UMcpAutomationBridgeSubsystem* Self = WeakSelf.Get())
        {
          if (bCapturedSaved) {
            Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, true,
                               TEXT("Level saved"), CapturedResp, FString());
          } else {
            Self->SendAutomationResponse(
                CapturedSocket, CapturedRequestId, false,
                FString::Printf(TEXT("Failed to save level: %s"), *CapturedErrorReason),
                CapturedErrorDetail, TEXT("SAVE_FAILED"));
          }
        }
      });
      
      FTimerHandle TempHandle;
      // Use 200ms delay to let GPU/Slate stabilize after World Partition save
      GEditor->GetTimerManager()->SetTimer(TempHandle, ResponseDelegate, 0.2f, false);
    }
    else
    {
      // Fallback: send immediately if timer not available
      if (bSaved) {
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level saved"), Resp, FString());
      } else {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Failed to save level: %s"), *CapturedErrorReason),
            CapturedErrorDetail, TEXT("SAVE_FAILED"));
      }
    }
    return true;
  }
  if (EffectiveAction == TEXT("save_level_as")) {
    // Use robust GPU sync for World Partition saves (Intel GPU crash fix)
    McpSyncGPUForWorldPartitionSave();

    FString SavePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("savePath required for save_level_as"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LevelEditorSS =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
      bool bSaved = false;
      FString ErrorMessage;
      
#if __has_include("FileHelpers.h")
      if (UWorld *World = GetActiveWorld()) {
        // CRITICAL FIX: Suppress modal dialogs during save to prevent automation breakage
        FModalDialogSuppressor DialogSuppressor;
        
        // UE 5.7: Streaming management is automatic. Ensure all pending streaming
        // requests are processed before saving to avoid conflicts.
        // CRITICAL: Only call if asset streaming is not suspended to avoid ensure() failures
        if (!IsAssetStreamingSuspended()) {
          IStreamingManager::Get().BlockTillAllRequestsFinished(5.0f, false);
        } else {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, 
                 TEXT("save_level_as: Asset streaming is suspended, skipping BlockTillAllRequestsFinished"));
        }

        // CRITICAL FIX: Check if target exists and clean up World Partition external actors
        // to prevent "Unable to delete existing actor packages" modal dialog
        FString TargetFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(
                SavePath, TargetFilename, FPackageName::GetMapPackageExtension())) {
          
          // Check if file exists
          if (IFileManager::Get().FileExists(*TargetFilename)) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
                   TEXT("save_level_as: Target exists, cleaning up World Partition external actors: %s"), 
                   *SavePath);
            
            // Clean up external actor folders before SaveMap tries to do it
            McpCleanupWorldPartitionExternalActors(SavePath);
            
            // Force garbage collection to release any file handles
            GEditor->ForceGarbageCollection(true);
            
            // Small delay to let file system settle
            FPlatformProcess::Sleep(0.1f);
          }
        }

        // CRITICAL: Check if asset streaming is suspended before SaveMap
        // SaveMap internally calls BlockTillAllRequestsFinished which causes ensure() failure
        // when streaming is already suspended (e.g., during another save operation)
        // Wait up to 5 seconds for streaming to resume before attempting save
        if (IsAssetStreamingSuspended()) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
                 TEXT("save_level_as: Asset streaming is suspended, waiting for it to resume..."));
          
          float WaitTime = 0.0f;
          const float MaxWaitTime = 5.0f;
          const float SleepInterval = 0.1f;
          
          while (IsAssetStreamingSuspended() && WaitTime < MaxWaitTime) {
            FPlatformProcess::Sleep(SleepInterval);
            WaitTime += SleepInterval;
          }
          
          if (IsAssetStreamingSuspended()) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, 
                   TEXT("save_level_as: Asset streaming still suspended after %fs, cannot save"), MaxWaitTime);
            ErrorMessage = TEXT("Asset streaming is suspended - cannot save while another streaming operation is in progress");
            bSaved = false;
          } else {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
                   TEXT("save_level_as: Asset streaming resumed after %fs, proceeding with save"), WaitTime);
          }
        }
        
        // Only attempt save if we haven't already marked it as failed
        if (ErrorMessage.IsEmpty()) {
          // Attempt the save
          bSaved = FEditorFileUtils::SaveMap(World, SavePath);
          
          if (!bSaved) {
            ErrorMessage = TEXT("SaveMap returned false - check Output Log for details");
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
                   TEXT("save_level_as: SaveMap failed for %s"), *SavePath);
          }
        }
      } else {
        ErrorMessage = TEXT("No active world to save");
      }
#endif
      // POST-SAVE GPU sync: SaveMap triggers async thumbnail generation which
      // causes recursive FlushRenderingCommands on Intel Gen12 drivers.
      // Flush GPU again AFTER save to catch any pending thumbnail work.
      McpSyncGPUForWorldPartitionSave();
      
      // UE 5.7 + Intel GPU Workaround: World Partition saves trigger massive
      // recursive FlushRenderingCommands which can exhaust GPU resources.
      // Defer the response by ~200ms to let rendering thread stabilize,
      // preventing D3D12 swapchain creation failures (E_ACCESSDENIED 0x80070005).
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("levelPath"), SavePath);
      Resp->SetBoolField(TEXT("success"), bSaved);
      if (!ErrorMessage.IsEmpty()) {
        Resp->SetStringField(TEXT("error"), ErrorMessage);
      }
      
      // Capture for deferred response
      TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf = this;
      const FString CapturedRequestId = RequestId;
      const bool bCapturedSaved = bSaved;
      const FString CapturedSavePath = SavePath;
      const FString CapturedError = ErrorMessage;
      TSharedPtr<FMcpBridgeWebSocket> CapturedSocket = RequestingSocket;
      TSharedPtr<FJsonObject> CapturedResp = Resp;
      
      // Defer response to allow rendering commands to settle after WP save
      if (GEditor)
      {
        FTimerDelegate ResponseDelegate;
        ResponseDelegate.BindLambda([WeakSelf, CapturedSocket, CapturedRequestId, 
                                     bCapturedSaved, CapturedSavePath, CapturedResp, CapturedError]()
        {
          if (UMcpAutomationBridgeSubsystem* Self = WeakSelf.Get())
          {
            if (bCapturedSaved) {
              Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, true,
                                 FString::Printf(TEXT("Level saved as %s"), *CapturedSavePath),
                                 CapturedResp, FString());
            } else {
              TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
              ErrorDetail->SetStringField(TEXT("attemptedPath"), CapturedSavePath);
              ErrorDetail->SetStringField(TEXT("reason"), CapturedError.IsEmpty() ? 
                                          TEXT("Save operation failed") : CapturedError);
              ErrorDetail->SetStringField(TEXT("hint"), 
                                          TEXT("For World Partition levels, ensure external actor folders are writable"));
              Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, false,
                                 FString::Printf(TEXT("Failed to save level as: %s"), 
                                 CapturedError.IsEmpty() ? TEXT("Unknown error") : *CapturedError),
                                 ErrorDetail, TEXT("SAVE_FAILED"));
            }
          }
        });
        
        FTimerHandle TempHandle;
        // Use 200ms for World Partition (more than 100ms for regular saves)
        GEditor->GetTimerManager()->SetTimer(TempHandle, ResponseDelegate, 0.2f, false);
      }
      else
      {
        // Fallback: send immediately if timer not available
        if (bSaved) {
          SendAutomationResponse(
              RequestingSocket, RequestId, true,
              FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp,
              FString());
        } else {
          TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
          ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
          ErrorDetail->SetStringField(TEXT("reason"), ErrorMessage.IsEmpty() ? 
                                      TEXT("Save operation failed") : *ErrorMessage);
          ErrorDetail->SetStringField(TEXT("hint"), 
                                      TEXT("For World Partition levels, ensure external actor folders are writable"));
          SendAutomationResponse(RequestingSocket, RequestId, false,
                                 FString::Printf(TEXT("Failed to save level as: %s"), 
                                 ErrorMessage.IsEmpty() ? TEXT("Unknown error") : *ErrorMessage),
                                 ErrorDetail, TEXT("SAVE_FAILED"));
        }
      }
      return true;
    }
#endif
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("LevelEditorSubsystem not available"), nullptr,
                           TEXT("SUBSYSTEM_MISSING"));
    return true;
  }
  if (EffectiveAction == TEXT("build_lighting") ||
      EffectiveAction == TEXT("bake_lightmap")) {
    TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
    if (Payload.IsValid()) {
      FString Q;
      if (Payload->TryGetStringField(TEXT("quality"), Q) && !Q.IsEmpty())
        P->SetStringField(TEXT("quality"), Q);
    }
    return HandleExecuteEditorFunction(
        RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
  }
  if (EffectiveAction == TEXT("create_new_level")) {
    FString LevelName;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelName"), LevelName);

    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

    // Construct valid package path
    FString SavePath = LevelPath;
    if (SavePath.IsEmpty() && !LevelName.IsEmpty()) {
      if (LevelName.StartsWith(TEXT("/")))
        SavePath = LevelName;
      else
        SavePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
    }

    if (SavePath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("levelName or levelPath required for create_level"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Check if map already exists
    if (FPackageName::DoesPackageExist(SavePath)) {
      // If exists, just open it
      const FString Cmd = FString::Printf(TEXT("Open %s"), *SavePath);
      TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
      P->SetStringField(TEXT("command"), Cmd);
      return HandleExecuteEditorFunction(
          RequestId, TEXT("execute_console_command"), P, RequestingSocket);
    }

    // Create new map
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM) && __has_include("FileHelpers.h")
    if (GEditor->IsPlaySessionInProgress()) {
      GEditor->RequestEndPlayMap();
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Cannot create level while Play In Editor is active."), nullptr,
          TEXT("PIE_ACTIVE"));
      return true;
    }

    // Force cleanup of previous world/resources to prevent RenderCore/Driver
    // crashes (monza/D3D12) especially when tests run back-to-back triggering
    // thumbnail generation or world partition shutdown.
    // CRITICAL FIX: Only flush if we're on the game thread and not already flushing
    if (GEditor && IsInGameThread() && !IsAsyncLoadingSuspended() && !IsAssetStreamingSuspended()) {
      FlushRenderingCommands();
      GEditor->ForceGarbageCollection(true);
      FlushRenderingCommands();
    } else if (GEditor) {
      // Fallback: Just do GC without rendering flush if we're in a restricted state
      GEditor->ForceGarbageCollection(true);
    }

    // ADDITIONAL FIX: Ensure current world is properly cleaned up before creating new map
    // This prevents component attachment inconsistencies during world transition
    if (GEditor) {
      UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
      if (CurrentWorld) {
        // Clean up any pending component attachments
        CurrentWorld->CleanupActors();
        CurrentWorld->UpdateWorldComponents(false, false);
      }
    }

    if (UWorld *NewWorld =
            GEditor->NewMap(true)) // true = force new map (creates untitled)
    {
      GEditor->GetEditorWorldContext().SetCurrentWorld(NewWorld);

      // Save it to valid path
      // ISSUE #1 FIX: Ensure directory exists
      FString Filename;
      if (FPackageName::TryConvertLongPackageNameToFilename(
              SavePath, Filename, FPackageName::GetMapPackageExtension())) {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
      }

      if (FEditorFileUtils::SaveMap(NewWorld, SavePath)) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("levelPath"), SavePath);
        Resp->SetStringField(TEXT("packagePath"), SavePath);
        Resp->SetStringField(TEXT("objectPath"),
                             SavePath + TEXT(".") +
                                 FPaths::GetBaseFilename(SavePath));
        SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Level created: %s"), *SavePath), Resp,
            FString());
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Failed to save new level"), nullptr,
                               TEXT("SAVE_FAILED"));
      }
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create new map"), nullptr,
                             TEXT("CREATION_FAILED"));
    }
    return true;
#else
    // Fallback for missing headers (shouldn't happen given build.cs)
    const FString Cmd = FString::Printf(TEXT("Open %s"), *SavePath);
    TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("command"), Cmd);
    return HandleExecuteEditorFunction(
        RequestId, TEXT("execute_console_command"), P, RequestingSocket);
#endif
  }
  if (EffectiveAction == TEXT("stream_level")) {
    FString LevelName;
    bool bLoad = true;
    bool bVis = true;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelName"), LevelName);
      Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad);
      Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVis);
      if (LevelName.IsEmpty())
        Payload->TryGetStringField(TEXT("levelPath"), LevelName);
    }
    if (LevelName.TrimStartAndEnd().IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("stream_level requires levelName or levelPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    const FString Cmd =
        FString::Printf(TEXT("StreamLevel %s %s %s"), *LevelName,
                        bLoad ? TEXT("Load") : TEXT("Unload"),
                        bVis ? TEXT("Show") : TEXT("Hide"));
    TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("command"), Cmd);
    return HandleExecuteEditorFunction(
        RequestId, TEXT("execute_console_command"), P, RequestingSocket);
  }
  if (EffectiveAction == TEXT("spawn_light")) {
    FString LightType = TEXT("Point");
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("lightType"), LightType);
    const FString LT = LightType.ToLower();
    FString ClassName;
    if (LT == TEXT("directional"))
      ClassName = TEXT("DirectionalLight");
    else if (LT == TEXT("spot"))
      ClassName = TEXT("SpotLight");
    else if (LT == TEXT("rect"))
      ClassName = TEXT("RectLight");
    else
      ClassName = TEXT("PointLight");
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    if (Payload.IsValid()) {
      const TSharedPtr<FJsonObject> *L = nullptr;
      if (Payload->TryGetObjectField(TEXT("location"), L) && L &&
          (*L).IsValid())
        Params->SetObjectField(TEXT("location"), *L);
      const TSharedPtr<FJsonObject> *R = nullptr;
      if (Payload->TryGetObjectField(TEXT("rotation"), R) && R &&
          (*R).IsValid())
        Params->SetObjectField(TEXT("rotation"), *R);
    }
    TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("functionName"), TEXT("SPAWN_ACTOR_AT_LOCATION"));
    P->SetStringField(TEXT("class_path"), ClassName);
    P->SetObjectField(TEXT("params"), Params.ToSharedRef());
    return HandleExecuteEditorFunction(
        RequestId, TEXT("execute_editor_function"), P, RequestingSocket);
  }
  if (EffectiveAction == TEXT("list_levels")) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> LevelsArray;

    UWorld *World =
        GetActiveWorld();

    // Add current persistent level
    if (World) {
      TSharedPtr<FJsonObject> CurrentLevel = MakeShared<FJsonObject>();
      CurrentLevel->SetStringField(TEXT("name"), World->GetMapName());
      CurrentLevel->SetStringField(TEXT("path"),
                                   World->GetOutermost()->GetName());
      CurrentLevel->SetBoolField(TEXT("isPersistent"), true);
      CurrentLevel->SetBoolField(TEXT("isLoaded"), true);
      CurrentLevel->SetBoolField(TEXT("isVisible"), true);
      LevelsArray.Add(MakeShared<FJsonValueObject>(CurrentLevel));

      // Add streaming levels
      for (const ULevelStreaming *StreamingLevel :
           World->GetStreamingLevels()) {
        if (!StreamingLevel)
          continue;

        TSharedPtr<FJsonObject> LevelEntry = MakeShared<FJsonObject>();
        LevelEntry->SetStringField(TEXT("name"),
                                   StreamingLevel->GetWorldAssetPackageName());
        LevelEntry->SetStringField(
            TEXT("path"),
            StreamingLevel->GetWorldAssetPackageFName().ToString());
        LevelEntry->SetBoolField(TEXT("isPersistent"), false);
        LevelEntry->SetBoolField(TEXT("isLoaded"),
                                 StreamingLevel->IsLevelLoaded());
        LevelEntry->SetBoolField(TEXT("isVisible"),
                                 StreamingLevel->IsLevelVisible());
        LevelEntry->SetStringField(
            TEXT("streamingState"),
            StreamingLevel->IsStreamingStatePending() ? TEXT("Pending")
            : StreamingLevel->IsLevelLoaded()         ? TEXT("Loaded")
                                                      : TEXT("Unloaded"));
        LevelsArray.Add(MakeShared<FJsonValueObject>(LevelEntry));
      }
    }

    // Also query Asset Registry for all map assets
    IAssetRegistry &AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")
            .Get();
    TArray<FAssetData> MapAssets;
    AssetRegistry.GetAssetsByClass(
        FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), MapAssets,
        false);

    TArray<TSharedPtr<FJsonValue>> AllMapsArray;
    for (const FAssetData &MapAsset : MapAssets) {
      TSharedPtr<FJsonObject> MapEntry = MakeShared<FJsonObject>();
      MapEntry->SetStringField(TEXT("name"), MapAsset.AssetName.ToString());
      MapEntry->SetStringField(TEXT("path"), MapAsset.PackageName.ToString());
      MapEntry->SetStringField(TEXT("objectPath"),
                               MapAsset.GetObjectPathString());
      AllMapsArray.Add(MakeShared<FJsonValueObject>(MapEntry));
    }

    Resp->SetArrayField(TEXT("currentWorldLevels"), LevelsArray);
    Resp->SetNumberField(TEXT("currentWorldLevelCount"), LevelsArray.Num());
    Resp->SetArrayField(TEXT("allMaps"), AllMapsArray);
    Resp->SetNumberField(TEXT("allMapsCount"), AllMapsArray.Num());

    if (World) {
      Resp->SetStringField(TEXT("currentMap"), World->GetMapName());
      Resp->SetStringField(TEXT("currentMapPath"),
                           World->GetOutermost()->GetName());
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Levels listed"), Resp, FString());
    return true;
  }
  if (EffectiveAction == TEXT("export_level")) {
    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    FString ExportPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("exportPath"), ExportPath);
    if (ExportPath.IsEmpty())
      if (Payload.IsValid())
        Payload->TryGetStringField(TEXT("destinationPath"), ExportPath);

    if (ExportPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("exportPath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *WorldToExport = nullptr;
    if (!LevelPath.IsEmpty()) {
      // If levelPath provided, we should probably load it first? Or export from
      // asset. Exporting unloaded level asset usually involves loading it. For
      // now, if levelPath is current, use current. If not, error (or attempt
      // load).
      UWorld *Current = GetActiveWorld();
      if (Current && (Current->GetOutermost()->GetName() == LevelPath ||
                      Current->GetPathName() == LevelPath)) {
        WorldToExport = Current;
      } else {
        // Should we load?
        // SendAutomationError(RequestingSocket, RequestId, TEXT("Level must be
        // loaded to export"), TEXT("LEVEL_NOT_LOADED")); return true; For
        // robustness, let's assume export current if path matches or empty.
      }
    }
    if (!WorldToExport)
      WorldToExport = GetActiveWorld();

    if (!WorldToExport) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Ensure directory
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

    // Determine export format based on path
    bool bSaved = false;
    FString ExportMethod;

    // Check if it's a package path (starts with / but not a drive letter)
    if (ExportPath.StartsWith(TEXT("/Game/")) || (ExportPath.StartsWith(TEXT("/")) && !ExportPath.Contains(TEXT(":"))))
    {
      // Package path export - use SaveMap
      bSaved = FEditorFileUtils::SaveMap(WorldToExport, ExportPath);
      ExportMethod = TEXT("Package SaveMap");
    }
    else
    {
      // For file system paths, use SaveMap with a temp package path then copy
      // Generate a temp package path based on level name
      FString LevelName = WorldToExport->GetMapName();
      FString TempPackagePath = FString::Printf(TEXT("/Game/_Temp/%s_Export"), *LevelName);
      
      if (FEditorFileUtils::SaveMap(WorldToExport, TempPackagePath))
      {
        // Find the saved .umap file and copy to destination
        FString TempFilePath = FPackageName::LongPackageNameToFilename(TempPackagePath, TEXT(".umap"));
        if (FPaths::FileExists(TempFilePath))
        {
          bSaved = IFileManager::Get().Copy(*ExportPath, *TempFilePath) == COPY_OK;
          // Clean up temp file
          IFileManager::Get().Delete(*TempFilePath);
        }
        ExportMethod = TEXT("File system copy via SaveMap");
      }
      else
      {
        ExportMethod = TEXT("SaveMap failed");
      }
    }

    if (bSaved) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("exportPath"), ExportPath);
      Resp->SetStringField(TEXT("method"), ExportMethod);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level exported"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to export level - export returned false"),
                          TEXT("EXPORT_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("import_level")) {
    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    if (SourcePath.IsEmpty())
      if (Payload.IsValid())
        Payload->TryGetStringField(TEXT("packagePath"), SourcePath); // Mapping

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath/packagePath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // If SourcePath is a package (starts with /Game), handle as Duplicate/Copy
    if (SourcePath.StartsWith(TEXT("/"))) {
      if (DestinationPath.IsEmpty()) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("destinationPath required for asset copy"),
                               nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
      }
      if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level imported (duplicated)"), nullptr);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Failed to duplicate level asset"), nullptr,
                               TEXT("IMPORT_FAILED"));
      }
      return true;
    }

    // If SourcePath is file, try Import
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    FString DestPath = DestinationPath.IsEmpty()
                           ? TEXT("/Game/Maps")
                           : FPaths::GetPath(DestinationPath);
    FString DestName = FPaths::GetBaseFilename(
        DestinationPath.IsEmpty() ? SourcePath : DestinationPath);

    TArray<FString> Files;
    Files.Add(SourcePath);
    // FEditorFileUtils::Import(DestPath, DestName); // Ambiguous/Removed
    // Use GEditor->ImportMap or handle via AssetTools
    // Simple fallback:
    if (GEditor) {
      // ImportMap is usually for T3D. If SourcePath is .umap, we should
      // Copy/Load. Assuming T3D import or similar:
      // GEditor->ImportMap(*DestPath, *DestName, *SourcePath);
      // ImportMap is deprecated/removed. For .umap files, manual import or Copy
      // is preferred.
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Direct map file import not supported. Use "
                                  "import_level with a package path to copy."),
                             nullptr, TEXT("NOT_IMPLEMENTED"));
      return true;
    }
    // Automation of Import is tricky without a factory wrapper.
    // Use AssetTools Import.

    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("File-based level import not fully automatic yet"), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
  }
  if (EffectiveAction == TEXT("add_sublevel")) {
    FString SubLevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("subLevelPath"), SubLevelPath);
    if (SubLevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SubLevelPath);

    if (SubLevelPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("subLevelPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Robustness: Cleanup before adding
    if (GEditor) {
      GEditor->ForceGarbageCollection(true);
    }

    // Verify file existence (more robust than DoesPackageExist for new files)
    FString Filename;
    bool bFileFound = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            SubLevelPath, Filename, FPackageName::GetMapPackageExtension())) {
      if (IFileManager::Get().FileExists(*Filename)) {
        bFileFound = true;
      }
    }

    // Fallback: Check without conversion if it's already a file path?
    if (!bFileFound && IFileManager::Get().FileExists(*SubLevelPath)) {
      bFileFound = true;
    }

    if (!bFileFound) {
      // Try checking DoesPackageExist as last resort
      if (!FPackageName::DoesPackageExist(SubLevelPath)) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found: %s"), *SubLevelPath),
            nullptr, TEXT("PACKAGE_NOT_FOUND"));
        return true;
      }
    }

    FString StreamingMethod = TEXT("Blueprint");
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("streamingMethod"), StreamingMethod);

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor unavailable"), nullptr,
                             TEXT("NO_EDITOR"));
      return true;
    }

    UWorld *World = GetActiveWorld();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Determine streaming class
    UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase)) {
      StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
    }

    ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *SubLevelPath, StreamingClass);
    if (NewLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sublevel added successfully"), nullptr);
    } else {
      // Did we fail because it's already there?
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to add sublevel %s (Check logs)"),
                          *SubLevelPath),
          nullptr, TEXT("ADD_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("create_sublevel")) {
    // Extract required parameters
    FString SublevelName;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("sublevelName"), SublevelName);
    }
    
    FString ParentLevel;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("parentLevel"), ParentLevel);
    }
    
    if (SublevelName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("sublevelName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    if (ParentLevel.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("parentLevel required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Extract optional streaming method
    FString StreamingMethod = TEXT("blueprint");
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("streamingMethod"), StreamingMethod);
    }
    
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    
    // Find the parent world
    UWorld *ParentWorld = nullptr;
    
    // Check if the current world matches the parent level
    UWorld *CurrentWorld = GetActiveWorld();
    if (CurrentWorld) {
      FString CurrentPackageName = CurrentWorld->GetOutermost()->GetName();
      if (CurrentPackageName == ParentLevel ||
          CurrentWorld->GetMapName() == ParentLevel) {
        ParentWorld = CurrentWorld;
      }
    }
    
    // If parent world not found in current context, try to load it
    if (!ParentWorld) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Parent level not found: %s"), *ParentLevel),
                             nullptr, TEXT("PARENT_NOT_FOUND"));
      return true;
    }
    
    // Construct valid package path for the new sublevel
    FString SublevelPath;
    if (SublevelName.StartsWith(TEXT("/"))) {
      SublevelPath = SublevelName;
    } else {
      // Place sublevel in the same directory as the parent level
      FString ParentDir = FPaths::GetPath(ParentLevel);
      if (ParentDir.IsEmpty()) {
        ParentDir = TEXT("/Game/Maps");
      }
      SublevelPath = FString::Printf(TEXT("%s/%s"), *ParentDir, *SublevelName);
    }
    
    // Ensure it has a valid package name
    if (!SublevelPath.EndsWith(TEXT("_Level")) && !SublevelPath.EndsWith(TEXT("_Sublevel"))) {
      // Optional: Add suffix for clarity
      SublevelPath = FString::Printf(TEXT("%s_Sublevel"), *SublevelPath);
    }
    
    // Check if sublevel already exists
    if (FPackageName::DoesPackageExist(SublevelPath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Sublevel already exists: %s"), *SublevelPath),
                             nullptr, TEXT("ALREADY_EXISTS"));
      return true;
    }
    
    // Create new map for the sublevel
    if (GEditor->IsPlaySessionInProgress()) {
      GEditor->RequestEndPlayMap();
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Cannot create sublevel while Play In Editor is active."), nullptr,
          TEXT("PIE_ACTIVE"));
      return true;
    }
    
    // Cleanup before creating
    if (IsInGameThread() && !IsAsyncLoadingSuspended() && !IsAssetStreamingSuspended()) {
      FlushRenderingCommands();
      GEditor->ForceGarbageCollection(true);
      FlushRenderingCommands();
    } else {
      GEditor->ForceGarbageCollection(true);
    }
    
    // Create the new level
    UWorld *NewWorld = GEditor->NewMap(true);
    if (!NewWorld) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to create new map for sublevel"), nullptr,
                             TEXT("CREATION_FAILED"));
      return true;
    }
    
    // Ensure directory exists before saving
    FString Filename;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            SublevelPath, Filename, FPackageName::GetMapPackageExtension())) {
      IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    }
    
    // Save the new sublevel
    // UE 5.7: Streaming management is automatic. Ensure all pending streaming
    // requests are processed before saving to avoid conflicts.
    // CRITICAL: Check if asset streaming is suspended and wait for it to resume
    bool bSaved = false;
    if (IsAssetStreamingSuspended()) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
             TEXT("create_sublevel: Asset streaming is suspended, waiting for it to resume..."));
      
      float WaitTime = 0.0f;
      const float MaxWaitTime = 5.0f;
      const float SleepInterval = 0.1f;
      
      while (IsAssetStreamingSuspended() && WaitTime < MaxWaitTime) {
        FPlatformProcess::Sleep(SleepInterval);
        WaitTime += SleepInterval;
      }
      
      if (IsAssetStreamingSuspended()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, 
               TEXT("create_sublevel: Asset streaming still suspended after %fs, cannot save"), MaxWaitTime);
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Asset streaming is suspended - cannot save while another streaming operation is in progress"), 
                               nullptr, TEXT("SAVE_FAILED"));
        return true;
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
               TEXT("create_sublevel: Asset streaming resumed after %fs, proceeding with save"), WaitTime);
      }
    }
    
    IStreamingManager::Get().BlockTillAllRequestsFinished(5.0f, false);
    bSaved = FEditorFileUtils::SaveMap(NewWorld, SublevelPath);
    
    if (!bSaved) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to save sublevel"), nullptr,
                             TEXT("SAVE_FAILED"));
      return true;
    }
    
    // Determine streaming class based on method
    UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
    FString StreamingState = TEXT("Blueprint");
    if (StreamingMethod.Equals(TEXT("always_loaded"), ESearchCase::IgnoreCase)) {
      StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
      StreamingState = TEXT("AlwaysLoaded");
    }
    
    // Add the created level as a sublevel to the parent world
    ULevelStreaming *AddedLevel = UEditorLevelUtils::AddLevelToWorld(
        ParentWorld, *SublevelPath, StreamingClass);
    
    if (!AddedLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to add sublevel %s to parent world"), *SublevelPath),
                             nullptr, TEXT("ADD_FAILED"));
      return true;
    }
    
    // Build success response
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("levelPath"), SublevelPath);
    Resp->SetStringField(TEXT("parentLevel"), ParentLevel);
    Resp->SetStringField(TEXT("streamingState"), StreamingState);
    Resp->SetBoolField(TEXT("success"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Sublevel created and added: %s"), *SublevelPath),
                           Resp, FString());
    return true;
  }

  // Handle direct HLOD and World Partition actions by forwarding to LevelStructure handlers
  if (EffectiveAction == TEXT("configure_hlod_settings") ||
      EffectiveAction == TEXT("build_hlod_for_level") ||
      EffectiveAction == TEXT("get_world_partition_cells") ||
      EffectiveAction == TEXT("configure_world_partition") ||
      EffectiveAction == TEXT("create_world_partition_cell") ||
      EffectiveAction == TEXT("configure_runtime_loading") ||
      EffectiveAction == TEXT("configure_world_settings"))
  {
      TSharedPtr<FJsonObject> StructurePayload = MakeShared<FJsonObject>();
      if (Payload.IsValid()) {
          for (const auto& Field : Payload->Values) {
              StructurePayload->Values.Add(Field.Key, Field.Value);
          }
      }
      StructurePayload->SetStringField(TEXT("subAction"), EffectiveAction);

      if (HandleManageLevelStructureAction(RequestId, Action, StructurePayload, RequestingSocket)) {
          return true;
      }
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Unknown level action: %s"), *EffectiveAction),
      nullptr, TEXT("UNKNOWN_ACTION"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Level actions require editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
