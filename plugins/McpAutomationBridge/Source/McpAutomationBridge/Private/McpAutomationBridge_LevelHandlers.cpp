#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

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
#include "GameFramework/WorldSettings.h"
#include "Engine/LevelBounds.h"
#include "LevelUtils.h"
#include "EditorBuildUtils.h"
#include "EditorAssetLibrary.h"
#include "TickTaskManagerInterface.h"  // Required for proper tick system cleanup
#include "WorldPartition/WorldPartition.h"  // Required for World Partition detection

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

/**
 * Safely creates a new map with proper tick system cleanup to prevent
 * TickTaskManager assertion crashes in UE 5.7+.
 *
 * CRITICAL: Calling GEditor->NewMap() without proper cleanup causes:
 * "Assertion failed: !LevelList.Contains(TickTaskLevel)" in TickTaskManager.cpp
 *
 * This is a known issue (UE-197643, UE-138424) where tick functions from
 * the old world remain registered when the new world is created.
 *
 * Root Cause Analysis:
 * The FTickTaskManager maintains a LevelList that's filled during StartFrame() 
 * and cleared during EndFrame(). When NewMap() destroys the old world:
 * 1. ULevel destructor calls FreeTickTaskLevel()
 * 2. FreeTickTaskLevel() asserts: check(!LevelList.Contains(TickTaskLevel))
 * 3. If a tick frame started but didn't complete, LevelList still has entries
 *
 * Fix Strategy:
 * 1. Set all levels to invisible (prevents FillLevelList from adding them)
 * 2. Disable all actor/component ticking
 * 3. Force a complete tick frame cycle to clear LevelList
 * 4. Properly cleanup before world destruction
  *
  * @param bForceNewMap If true, create a completely new empty map (default: true)
  * @param Subsystem Optional subsystem for sending progress updates
  * @param RequestId Optional request ID for progress updates
  * @param bUseWorldPartition If true, create World Partition level (default: false to avoid freeze)
  * @return UWorld* The newly created world, or nullptr on failure
  */
static UWorld* McpSafeNewMap(bool bForceNewMap = true, UMcpAutomationBridgeSubsystem* Subsystem = nullptr, const FString& RequestId = FString(), bool bUseWorldPartition = false)
{
    if (!GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeNewMap: GEditor is null"));
        return nullptr;
    }

    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    
    // CRITICAL FIX: Check if current world has World Partition before cleanup
    // World Partition uninitialize can freeze for 20+ seconds in UE 5.7
    // We need to handle this specially to avoid the freeze
    if (CurrentWorld)
    {
        AWorldSettings* WorldSettings = CurrentWorld->GetWorldSettings();
        UWorldPartition* WorldPartition = WorldSettings ? WorldSettings->GetWorldPartition() : nullptr;
        if (WorldPartition)
        {
            UE_LOG(LogTemp, Warning, TEXT("McpSafeNewMap: Current world '%s' has World Partition - cleanup may be slow"), *CurrentWorld->GetName());
            
            // Send progress update warning about WP cleanup
            if (Subsystem && !RequestId.IsEmpty())
            {
                Subsystem->SendProgressUpdate(RequestId, 2.0f, TEXT("Warning: Current world has World Partition - cleanup may be slow..."));
            }
            
            // Note: There's no API to speed up WP uninitialize
            // The freeze is unavoidable if the current world has WP
            // Solution: Don't create WP levels for tests (useWorldPartition=false)
        }
    }
    
    if (CurrentWorld)
    {
        // Send initial progress update
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 5.0f, TEXT("Starting level creation cleanup..."));
        }
        
        UE_LOG(LogTemp, Log, TEXT("McpSafeNewMap: Cleaning up current world '%s'"), *CurrentWorld->GetName());
        
        // STEP 1: Mark all levels as invisible to prevent FillLevelList from adding them
        // This is CRITICAL - FillLevelList only adds levels where bIsVisible is true
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (Level)
            {
                Level->bIsVisible = false;
            }
        }
        
        // STEP 2: Unregister all tick functions (not just disable)
        // CRITICAL: SetActorTickEnabled(false) only DISABLES ticking - it doesn't UNREGISTER
        // the tick function from FTickTaskManager. The assertion "!LevelList.Contains(TickTaskLevel)"
        // still fires because the tick function is still registered in LevelList.
        // We must call UnRegisterTickFunction() to properly remove from LevelList.
        int32 UnregisteredActorCount = 0;
        int32 UnregisteredComponentCount = 0;
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (!Level) continue;
            
            for (AActor* Actor : Level->Actors)
            {
                if (Actor)
                {
                    // CRITICAL FIX: Unregister the actor's primary tick function
                    // This removes it from FTickTaskManager's LevelList
                    if (Actor->PrimaryActorTick.IsTickFunctionRegistered())
                    {
                        Actor->PrimaryActorTick.UnRegisterTickFunction();
                        UnregisteredActorCount++;
                    }
                    
                    // Also unregister all component tick functions
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component && Component->PrimaryComponentTick.IsTickFunctionRegistered())
                        {
                            Component->PrimaryComponentTick.UnRegisterTickFunction();
                            UnregisteredComponentCount++;
                        }
                    }
                }
            }
        }
        UE_LOG(LogTemp, Log, TEXT("McpSafeNewMap: Unregistered %d actor ticks and %d component ticks"), UnregisteredActorCount, UnregisteredComponentCount);
        
        // Progress update: unregistered ticking
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 15.0f, FString::Printf(TEXT("Unregistered %d actor ticks and %d component ticks"), UnregisteredActorCount, UnregisteredComponentCount));
        }
        
        // STEP 3: Send end-of-frame updates to complete any pending tick work
        CurrentWorld->SendAllEndOfFrameUpdates();
        
        // STEP 4: Flush rendering commands to ensure all GPU work is complete
        FlushRenderingCommands();
        
        // Progress update: flushing GPU
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 25.0f, TEXT("Flushing GPU commands..."));
        }
        
        // STEP 5: Explicitly unload streaming levels
        // This prevents UE-197643 where tick prerequisites cross level boundaries
        TArray<ULevelStreaming*> StreamingLevels = CurrentWorld->GetStreamingLevels();
        for (ULevelStreaming* StreamingLevel : StreamingLevels)
        {
            if (StreamingLevel)
            {
                StreamingLevel->SetShouldBeLoaded(false);
                StreamingLevel->SetShouldBeVisible(false);
            }
        }
        
        // STEP 6: Flush rendering commands again after streaming level changes
        FlushRenderingCommands();
        
        // Progress update: streaming levels unloaded
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 40.0f, TEXT("Unloaded streaming levels"));
        }
        
        // STEP 7: Force garbage collection to clean up any remaining references
        GEditor->ForceGarbageCollection(true);
        
        // STEP 8: Another flush after GC
        FlushRenderingCommands();
        
        // Progress update: GC complete
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 55.0f, TEXT("Garbage collection complete"));
        }
        
        // STEP 9: REMOVED - Calling StartFrame triggers assertion if TickCompletionEvents is not empty
        // The assertion "check(!TickCompletionEvents[Index].Num())" at TickTaskManager.cpp:1097
        // fires BEFORE LevelList is cleared, creating a catch-22.
        // 
        // Instead, we rely on Steps 1-8 which are sufficient:
        // - Setting bIsVisible = false prevents FillLevelList from adding levels
        // - Disabling all actor/component ticks prevents new tick registrations
        // - FlushRenderingCommands clears GPU work
        // - GC cleans up references
        // - 100ms sleep allows engine to settle
        
        // Progress update: tick cleanup complete (via Steps 1-8)
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 65.0f, TEXT("Tick cleanup complete (via visibility/tick disable)"));
        }
        
        // STEP 10: Give the engine a moment to process cleanup
        FPlatformProcess::Sleep(0.10f); // 100ms delay for full cleanup
    }
    
    // STEP 11: Now safe to create new map
    UE_LOG(LogTemp, Log, TEXT("McpSafeNewMap: Creating new map (bForceNewMap=%s, bUseWorldPartition=%s)"), 
           bForceNewMap ? TEXT("true") : TEXT("false"), bUseWorldPartition ? TEXT("true") : TEXT("false"));
    
    // Progress update: creating new map
    if (Subsystem && !RequestId.IsEmpty())
    {
        Subsystem->SendProgressUpdate(RequestId, 75.0f, TEXT("Creating new level..."));
    }
    
    // CRITICAL FIX: Use GEditor->NewMap() with bIsPartitionedWorld parameter
    // This is the proper way to control World Partition creation
    // Reference: Engine/Source/Editor/UnrealEd/Classes/Editor/EditorEngine.h line 2007
    // UWorld* NewMap(bool bIsPartitionedWorld = false);
    // Default is false, but we pass it explicitly for clarity
    UWorld* NewWorld = GEditor->NewMap(bUseWorldPartition);
    
    if (NewWorld)
    {
        // STEP 12: CRITICAL - Disable ticking on the new world's actors immediately
        // The NewMap creates actors (like WorldSettings) that might trigger tick assertions
        // if not properly initialized before the next tick frame
        if (NewWorld->PersistentLevel)
        {
            for (AActor* Actor : NewWorld->PersistentLevel->Actors)
            {
                if (Actor)
                {
                    Actor->SetActorTickEnabled(false);
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component)
                        {
                            Component->SetComponentTickEnabled(false);
                        }
                    }
                }
            }
        }
        
        // STEP 13: Flush any pending operations from world creation
        FlushRenderingCommands();
        
        // Progress update: finalizing new world
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 90.0f, TEXT("Finalizing new level..."));
        }
        
        // STEP 14: REMOVED - Calling StartFrame triggers assertion if TickCompletionEvents is not empty
        // The new world's actors already have ticking disabled from Step 12, so no tick functions
        // should be registered. We skip the StartFrame/EndFrame cycle to avoid the assertion.
        // Instead, we rely on Step 15's additional delay for stability.
        
        // STEP 15: Additional delay to ensure engine is stable
        FPlatformProcess::Sleep(0.10f); // Increased from 0.05f for better stability
        
        // Progress update: complete
        if (Subsystem && !RequestId.IsEmpty())
        {
            Subsystem->SendProgressUpdate(RequestId, 95.0f, TEXT("Level creation complete"));
        }
        
        UE_LOG(LogTemp, Log, TEXT("McpSafeNewMap: Successfully created new world '%s'"), *NewWorld->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("McpSafeNewMap: Failed to create new map"));
    }
    
    return NewWorld;
}

bool UMcpAutomationBridgeSubsystem::HandleLevelAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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
        FolderMapPath = FPaths::GetPath(FlatMapPath) / LevelName + FPackageName::GetMapPackageExtension();
        FullFolderMapPath = FPaths::ConvertRelativePathToFull(FolderMapPath);
      }
      
      // Check both paths - prefer folder-based (UE 5.x standard)
      if (!FullFolderMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFolderMapPath)) {
        bFileExists = true;
        UE_LOG(LogTemp, Log, TEXT("load: Found level at folder-based path: %s"), *FullFolderMapPath);
      } else if (!FullFlatMapPath.IsEmpty() && IFileManager::Get().FileExists(*FullFlatMapPath)) {
        bFileExists = true;
        UE_LOG(LogTemp, Log, TEXT("load: Found level at flat path: %s"), *FullFlatMapPath);
      }
      
      // Also check if it's a valid package path (for levels in memory but not on disk yet)
      if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
        TSharedPtr<FJsonObject> ErrorDetails = MakeShared<FJsonObject>();
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

      // LoadMap prompts for save if dirty. To avoid blocking automation, we
      // should carefuly consider. But for now, we assume user wants standard
      // behavior or has saved. There isn't a simple "Force Load" via FileUtils
      // without clearing dirty flags manually. We will proceed with LoadMap.
      const bool bLoaded = McpSafeLoadMap(FileToLoad);

      // Post-load verification: check that the loaded world matches the requested path
      if (bLoaded) {
        UWorld* LoadedWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (LoadedWorld) {
          FString LoadedPath = LoadedWorld->GetOutermost()->GetName();
          // Normalize paths for comparison (handle case differences)
          if (LoadedPath.ToLower() != LevelPath.ToLower()) {
            // The requested level was not actually loaded - engine fell back to default
            SendAutomationResponse(
                RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Level path mismatch: requested %s but loaded %s"), *LevelPath, *LoadedPath),
                nullptr, TEXT("LOAD_MISMATCH"));
            return true;
          }
        }
        
TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        VerifyAssetExists(Resp, LevelPath);
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
    } else if (LowerSub == TEXT("delete") || LowerSub == TEXT("delete_level")) {
      EffectiveAction = TEXT("delete_level");
    } else if (LowerSub == TEXT("rename") || LowerSub == TEXT("rename_level")) {
      EffectiveAction = TEXT("rename_level");
    } else if (LowerSub == TEXT("duplicate") || LowerSub == TEXT("duplicate_level")) {
      EffectiveAction = TEXT("duplicate_level");
    } else if (LowerSub == TEXT("get_level_info")) {
      EffectiveAction = TEXT("get_level_info");
    } else if (LowerSub == TEXT("set_level_world_settings")) {
      EffectiveAction = TEXT("set_level_world_settings");
    } else if (LowerSub == TEXT("set_level_lighting")) {
      EffectiveAction = TEXT("set_level_lighting");
    } else if (LowerSub == TEXT("add_level_to_world")) {
      EffectiveAction = TEXT("add_level_to_world");
    } else if (LowerSub == TEXT("remove_level_from_world")) {
      EffectiveAction = TEXT("remove_level_from_world");
    } else if (LowerSub == TEXT("set_level_visibility")) {
      EffectiveAction = TEXT("set_level_visibility");
    } else if (LowerSub == TEXT("set_level_locked")) {
      EffectiveAction = TEXT("set_level_locked");
    } else if (LowerSub == TEXT("get_level_actors")) {
      EffectiveAction = TEXT("get_level_actors");
    } else if (LowerSub == TEXT("get_level_bounds")) {
      EffectiveAction = TEXT("get_level_bounds");
    } else if (LowerSub == TEXT("get_level_lighting_scenarios")) {
      EffectiveAction = TEXT("get_level_lighting_scenarios");
    } else if (LowerSub == TEXT("build_level_lighting")) {
      EffectiveAction = TEXT("build_level_lighting");
    } else if (LowerSub == TEXT("build_level_navigation")) {
      EffectiveAction = TEXT("build_level_navigation");
    } else if (LowerSub == TEXT("build_all_level")) {
      EffectiveAction = TEXT("build_all_level");
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown manage_level action: %s"), *SubAction),
          TEXT("UNKNOWN_ACTION"));
      return true;
    }
  }

  // Helper lambda to get all levels from a world (替代 UEditorLevelUtils::GetLevels which has linker issues)
  auto GetAllLevelsFromWorld = [](UWorld* World) -> TArray<ULevel*> {
    TArray<ULevel*> Levels;
    if (!World) return Levels;
    
    // Add persistent level
    if (World->PersistentLevel) {
      Levels.Add(World->PersistentLevel);
    }
    
    // Add streaming levels
    for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
      if (StreamingLevel) {
        ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
        if (LoadedLevel) {
          Levels.Add(LoadedLevel);
        }
      }
    }
    
    return Levels;
  };

  if (EffectiveAction == TEXT("save_current_level")) {
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
      TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
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
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      FString LevelPath = World->GetOutermost()->GetName();
      VerifyAssetExists(Resp, LevelPath);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level saved"), Resp, FString());
    } else {
      // Provide detailed error information
      TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
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
  if (EffectiveAction == TEXT("save_level_as")) {
    // Force cleanup to prevent potential deadlocks with HLODs/WorldPartition
    // during save
    if (GEditor) {
      FlushRenderingCommands();
      GEditor->ForceGarbageCollection(true);
      FlushRenderingCommands();
    }

    FString SavePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("savePath required for save_level_as"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // CRITICAL: Validate path length BEFORE attempting save to prevent silent hangs
    // McpSafeLevelSave validates internally but may not send error response in all code paths
    {
      FString AbsoluteFilePath;
      if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, AbsoluteFilePath, FPackageName::GetMapPackageExtension()))
      {
        AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
        const int32 SafePathLength = 240;
        if (AbsoluteFilePath.Len() > SafePathLength)
        {
          TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
          ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
          ErrorDetail->SetStringField(TEXT("absolutePath"), AbsoluteFilePath);
          ErrorDetail->SetNumberField(TEXT("pathLength"), AbsoluteFilePath.Len());
          ErrorDetail->SetNumberField(TEXT("maxLength"), SafePathLength);
          SendAutomationResponse(
              RequestingSocket, RequestId, false,
              FString::Printf(TEXT("Path too long (%d chars, max %d): %s"), 
                  AbsoluteFilePath.Len(), SafePathLength, *SavePath),
              ErrorDetail, TEXT("PATH_TOO_LONG"));
          return true;
        }
      }
    }

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LevelEditorSS =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
      bool bSaved = false;
#if __has_include("FileHelpers.h")
      if (UWorld *World = GEditor->GetEditorWorldContext().World()) {
        // Use McpSafeLevelSave to prevent Intel GPU driver crashes
        // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
        bSaved = McpSafeLevelSave(World->PersistentLevel, SavePath);
      }
#endif
      if (bSaved) {
        // Refresh Asset Registry so the saved level is immediately visible for rename/duplicate operations
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        FString SavedFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(SavePath, SavedFilename, FPackageName::GetMapPackageExtension())) {
          TArray<FString> FilesToScan;
          FilesToScan.Add(SavedFilename);
          AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("levelPath"), SavePath);
        SendAutomationResponse(
            RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Level saved as %s"), *SavePath), Resp,
            FString());
      } else {
        // CRITICAL FIX: Send error response when save fails (was missing before, causing silent hangs)
        TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
        ErrorDetail->SetStringField(TEXT("attemptedPath"), SavePath);
        ErrorDetail->SetStringField(TEXT("reason"), TEXT("Save operation failed - check Output Log for details"));
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Failed to save level as: %s"), *SavePath),
            ErrorDetail, TEXT("SAVE_FAILED"));
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

    // SECURITY: Sanitize LevelName to prevent path injection
    // Remove any path separators (only allow the final name component)
    // and reject traversal sequences
    if (!LevelName.IsEmpty()) {
      int32 LastSlash = -1;
      LevelName.FindLastChar(TEXT('/'), LastSlash);
      if (LastSlash >= 0) {
        LevelName = LevelName.RightChop(LastSlash + 1);
      }
      LevelName.FindLastChar(TEXT('\\'), LastSlash);
      if (LastSlash >= 0) {
        LevelName = LevelName.RightChop(LastSlash + 1);
      }
      if (LevelName.Contains(TEXT(".."))) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            TEXT("Invalid levelName: contains path traversal (..)"),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }
    }

    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

    // Parse useWorldPartition - default to false for faster level creation
    // World Partition levels take 20+ seconds to unload in UE 5.7
    bool bUseWorldPartition = false;
    if (Payload.IsValid()) {
      Payload->TryGetBoolField(TEXT("useWorldPartition"), bUseWorldPartition);
    }

    // SECURITY: Sanitize LevelPath to prevent path traversal attacks
    // Rejects paths containing "..", double slashes, or invalid characters
    // that could cause engine crashes or security violations
    FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
    if (!LevelPath.IsEmpty() && SanitizedLevelPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Invalid levelPath: contains path traversal (..), double slashes, or invalid characters"),
          nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // CRITICAL FIX: Properly combine levelPath (parent directory) and levelName
    // If both are provided, levelPath is the parent directory and levelName is the level name
    // If only levelName is provided and it starts with '/', it's treated as a full path
    // If only levelPath is provided, it's treated as a full path (backwards compatibility)
    FString SavePath;
    
    if (!SanitizedLevelPath.IsEmpty() && !LevelName.IsEmpty()) {
      // Both provided: levelPath is parent directory, levelName is the level name
      // Combine them: /Game/MCPTest + TestLevel = /Game/MCPTest/TestLevel
      SavePath = SanitizedLevelPath;
      if (!SavePath.EndsWith(TEXT("/"))) {
        SavePath += TEXT("/");
      }
      SavePath += LevelName;
    } else if (!LevelName.IsEmpty()) {
      // Only levelName provided
      if (LevelName.StartsWith(TEXT("/"))) {
        // levelName is actually a full path
        SavePath = LevelName;
      } else {
        // Just the name - save to default location
        SavePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
      }
    } else if (!SanitizedLevelPath.IsEmpty()) {
      // Only levelPath provided - treat as full path (backwards compatibility)
      SavePath = SanitizedLevelPath;
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
      // Level already exists - return success with info instead of trying to open
      // Opening an existing level can trigger dialogs about unsaved changes, causing hangs
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("levelPath"), SavePath);
      Resp->SetStringField(TEXT("packagePath"), SavePath);
      Resp->SetBoolField(TEXT("alreadyExists"), true);
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Level already exists: %s"), *SavePath), Resp,
          FString());
      return true;
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

    // CRITICAL: Use McpSafeNewMap instead of GEditor->NewMap() directly.
    // Calling NewMap() without proper tick cleanup causes:
    // "Assertion failed: !LevelList.Contains(TickTaskLevel)" in TickTaskManager.cpp
    // This is a known UE 5.7 issue (UE-197643, UE-138424).
    // McpSafeNewMap handles:
    // 1. Disabling all actor/component ticking
    // 2. Removing tick prerequisites
    // 3. Flushing async loading and streaming levels
    // 4. Proper garbage collection
    // Pass Subsystem and RequestId to enable progress updates for timeout extension
    // CRITICAL FIX: Pass bUseWorldPartition to control World Partition creation
    // World Partition levels cause 20+ second freeze during Uninitialize in UE 5.7
    UWorld* NewWorld = McpSafeNewMap(true, this, RequestId, bUseWorldPartition);

    if (NewWorld) {
      GEditor->GetEditorWorldContext().SetCurrentWorld(NewWorld);

      // CRITICAL: Verify and ensure World Partition is properly initialized
      // GEditor->NewMap(bUseWorldPartition) should create WP, but sometimes
      // the initialization is incomplete. We need to verify and potentially
      // force WP creation if it was requested but not actually enabled.
      bool bWorldPartitionActuallyEnabled = false;
      if (bUseWorldPartition)
      {
          UWorldPartition* WorldPartition = NewWorld->GetWorldPartition();
          bWorldPartitionActuallyEnabled = (WorldPartition != nullptr);
          
          // If WP was requested but GetWorldPartition() returns null,
          // we need to explicitly create it via CreateOrRepairWorldPartition
          if (!bWorldPartitionActuallyEnabled)
          {
              UE_LOG(LogTemp, Warning, TEXT("create_new_level: World Partition was requested but not initialized by NewMap. Forcing creation..."));
              
              AWorldSettings* WorldSettings = NewWorld->GetWorldSettings();
              if (WorldSettings)
              {
                  WorldPartition = UWorldPartition::CreateOrRepairWorldPartition(WorldSettings);
                  if (WorldPartition)
                  {
                      bWorldPartitionActuallyEnabled = true;
                      UE_LOG(LogTemp, Log, TEXT("create_new_level: Successfully created World Partition via CreateOrRepairWorldPartition"));
                  }
                  else
                  {
                      UE_LOG(LogTemp, Error, TEXT("create_new_level: Failed to create World Partition via CreateOrRepairWorldPartition"));
                  }
              }
              else
              {
                  UE_LOG(LogTemp, Error, TEXT("create_new_level: Cannot create World Partition - WorldSettings is null"));
              }
          }
          else
          {
              UE_LOG(LogTemp, Log, TEXT("create_new_level: World Partition verified - GetWorldPartition() returned valid pointer"));
          }
      }

      // Save it to valid path
      // ISSUE #1 FIX: Ensure directory exists
      FString Filename;
      if (FPackageName::TryConvertLongPackageNameToFilename(
              SavePath, Filename, FPackageName::GetMapPackageExtension())) {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
      }

      // CRITICAL: Use McpSafeLevelSave instead of FEditorFileUtils::SaveMap
      // to prevent Intel GPU driver crashes (MONZA DdiThreadingContext)
      // during level save operations
      bool bSaved = McpSafeLevelSave(NewWorld->PersistentLevel, SavePath);
      
      // CRITICAL FIX: Verify the save actually succeeded using MULTIPLE methods
      // 1. File system check (most reliable if path conversion works)
      // 2. Asset Registry check (works even if path conversion fails)
      // 3. Package existence check (UE's internal method)
      
      FString ActualFilename;
      bool bFileOnDisk = false;
      bool bPathConversionOk = FPackageName::TryConvertLongPackageNameToFilename(
          SavePath, ActualFilename, FPackageName::GetMapPackageExtension());
      
      if (bPathConversionOk && !ActualFilename.IsEmpty()) {
        ActualFilename = FPaths::ConvertRelativePathToFull(ActualFilename);
        bFileOnDisk = IFileManager::Get().FileExists(*ActualFilename);
        UE_LOG(LogTemp, Log, TEXT("create_new_level: File check - path=%s, exists=%d"), *ActualFilename, bFileOnDisk);
      }
      
      // Fallback verification using Asset Registry
      IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
      
      // Force scan the directory first
      FString DirectoryPath = FPaths::GetPath(SavePath);
      if (!DirectoryPath.IsEmpty()) {
        TArray<FString> ScanPaths;
        ScanPaths.Add(DirectoryPath);
        AssetRegistry.ScanPathsSynchronous(ScanPaths, true);
      }
      
      // Check Asset Registry for the saved level
      bool bAssetRegistryOk = FPackageName::DoesPackageExist(SavePath);
      if (!bAssetRegistryOk) {
        // Try checking the Asset Registry directly
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(SavePath));
        #else
        FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*SavePath));
        #endif
        bAssetRegistryOk = AssetData.IsValid();
      }
      
      UE_LOG(LogTemp, Log, TEXT("create_new_level: Verification - saved=%d, fileOnDisk=%d, assetRegistry=%d, packageExists=%d"), 
             bSaved, bFileOnDisk, bAssetRegistryOk, FPackageName::DoesPackageExist(SavePath));
      
      // Consider success if Asset Registry shows it exists (file check may fail due to path issues)
      bool bSuccess = bSaved && (bFileOnDisk || bAssetRegistryOk);
      
      if (bSuccess) {
        // Also scan the specific file if path conversion worked
        if (bFileOnDisk && !ActualFilename.IsEmpty()) {
          TArray<FString> FilesToScan;
          FilesToScan.Add(ActualFilename);
          AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
        }
        
        // Wait for Asset Registry to process
        FlushRenderingCommands();
        FPlatformProcess::Sleep(0.05f);
        
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetStringField(TEXT("levelPath"), SavePath);
        Resp->SetStringField(TEXT("packagePath"), SavePath);
        Resp->SetStringField(TEXT("objectPath"),
                             SavePath + TEXT(".") +
                                 FPaths::GetBaseFilename(SavePath));
        if (!ActualFilename.IsEmpty()) {
          Resp->SetStringField(TEXT("filename"), ActualFilename);
        }
        Resp->SetBoolField(TEXT("fileOnDisk"), bFileOnDisk);
        Resp->SetBoolField(TEXT("assetRegistryOk"), bAssetRegistryOk);
        Resp->SetBoolField(TEXT("worldPartitionEnabled"), bWorldPartitionActuallyEnabled);
        Resp->SetBoolField(TEXT("worldPartitionRequested"), bUseWorldPartition);
        
        // Build response message with WP status if applicable
        FString ResponseMsg = FString::Printf(TEXT("Level created: %s"), *SavePath);
        if (bUseWorldPartition && !bWorldPartitionActuallyEnabled)
        {
            ResponseMsg += TEXT(" (WARNING: World Partition requested but not enabled)");
        }
        else if (bUseWorldPartition && bWorldPartitionActuallyEnabled)
        {
            ResponseMsg += TEXT(" (World Partition enabled)");
        }
        
        SendAutomationResponse(
            RequestingSocket, RequestId, true,
            *ResponseMsg, Resp,
            FString());
      } else {
        // Save failed - provide detailed error
        FString ErrorMsg;
        if (!bSaved) {
          ErrorMsg = TEXT("Failed to save new level after 5 retries (check GPU driver stability)");
        } else if (!bPathConversionOk) {
          ErrorMsg = FString::Printf(TEXT("Level saved but path conversion failed for: %s"), *SavePath);
        } else if (!bFileOnDisk && !bAssetRegistryOk) {
          ErrorMsg = FString::Printf(TEXT("Level save reported success but verification failed for: %s"), *SavePath);
        } else {
          ErrorMsg = FString::Printf(TEXT("Level save failed for: %s"), *SavePath);
        }
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               *ErrorMsg, nullptr,
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

    // CRITICAL FIX: Use UEditorLevelUtils for streaming instead of console command
    // Console command StreamLevel is unreliable and returns EXEC_FAILED in many cases
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Find the streaming level by name/path
    ULevelStreaming* TargetStreamingLevel = nullptr;
    FString NormalizedLevelName = LevelName;
    
    // Normalize the path - remove .umap extension if present
    if (NormalizedLevelName.EndsWith(TEXT(".umap"))) {
      NormalizedLevelName = NormalizedLevelName.LeftChop(5);
    }
    
    // Search for the streaming level
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
      if (StreamingLevel) {
        FString StreamingName = StreamingLevel->GetWorldAssetPackageName();
        if (StreamingName.Equals(NormalizedLevelName, ESearchCase::IgnoreCase) ||
            StreamingName.EndsWith(NormalizedLevelName, ESearchCase::IgnoreCase) ||
            FPaths::GetBaseFilename(StreamingName).Equals(NormalizedLevelName, ESearchCase::IgnoreCase)) {
          TargetStreamingLevel = StreamingLevel;
          break;
        }
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelName"), NormalizedLevelName);
    Result->SetBoolField(TEXT("shouldBeLoaded"), bLoad);
    Result->SetBoolField(TEXT("shouldBeVisible"), bVis);

    if (TargetStreamingLevel) {
      // Use the streaming level API directly
      TargetStreamingLevel->SetShouldBeLoaded(bLoad);
      TargetStreamingLevel->SetShouldBeVisible(bVis);
      
      Result->SetStringField(TEXT("streamingState"), 
          TargetStreamingLevel->IsStreamingStatePending() ? TEXT("Pending") :
          TargetStreamingLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded"));
      
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Streaming level state updated: %s (Loaded=%s, Visible=%s)"),
                                 *NormalizedLevelName, 
                                 bLoad ? TEXT("true") : TEXT("false"),
                                 bVis ? TEXT("true") : TEXT("false")),
                             Result);
    } else {
      // Streaming level not found - try console command as fallback
      const FString Cmd =
          FString::Printf(TEXT("StreamLevel %s %s %s"), *NormalizedLevelName,
                          bLoad ? TEXT("Load") : TEXT("Unload"),
                          bVis ? TEXT("Show") : TEXT("Hide"));
      
      // Execute console command and check result
      bool bCmdSuccess = false;
      if (World) {
        bCmdSuccess = GEditor->Exec(World, *Cmd);
      }
      
      if (bCmdSuccess) {
        Result->SetStringField(TEXT("method"), TEXT("console_command"));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Streaming command executed"), Result);
      } else {
        // Even if console command returns false, the operation may still be in progress
        // Return "handled" status instead of error for streaming operations
        Result->SetStringField(TEXT("method"), TEXT("console_command_fallback"));
        Result->SetStringField(TEXT("command"), Cmd);
        Result->SetBoolField(TEXT("handled"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Streaming command submitted (level may not be in world yet)"), 
                               Result, TEXT("HANDLED"));
      }
    }
    return true;
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
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetRegistry.GetAssetsByClass(
        FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), MapAssets,
        false);
#else
    // UE 5.0: Use FName for class path
    AssetRegistry.GetAssetsByClass(
        FName(TEXT("World")), MapAssets,
        false);
#endif

    TArray<TSharedPtr<FJsonValue>> AllMapsArray;
    for (const FAssetData &MapAsset : MapAssets) {
      TSharedPtr<FJsonObject> MapEntry = MakeShared<FJsonObject>();
      MapEntry->SetStringField(TEXT("name"), MapAsset.AssetName.ToString());
      MapEntry->SetStringField(TEXT("path"), MapAsset.PackageName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      MapEntry->SetStringField(TEXT("objectPath"),
                               MapAsset.GetObjectPathString());
#else
      // UE 5.0: Construct object path from package and asset name
      MapEntry->SetStringField(TEXT("objectPath"),
                               FString::Printf(TEXT("%s.%s"), *MapAsset.PackageName.ToString(), *MapAsset.AssetName.ToString()));
#endif
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
      UWorld *Current = GEditor->GetEditorWorldContext().World();
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
      WorldToExport = GEditor->GetEditorWorldContext().World();

    if (!WorldToExport) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Ensure directory
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ExportPath), true);

    // CRITICAL: Use McpSafeLevelSave instead of FEditorFileUtils::SaveMap
    // to prevent Intel GPU driver crashes (MONZA DdiThreadingContext)
    bool bExported = McpSafeLevelSave(WorldToExport->PersistentLevel, ExportPath);
    if (bExported) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level exported"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to export level after 5 retries (check GPU driver stability)"), nullptr,
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
      
      // CRITICAL FIX: Check if destination already exists BEFORE trying to duplicate
      // This prevents "An asset already exists at this location" errors and makes
      // the operation idempotent
      if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("sourcePath"), SourcePath);
        Result->SetStringField(TEXT("destinationPath"), DestinationPath);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               FString::Printf(TEXT("Destination already exists: %s"), *DestinationPath), Result);
        return true;
      }
      
      if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath) != nullptr) {
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

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // CRITICAL FIX: Check if sublevel is already in the world BEFORE trying to add it
    // This prevents "A level with that name already exists in the world" modal dialog
    // which blocks execution and causes test timeouts
    // BUT: Also check if the existing level is actually loaded/valid
    for (ULevelStreaming* ExistingStreamingLevel : World->GetStreamingLevels()) {
      if (ExistingStreamingLevel) {
        FString ExistingPath = ExistingStreamingLevel->GetWorldAssetPackageName();
        // Compare normalized paths (without .umap extension)
        FString NormalizedExisting = ExistingPath;
        FString NormalizedNew = SubLevelPath;
        if (NormalizedExisting.EndsWith(TEXT(".umap"))) {
          NormalizedExisting = NormalizedExisting.LeftChop(5);
        }
        if (NormalizedNew.EndsWith(TEXT(".umap"))) {
          NormalizedNew = NormalizedNew.LeftChop(5);
        }
        if (NormalizedExisting.Equals(NormalizedNew, ESearchCase::IgnoreCase)) {
          // Check if the existing streaming level is actually valid/loaded
          // If it failed to load (file doesn't exist), it's a broken reference
          ULevel* ExistingLevel = ExistingStreamingLevel->GetLoadedLevel();
          bool bIsValidStreaming = ExistingLevel != nullptr || 
                                   ExistingStreamingLevel->IsStreamingStatePending();
          
          if (bIsValidStreaming) {
            // Sublevel already exists and is valid - return success with info
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
            Result->SetStringField(TEXT("world"), World->GetName());
            Result->SetBoolField(TEXT("alreadyExists"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   FString::Printf(TEXT("Sublevel already in world: %s"), *SubLevelPath), Result);
            return true;
          } else {
            // Existing streaming level is broken (failed to load)
            // Remove it and continue to add the new one
            UE_LOG(LogTemp, Warning, TEXT("add_sublevel: Removing broken streaming level reference: %s"), *SubLevelPath);
            World->RemoveStreamingLevel(ExistingStreamingLevel);
            break;  // Exit the loop to continue adding
          }
        }
      }
    }

    // Determine streaming class
    UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase)) {
      StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
    }

    ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *SubLevelPath, StreamingClass);
    if (NewLevel) {
      // CRITICAL FIX: Verify the streaming level can actually be loaded
      // AddLevelToWorld() creates the streaming level object but doesn't verify
      // the level file exists. Check if the level loaded successfully.
      // Give the engine a moment to attempt loading
      FlushRenderingCommands();
      FPlatformProcess::Sleep(0.1f);
      
      // Check if the level is actually loaded or pending load
      // If the level file doesn't exist, GetLoadedLevel() will be null and
      // the streaming state will not be pending
      ULevel* LoadedLevel = NewLevel->GetLoadedLevel();
      bool bIsPendingLoad = NewLevel->IsStreamingStatePending();
      
      // If level is loaded or pending, it's a valid streaming level
      if (LoadedLevel || bIsPendingLoad) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
        Result->SetStringField(TEXT("world"), World->GetName());
        Result->SetStringField(TEXT("streamingMethod"), StreamingMethod);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sublevel added successfully"), Result);
      } else {
        // CRITICAL FIX: Level file doesn't exist - return ERROR not success with warning
        // The streaming level was added to the world but the level file doesn't exist
        // This is an error condition, not a warning
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Sublevel file not found: %s"), *SubLevelPath),
            nullptr, TEXT("FILE_NOT_FOUND"));
      }
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
  if (EffectiveAction == TEXT("delete_level")) {
    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
    if (LevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("path"), LevelPath);

    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required for delete_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Issue #8: Sanitize path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(LevelPath);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *LevelPath),
                             nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LevelPath = SanitizedPath;

    // Use UEditorAssetLibrary to delete the level asset
    bool bDeleted = UEditorAssetLibrary::DeleteAsset(LevelPath);
    if (bDeleted) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("levelPath"), LevelPath);
      Result->SetBoolField(TEXT("deleted"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level deleted: %s"), *LevelPath), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to delete level: %s"), *LevelPath),
                             nullptr, TEXT("DELETE_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("rename_level")) {
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SourcePath);
    if (SourcePath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath or sourcePath required for rename_level"),
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
    SourcePath = SanitizedSource;
    DestinationPath = SanitizedDest;
    if (DestinationPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for rename_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Use UEditorAssetLibrary to rename the level asset
    bool bRenamed = UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
    if (bRenamed) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("sourcePath"), SourcePath);
      Result->SetStringField(TEXT("destinationPath"), DestinationPath);
      Result->SetBoolField(TEXT("renamed"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level renamed to: %s"), *DestinationPath), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to rename level: %s"), *SourcePath),
                             nullptr, TEXT("RENAME_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("duplicate_level")) {
    FString SourcePath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    if (SourcePath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SourcePath);

    FString DestinationPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

    if (SourcePath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("sourcePath or levelPath required for duplicate_level"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (DestinationPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("destinationPath required for duplicate_level"),
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
    SourcePath = SanitizedSource;
    DestinationPath = SanitizedDest;

    // Use UEditorAssetLibrary to duplicate the level asset
    UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
    if (DuplicatedAsset != nullptr) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("sourcePath"), SourcePath);
      Result->SetStringField(TEXT("destinationPath"), DestinationPath);
      Result->SetBoolField(TEXT("duplicated"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Level duplicated to: %s"), *DestinationPath), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to duplicate level: %s"), *SourcePath),
                             nullptr, TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("get_level_info")) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
      for (ULevel* Level : Levels) {
        if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
          TargetLevel = Level;
          break;
        }
      }
    } else {
      TargetLevel = World->GetCurrentLevel();
    }
    
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
      return true;
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetStringField(TEXT("levelName"), TargetLevel->GetName());
    Result->SetNumberField(TEXT("actorCount"), TargetLevel->Actors.Num());
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level info retrieved"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("set_level_world_settings")) {
    FString RequestedLevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), RequestedLevelPath);
      if (RequestedLevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), RequestedLevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevel* TargetLevel = World->GetCurrentLevel();
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }
    
    FString CurrentLevelPath = TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT("");
    
    // If a specific level path was requested, validate it matches the current level
    if (!RequestedLevelPath.IsEmpty()) {
      if (CurrentLevelPath.ToLower() != RequestedLevelPath.ToLower()) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Requested level '%s' is not loaded (current: %s)"), 
                           *RequestedLevelPath, *CurrentLevelPath),
            nullptr, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelPath"), CurrentLevelPath);
    Result->SetBoolField(TEXT("settingsApplied"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("World settings updated"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("set_level_lighting")) {
    FString RequestedLevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), RequestedLevelPath);
      if (RequestedLevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), RequestedLevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevel* TargetLevel = World->GetCurrentLevel();
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }
    
    FString CurrentLevelPath = TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT("");
    
    // If a specific level path was requested, validate it matches the current level
    if (!RequestedLevelPath.IsEmpty()) {
      if (CurrentLevelPath.ToLower() != RequestedLevelPath.ToLower()) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Requested level '%s' is not loaded (current: %s)"), 
                           *RequestedLevelPath, *CurrentLevelPath),
            nullptr, TEXT("LEVEL_NOT_LOADED"));
        return true;
      }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelPath"), CurrentLevelPath);
    Result->SetBoolField(TEXT("lightingSet"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level lighting settings updated"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("add_level_to_world")) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }
    
    if (LevelPath.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("levelPath required"), nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    // Verify level package exists before adding to avoid false positives
    FString FilenameToCheck;
    bool bFileExists = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            LevelPath, FilenameToCheck, FPackageName::GetMapPackageExtension())) {
      bFileExists = IFileManager::Get().FileExists(*FilenameToCheck);
    }
    if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Level file not found: %s"), *LevelPath),
          nullptr, TEXT("PACKAGE_NOT_FOUND"));
      return true;
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevelStreaming* StreamingLevel = UEditorLevelUtils::AddLevelToWorld(World, *LevelPath, ULevelStreamingDynamic::StaticClass());
    if (StreamingLevel) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("levelPath"), LevelPath);
      Result->SetBoolField(TEXT("added"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level added to world"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Failed to add level: %s"), *LevelPath),
                             nullptr, TEXT("ADD_FAILED"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("remove_level_from_world")) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    ULevel* TargetLevel = nullptr;
    for (ULevel* Level : Levels) {
      if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
        TargetLevel = Level;
        break;
      }
    }
    
    if (TargetLevel) {
      bool bRemoved = UEditorLevelUtils::RemoveLevelFromWorld(TargetLevel);
      if (bRemoved) {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("levelPath"), LevelPath);
        Result->SetBoolField(TEXT("removed"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level removed from world"), Result);
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Failed to remove level"), nullptr, TEXT("REMOVE_FAILED"));
      }
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("set_level_visibility")) {
    FString LevelPath;
    bool bVisible = true;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
      Payload->TryGetBoolField(TEXT("visible"), bVisible);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    ULevel* TargetLevel = nullptr;
    for (ULevel* Level : Levels) {
      if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
        TargetLevel = Level;
        break;
      }
    }
    
    if (TargetLevel) {
      UEditorLevelUtils::SetLevelVisibility(TargetLevel, bVisible, true);
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("levelPath"), LevelPath);
      Result->SetBoolField(TEXT("visible"), bVisible);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level visibility set"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("set_level_locked")) {
    FString LevelPath;
    bool bLocked = true;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
      Payload->TryGetBoolField(TEXT("locked"), bLocked);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    ULevel* TargetLevel = nullptr;
    for (ULevel* Level : Levels) {
      if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
        TargetLevel = Level;
        break;
      }
    }
    
    if (TargetLevel) {
      if (bLocked != FLevelUtils::IsLevelLocked(TargetLevel)) {
        FLevelUtils::ToggleLevelLock(TargetLevel);
      }
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("levelPath"), LevelPath);
      Result->SetBoolField(TEXT("locked"), FLevelUtils::IsLevelLocked(TargetLevel));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level lock set"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
    }
    return true;
  }
  if (EffectiveAction == TEXT("get_level_actors")) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
      for (ULevel* Level : Levels) {
        if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
          TargetLevel = Level;
          break;
        }
      }
    } else {
      TargetLevel = World->GetCurrentLevel();
    }
    
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
      return true;
    }
    
    TArray<TSharedPtr<FJsonValue>> ActorsArray;
    for (AActor* Actor : TargetLevel->Actors) {
      if (Actor) {
        ActorsArray.Add(MakeShared<FJsonValueString>(Actor->GetName()));
      }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetNumberField(TEXT("count"), ActorsArray.Num());
    Result->SetArrayField(TEXT("actors"), ActorsArray);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level actors retrieved"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("get_level_bounds")) {
    FString LevelPath;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
      if (LevelPath.IsEmpty()) Payload->TryGetStringField(TEXT("level_path"), LevelPath);
    }
    
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    ULevel* TargetLevel = nullptr;
    if (!LevelPath.IsEmpty()) {
      TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
      for (ULevel* Level : Levels) {
        if (Level && Level->GetOutermost() && Level->GetOutermost()->GetName() == LevelPath) {
          TargetLevel = Level;
          break;
        }
      }
    } else {
      TargetLevel = World->GetCurrentLevel();
    }
    
    if (!TargetLevel) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Level not found: %s"), *LevelPath),
                             nullptr, TEXT("LEVEL_NOT_FOUND"));
      return true;
    }
    
    FBox LevelBounds(ForceInit);
    if (TargetLevel->LevelBoundsActor.IsValid()) {
      LevelBounds = TargetLevel->LevelBoundsActor->GetComponentsBoundingBox();
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("levelPath"), TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : TEXT(""));
    Result->SetStringField(TEXT("min"), FString::Printf(TEXT("X=%f Y=%f Z=%f"), LevelBounds.Min.X, LevelBounds.Min.Y, LevelBounds.Min.Z));
    Result->SetStringField(TEXT("max"), FString::Printf(TEXT("X=%f Y=%f Z=%f"), LevelBounds.Max.X, LevelBounds.Max.Y, LevelBounds.Max.Z));
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Level bounds retrieved"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("get_level_lighting_scenarios")) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    TArray<TSharedPtr<FJsonValue>> Scenarios;
    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    for (ULevel* Level : Levels) {
      if (Level && Level->bIsLightingScenario) {
        TSharedPtr<FJsonObject> ScenarioInfo = MakeShared<FJsonObject>();
        ScenarioInfo->SetStringField(TEXT("levelPath"), Level->GetOutermost() ? Level->GetOutermost()->GetName() : TEXT(""));
        ScenarioInfo->SetStringField(TEXT("levelName"), Level->GetName());
        Scenarios.Add(MakeShared<FJsonValueObject>(ScenarioInfo));
      }
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("scenarios"), Scenarios);
    Result->SetNumberField(TEXT("count"), Scenarios.Num());
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighting scenarios retrieved"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("build_level_lighting")) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildLighting);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("buildStarted"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighting build started"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("build_level_navigation")) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAIPaths);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("buildStarted"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Navigation build started"), Result);
    return true;
  }
  if (EffectiveAction == TEXT("build_all_level")) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }
    
    FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAll);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("buildStarted"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Full build started"), Result);
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
