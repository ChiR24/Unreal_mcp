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
	// Issue a fence on the render thread
	FRenderCommandFence Fence;
	Fence.BeginFence();
	
	// Wait for all current rendering commands to complete
	// Intel Gen12 drivers crash when SaveMap triggers recursive FlushRenderingCommands.
	// We use a single flush and a fence to ensure GPU work is complete safely.
	FlushRenderingCommands();
	
	// Wait for the fence to signal (ensures GPU work is complete)
	Fence.Wait();
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
       Lower == TEXT("add_sublevel"));
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

      // Force any pending work to complete
      FlushRenderingCommands();

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
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
        if (ULevelEditorSubsystem *LevelEditorSS = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
          bool bSaved = false;
#if __has_include("FileHelpers.h")
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
#if __has_include("FileHelpers.h")
      if (UWorld *World = GetActiveWorld()) {
        bSaved = FEditorFileUtils::SaveMap(World, SavePath);
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
      
      // Capture for deferred response
      TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf = this;
      const FString CapturedRequestId = RequestId;
      const bool bCapturedSaved = bSaved;
      const FString CapturedSavePath = SavePath;
      TSharedPtr<FMcpBridgeWebSocket> CapturedSocket = RequestingSocket;
      TSharedPtr<FJsonObject> CapturedResp = Resp;
      
      // Defer response to allow rendering commands to settle after WP save
      if (GEditor)
      {
        FTimerDelegate ResponseDelegate;
        ResponseDelegate.BindLambda([WeakSelf, CapturedSocket, CapturedRequestId, bCapturedSaved, CapturedSavePath, CapturedResp]()
        {
          if (UMcpAutomationBridgeSubsystem* Self = WeakSelf.Get())
          {
            if (bCapturedSaved) {
              Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, true,
                                 FString::Printf(TEXT("Level saved as %s"), *CapturedSavePath),
                                 CapturedResp, FString());
            } else {
              Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, false,
                                 TEXT("Failed to save level as"), nullptr,
                                 TEXT("SAVE_FAILED"));
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
          SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Failed to save level as"), nullptr,
                                 TEXT("SAVE_FAILED"));
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
    if (GEditor) {
      FlushRenderingCommands();
      GEditor->ForceGarbageCollection(true);
      FlushRenderingCommands();
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

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Level actions require editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
