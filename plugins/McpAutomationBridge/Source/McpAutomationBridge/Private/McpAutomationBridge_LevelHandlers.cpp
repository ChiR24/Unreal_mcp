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
#endif

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
      FString FilenameToCheck;
      bool bFileExists = false;
      if (FPackageName::TryConvertLongPackageNameToFilename(
              LevelPath, FilenameToCheck, FPackageName::GetMapPackageExtension())) {
        bFileExists = IFileManager::Get().FileExists(*FilenameToCheck);
      }
      // Also check if it's a valid package path
      if (!bFileExists && !FPackageName::DoesPackageExist(LevelPath)) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found: %s"), *LevelPath),
            nullptr, TEXT("FILE_NOT_FOUND"));
        return true;
      }

      // Force any pending work to complete
      FlushRenderingCommands();

      // LoadMap prompts for save if dirty. To avoid blocking automation, we
      // should carefuly consider. But for now, we assume user wants standard
      // behavior or has saved. There isn't a simple "Force Load" via FileUtils
      // without clearing dirty flags manually. We will proceed with LoadMap.
      const bool bLoaded = FEditorFileUtils::LoadMap(FileToLoad);

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

#if WITH_EDITOR
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

    bool bSaved = FEditorFileUtils::SaveCurrentLevel();
    if (bSaved) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("levelPath"), World->GetOutermost()->GetName());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Level saved"), Resp, FString());
    } else {
      // Provide detailed error information
      TSharedPtr<FJsonObject> ErrorDetail = MakeShared<FJsonObject>();
      FString PackageName = World->GetOutermost()->GetName();
      ErrorDetail->SetStringField(TEXT("attemptedPath"), PackageName);

      FString Filename;
      FString ErrorReason = TEXT("Unknown save failure");

      if (PackageName.Contains(TEXT("Untitled")) ||
          PackageName.StartsWith(TEXT("/Temp/"))) {
        ErrorReason = TEXT(
            "Level is unsaved/temporary. Use save_level_as with a path first.");
        ErrorDetail->SetStringField(
            TEXT("hint"),
            TEXT(
                "Use manage_level with action='save_as' and provide savePath"));
      } else if (FPackageName::TryConvertLongPackageNameToFilename(
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

#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LevelEditorSS =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
      bool bSaved = false;
#if __has_include("FileHelpers.h")
      if (UWorld *World = GEditor->GetEditorWorldContext().World()) {
        bSaved = FEditorFileUtils::SaveMap(World, SavePath);
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

    // FEditorFileUtils::ExportMap(WorldToExport, ExportPath); // Legacy/Removed
    // Use SaveMap for .umap or FEditorFileUtils::SaveLevel
    FEditorFileUtils::SaveMap(WorldToExport, ExportPath);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Level exported"), nullptr);
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

    // Determine streaming class
    UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase)) {
      StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
    }

    ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *SubLevelPath, StreamingClass);
    if (NewLevel) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
      Result->SetStringField(TEXT("world"), World->GetName());
      Result->SetStringField(TEXT("streamingMethod"), StreamingMethod);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sublevel added successfully"), Result);
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
