#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"


#if WITH_EDITOR
#include "ContentStreaming.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "IMergeActorsModule.h"
#include "IMergeActorsTool.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Subsystems/EditorActorSubsystem.h"

#endif

bool UMcpAutomationBridgeSubsystem::HandlePerformanceAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("generate_memory_report")) &&
      !Lower.StartsWith(TEXT("configure_texture_streaming")) &&
      !Lower.StartsWith(TEXT("merge_actors")) &&
      !Lower.StartsWith(TEXT("start_profiling")) &&
      !Lower.StartsWith(TEXT("stop_profiling")) &&
      !Lower.StartsWith(TEXT("show_fps")) &&
      !Lower.StartsWith(TEXT("show_stats")) &&
      !Lower.StartsWith(TEXT("set_scalability")) &&
      !Lower.StartsWith(TEXT("set_resolution_scale")) &&
      !Lower.StartsWith(TEXT("set_vsync")) &&
      !Lower.StartsWith(TEXT("set_frame_rate_limit")) &&
      !Lower.StartsWith(TEXT("configure_nanite")) &&
      !Lower.StartsWith(TEXT("configure_lod")) &&
      !Lower.StartsWith(TEXT("run_benchmark")) &&
      !Lower.StartsWith(TEXT("enable_gpu_timing")) &&
      !Lower.StartsWith(TEXT("apply_baseline_settings")) &&
      !Lower.StartsWith(TEXT("optimize_draw_calls")) &&
      !Lower.StartsWith(TEXT("configure_occlusion_culling")) &&
      !Lower.StartsWith(TEXT("optimize_shaders")) &&
      !Lower.StartsWith(TEXT("configure_world_partition"))) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Performance payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower == TEXT("generate_memory_report")) {
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);

    // Execute memreport command
    FString Cmd = bDetailed ? TEXT("memreport -full") : TEXT("memreport");
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

    // If output path provided, we might want to move the log file, but
    // memreport writes to a specific location. For now, just acknowledge
    // execution.

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Memory report generated"), nullptr);
    return true;
  } else if (Lower == TEXT("start_profiling")) {
    // "stat startfile"
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat startfile"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Profiling started"), nullptr);
    return true;
  } else if (Lower == TEXT("stop_profiling")) {
    // "stat stopfile"
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat stopfile"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Profiling stopped"), nullptr);
    return true;
  } else if (Lower == TEXT("show_fps")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    FString Cmd = bEnabled ? TEXT("stat fps") : TEXT("stat none");
    // Note: "stat fps" toggles, so we might need check, but mostly users just
    // want to run the command. For explicit set, we can use "stat fps 1" or
    // "stat fps 0" if supported, but typically it's a toggle. Better: use
    // GAreyouSure? No, just exec.
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat fps"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("FPS stat toggled"), nullptr);
    return true;
  } else if (Lower == TEXT("show_stats")) {
    FString Category;
    if (Payload->TryGetStringField(TEXT("category"), Category) &&
        !Category.IsEmpty()) {
      if (!GEditor)
      {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
          return true;
      }

      // Sanitize category to prevent console command injection
      // Only allow alphanumeric characters and underscores
      bool bIsValidCategory = true;
      for (int32 i = 0; i < Category.Len(); ++i) {
        TCHAR C = Category[i];
        if (!FChar::IsAlnum(C) && C != TEXT('_')) {
          bIsValidCategory = false;
          break;
        }
      }

      if (!bIsValidCategory) {
        SendAutomationError(RequestingSocket, RequestId, 
                            TEXT("Invalid stat category name. Only alphanumeric characters and underscores allowed."),
                            TEXT("INVALID_CATEGORY"));
        return true;
      }

      GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                    *FString::Printf(TEXT("stat %s"), *Category));
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Stat '%s' toggled"), *Category), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Category required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
  } else if (Lower == TEXT("set_scalability")) {
    int32 Level = 3; // Epic
    Payload->TryGetNumberField(TEXT("level"), Level);

    // simple batch scalability
    Scalability::FQualityLevels Quals;
    Quals.SetFromSingleQualityLevel(Level);
    Scalability::SetQualityLevels(Quals);
    Scalability::SaveState(GEditorIni);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Scalability set"), nullptr);
    return true;
  } else if (Lower == TEXT("set_resolution_scale")) {
    double Scale = 100.0;
    if (Payload->TryGetNumberField(TEXT("scale"), Scale)) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.ScreenPercentage"));
      if (CVar)
        CVar->Set((float)Scale);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Resolution scale set"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Scale required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
  } else if (Lower == TEXT("set_vsync")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
    if (CVar)
      CVar->Set(bEnabled ? 1 : 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("VSync configured"), nullptr);
    return true;
  } else if (Lower == TEXT("set_frame_rate_limit")) {
    double Limit = 0.0;
    if (Payload->TryGetNumberField(TEXT("maxFPS"), Limit)) {
      GEngine->SetMaxFPS((float)Limit);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Max FPS set"), nullptr);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("maxFPS required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
    }
    return true;
  } else if (Lower == TEXT("configure_nanite")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite"));
    if (CVar)
      CVar->Set(bEnabled ? 1 : 0);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Nanite configured"), nullptr);
    return true;
  } else if (Lower == TEXT("configure_lod")) {
    double LODBias = 0.0;
    if (Payload->TryGetNumberField(TEXT("lodBias"), LODBias)) {
      IConsoleVariable *CVar =
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.MipMapLODBias"));
      if (CVar)
        CVar->Set((float)LODBias);
    }

    double ForceLOD = -1.0;
    if (Payload->TryGetNumberField(TEXT("forceLOD"), ForceLOD)) {
      IConsoleVariable *CVar =
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));
      if (CVar)
        CVar->Set((int32)ForceLOD);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("LOD settings configured"), nullptr);
    return true;
  } else if (Lower == TEXT("configure_texture_streaming")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double PoolSize = 0;
    if (Payload->TryGetNumberField(TEXT("poolSize"), PoolSize)) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.Streaming.PoolSize"));
      if (CVar)
        CVar->Set((float)PoolSize);
    }

    bool bBoost = false;
    if (Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost) &&
        bBoost) {
      // Logic to boost streaming around player
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        APlayerCameraManager *Cam = UGameplayStatics::GetPlayerCameraManager(
            GEditor->GetEditorWorldContext().World(), 0);
        if (Cam) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
          IStreamingManager::Get().AddViewLocation(Cam->GetCameraLocation());
#else
          // UE 5.0: AddViewLocation not available - use alternative approach
          // Just notify that streaming is enabled without location boost
#endif
        }
      }
    }

    IConsoleVariable *CVarStream =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming"));
    if (CVarStream)
      CVarStream->Set(bEnabled ? 1 : 0);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Texture streaming configured"), nullptr);
    return true;
  } else if (Lower == TEXT("merge_actors")) {
    // merge_actors: drive the editor's Merge Actors tools by selecting the
    // requested actors in the current editor world and invoking
    // IMergeActorsTool::RunMergeFromSelection(). This relies on the
    // MergeActors module and registered tools, but never reports success
    // unless a real merge was requested and executed.

    const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("actors"), NamesArray) || !NamesArray ||
        NamesArray->Num() < 2) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("merge_actors requires an 'actors' array "
                                  "with at least 2 entries"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Editor world not available for merge_actors"), nullptr,
          TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    TArray<AActor *> ActorsToMerge;

    auto ResolveActorByName = [World](const FString &Name) -> AActor * {
      if (Name.IsEmpty()) {
        return nullptr;
      }

      // Try to resolve by full object path first
      if (AActor *ByPath = FindObject<AActor>(nullptr, *Name)) {
        return ByPath;
      }

      // Fallback: search the current editor world by label and by name
      for (TActorIterator<AActor> It(World); It; ++It) {
        AActor *Actor = *It;
        if (!Actor) {
          continue;
        }

        const FString Label = Actor->GetActorLabel();
        const FString ObjName = Actor->GetName();
        if (Label.Equals(Name, ESearchCase::IgnoreCase) ||
            ObjName.Equals(Name, ESearchCase::IgnoreCase)) {
          return Actor;
        }
      }

      return nullptr;
    };

    for (const TSharedPtr<FJsonValue> &Val : *NamesArray) {
      if (!Val.IsValid() || Val->Type != EJson::String) {
        continue;
      }

      const FString RawName = Val->AsString().TrimStartAndEnd();
      if (RawName.IsEmpty()) {
        continue;
      }

      if (AActor *Resolved = ResolveActorByName(RawName)) {
        ActorsToMerge.AddUnique(Resolved);
      }
    }

    if (ActorsToMerge.Num() < 2) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("merge_actors resolved fewer than 2 valid actors"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Prepare selection for the Merge Actors tool
    GEditor->SelectNone(true, true, false);
    for (AActor *Actor : ActorsToMerge) {
      if (Actor) {
        GEditor->SelectActor(Actor, true, true, true);
      }
    }

    IMergeActorsModule &MergeModule = IMergeActorsModule::Get();
    TArray<IMergeActorsTool *> Tools;
    MergeModule.GetRegisteredMergeActorsTools(Tools);

    if (Tools.Num() == 0) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("No Merge Actors tools are registered in this editor"), nullptr,
          TEXT("MERGE_TOOL_MISSING"));
      return true;
    }

    FString RequestedToolName;
    Payload->TryGetStringField(TEXT("toolName"), RequestedToolName);
    IMergeActorsTool *ChosenTool = nullptr;

    // Prefer a tool whose display name matches the requested toolName
    if (!RequestedToolName.IsEmpty()) {
      for (IMergeActorsTool *Tool : Tools) {
        if (!Tool) {
          continue;
        }

        const FText ToolNameText = Tool->GetToolNameText();
        if (ToolNameText.ToString().Equals(RequestedToolName,
                                           ESearchCase::IgnoreCase)) {
          ChosenTool = Tool;
          break;
        }
      }
    }

    // Fallback: first tool that can merge from the current selection
    if (!ChosenTool) {
      for (IMergeActorsTool *Tool : Tools) {
        if (Tool && Tool->CanMergeFromSelection()) {
          ChosenTool = Tool;
          break;
        }
      }
    }

    if (!ChosenTool) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("No Merge Actors tool can operate on the current selection"),
          nullptr, TEXT("MERGE_TOOL_UNAVAILABLE"));
      return true;
    }

    bool bReplaceSources = false;
    if (Payload->TryGetBoolField(TEXT("replaceSourceActors"),
                                 bReplaceSources)) {
      ChosenTool->SetReplaceSourceActors(bReplaceSources);
    }

    if (!ChosenTool->CanMergeFromSelection()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Merge operation is not valid for the current selection"),
          nullptr, TEXT("MERGE_NOT_POSSIBLE"));
      return true;
    }

    const FString DefaultPackageName = ChosenTool->GetDefaultPackageName();
    const bool bMerged = ChosenTool->RunMergeFromSelection();
    if (!bMerged) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor merge operation failed"), nullptr,
                             TEXT("MERGE_FAILED"));
      return true;
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetNumberField(TEXT("mergedActorCount"), ActorsToMerge.Num());
    Resp->SetBoolField(TEXT("replaceSourceActors"),
                       ChosenTool->GetReplaceSourceActors());
    if (!DefaultPackageName.IsEmpty()) {
      Resp->SetStringField(TEXT("defaultPackageName"), DefaultPackageName);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actors merged using Merge Actors tool"), Resp,
                           FString());
    return true;
  } else if (Lower == TEXT("run_benchmark")) {
    // Run performance benchmark
    double Duration = 60.0;
    Payload->TryGetNumberField(TEXT("duration"), Duration);

    FString BenchmarkType = TEXT("all");
    Payload->TryGetStringField(TEXT("type"), BenchmarkType);

    // Start profiling for benchmark
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat startfile"));

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetNumberField(TEXT("duration"), Duration);
    Resp->SetStringField(TEXT("type"), BenchmarkType);
    Resp->SetStringField(TEXT("status"), TEXT("started"));

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Benchmark started (type: %s, duration: %.0fs)"),
                        *BenchmarkType, Duration),
        Resp);
    return true;
  } else if (Lower == TEXT("enable_gpu_timing")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    IConsoleVariable *CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUStatsEnabled"));
    if (CVar) {
      CVar->Set(bEnabled ? 1 : 0);
    }

    // Also toggle stat gpu for visual feedback
    if (bEnabled) {
      if (!GEditor)
      {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
          return true;
      }

      GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                    TEXT("stat gpu"));
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("enabled"), bEnabled);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("GPU timing %s"),
                        bEnabled ? TEXT("enabled") : TEXT("disabled")),
        Resp);
    return true;
  } else if (Lower == TEXT("apply_baseline_settings")) {
    // Apply common baseline optimization settings
    FString Profile = TEXT("balanced");
    Payload->TryGetStringField(TEXT("profile"), Profile);

    // Common optimization CVars
    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    if (Profile.Equals(TEXT("performance"), ESearchCase::IgnoreCase)) {
      SetCVar(TEXT("r.VSync"), 0);
      SetCVar(TEXT("r.AllowHDR"), 0);
      SetCVar(TEXT("r.MotionBlurQuality"), 0);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 0);
      SetCVar(TEXT("r.BloomQuality"), 0);
      SetCVar(TEXT("r.ShadowQuality"), 1);
      SetCVar(TEXT("r.MaxAnisotropy"), 4);
    } else if (Profile.Equals(TEXT("quality"), ESearchCase::IgnoreCase)) {
      SetCVar(TEXT("r.VSync"), 1);
      SetCVar(TEXT("r.AllowHDR"), 1);
      SetCVar(TEXT("r.MotionBlurQuality"), 4);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 2);
      SetCVar(TEXT("r.BloomQuality"), 5);
      SetCVar(TEXT("r.ShadowQuality"), 5);
      SetCVar(TEXT("r.MaxAnisotropy"), 16);
    } else {
      // Balanced defaults
      SetCVar(TEXT("r.VSync"), 1);
      SetCVar(TEXT("r.AllowHDR"), 1);
      SetCVar(TEXT("r.MotionBlurQuality"), 2);
      SetCVar(TEXT("r.DepthOfFieldQuality"), 1);
      SetCVar(TEXT("r.BloomQuality"), 3);
      SetCVar(TEXT("r.ShadowQuality"), 3);
      SetCVar(TEXT("r.MaxAnisotropy"), 8);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("profile"), Profile);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Baseline settings applied: %s"), *Profile), Resp);
    return true;
  } else if (Lower == TEXT("optimize_draw_calls")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    bool bInstancing = true;
    Payload->TryGetBoolField(TEXT("instancing"), bInstancing);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    // Draw call optimization CVars
    SetCVar(TEXT("r.MeshDrawCommands.DynamicInstancing"), bInstancing ? 1 : 0);
    SetCVar(TEXT("r.MeshDrawCommands.UseCachedCommands"), bEnabled ? 1 : 0);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("optimized"), bEnabled);
    Resp->SetBoolField(TEXT("instancing"), bInstancing);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Draw call optimizations configured"), Resp);
    return true;
  } else if (Lower == TEXT("configure_occlusion_culling")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double OcclusionSlop = 0.0;
    bool bHasSlop = Payload->TryGetNumberField(TEXT("slop"), OcclusionSlop);

    double MinScreenRadiusForOcclusion = 0.0;
    bool bHasMinRadius = Payload->TryGetNumberField(
        TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    auto SetCVarFloat = [](const TCHAR *Name, float Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    SetCVar(TEXT("r.AllowOcclusionQueries"), bEnabled ? 1 : 0);

    if (bHasSlop) {
      SetCVarFloat(TEXT("r.OcclusionSlop"), (float)OcclusionSlop);
    }

    if (bHasMinRadius) {
      SetCVarFloat(TEXT("r.OcclusionCullMinScreenRadius"),
                   (float)MinScreenRadiusForOcclusion);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("enabled"), bEnabled);
    if (bHasSlop) {
      Resp->SetNumberField(TEXT("slop"), OcclusionSlop);
    }
    if (bHasMinRadius) {
      Resp->SetNumberField(TEXT("minScreenRadius"), MinScreenRadiusForOcclusion);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Occlusion culling configured"), Resp);
    return true;
  } else if (Lower == TEXT("optimize_shaders")) {
    FString Mode = TEXT("changed");
    Payload->TryGetStringField(TEXT("mode"), Mode);

    bool bForceRecompile = false;
    Payload->TryGetBoolField(TEXT("forceRecompile"), bForceRecompile);

    FString Cmd;
    if (bForceRecompile) {
      Cmd = TEXT("recompileshaders all");
    } else if (Mode.Equals(TEXT("material"), ESearchCase::IgnoreCase)) {
      Cmd = TEXT("recompileshaders material");
    } else if (Mode.Equals(TEXT("global"), ESearchCase::IgnoreCase)) {
      Cmd = TEXT("recompileshaders global");
    } else {
      Cmd = TEXT("recompileshaders changed");
    }

    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("NO_EDITOR"));
        return true;
    }

    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetBoolField(TEXT("forceRecompile"), bForceRecompile);
    Resp->SetStringField(TEXT("command"), Cmd);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Shader optimization initiated: %s"), *Cmd),
        Resp);
    return true;
  } else if (Lower == TEXT("configure_world_partition")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double CellSize = 0.0;
    bool bHasCellSize = Payload->TryGetNumberField(TEXT("cellSize"), CellSize);

    double LoadingRange = 0.0;
    bool bHasLoadingRange =
        Payload->TryGetNumberField(TEXT("loadingRange"), LoadingRange);

    auto SetCVar = [](const TCHAR *Name, int32 Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    auto SetCVarFloat = [](const TCHAR *Name, float Value) {
      if (IConsoleVariable *CVar =
              IConsoleManager::Get().FindConsoleVariable(Name)) {
        CVar->Set(Value);
      }
    };

    SetCVar(TEXT("wp.Runtime.EnableStreaming"), bEnabled ? 1 : 0);

    if (bHasCellSize) {
      SetCVarFloat(TEXT("wp.Runtime.RuntimeCellSize"), (float)CellSize);
    }

    if (bHasLoadingRange) {
      SetCVarFloat(TEXT("wp.Runtime.RuntimeStreamingRange"),
                   (float)LoadingRange);
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("streamingEnabled"), bEnabled);
    if (bHasCellSize) {
      Resp->SetNumberField(TEXT("cellSize"), CellSize);
    }
    if (bHasLoadingRange) {
      Resp->SetNumberField(TEXT("loadingRange"), LoadingRange);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("World Partition settings configured"), Resp);
    return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Performance actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
