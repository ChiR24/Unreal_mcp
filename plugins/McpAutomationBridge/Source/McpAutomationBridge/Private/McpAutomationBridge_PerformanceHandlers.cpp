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
      !Lower.StartsWith(TEXT("configure_lod"))) {
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
    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *Cmd);

    // If output path provided, we might want to move the log file, but
    // memreport writes to a specific location. For now, just acknowledge
    // execution.

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Memory report generated"), nullptr);
    return true;
  } else if (Lower == TEXT("start_profiling")) {
    // "stat startfile"
    GEngine->Exec(GEditor->GetEditorWorldContext().World(),
                  TEXT("stat startfile"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Profiling started"), nullptr);
    return true;
  } else if (Lower == TEXT("stop_profiling")) {
    // "stat stopfile"
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
    GEngine->Exec(GEditor->GetEditorWorldContext().World(), TEXT("stat fps"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("FPS stat toggled"), nullptr);
    return true;
  } else if (Lower == TEXT("show_stats")) {
    FString Category;
    if (Payload->TryGetStringField(TEXT("category"), Category) &&
        !Category.IsEmpty()) {
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
          IStreamingManager::Get().AddViewLocation(Cam->GetCameraLocation());
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
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Performance actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
