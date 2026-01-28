#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#endif
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GeneralProjectSettings.h"
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"

#endif

bool UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("build_environment"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("build_environment")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("build_environment payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

  // Fast-path foliage sub-actions to dedicated native handlers to avoid double
  // responses
  if (LowerSub == TEXT("add_foliage_instances")) {
    // Transform from build_environment schema to foliage handler schema
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    const TArray<TSharedPtr<FJsonValue>> *Transforms = nullptr;
    Payload->TryGetArrayField(TEXT("transforms"), Transforms);
    TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
    if (!FoliageTypePath.IsEmpty()) {
      FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }
    TArray<TSharedPtr<FJsonValue>> Locations;
    if (Transforms) {
      for (const TSharedPtr<FJsonValue> &V : *Transforms) {
        if (!V.IsValid() || V->Type != EJson::Object)
          continue;
        const TSharedPtr<FJsonObject> *TObj = nullptr;
        if (!V->TryGetObject(TObj) || !TObj)
          continue;
        const TSharedPtr<FJsonObject> *LocObj = nullptr;
        if (!(*TObj)->TryGetObjectField(TEXT("location"), LocObj) || !LocObj)
          continue;
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>();
        L->SetNumberField(TEXT("x"), X);
        L->SetNumberField(TEXT("y"), Y);
        L->SetNumberField(TEXT("z"), Z);
        Locations.Add(MakeShared<FJsonValueObject>(L));
      }
    }
    FoliagePayload->SetArrayField(TEXT("locations"), Locations);
    return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), FoliagePayload,
                              RequestingSocket);
  } else if (LowerSub == TEXT("get_foliage_instances")) {
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
    if (!FoliageTypePath.IsEmpty()) {
      FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }
    return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"),
                                     FoliagePayload, RequestingSocket);
  } else if (LowerSub == TEXT("remove_foliage")) {
    FString FoliageTypePath;
    Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
    bool bRemoveAll = false;
    Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
    TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
    if (!FoliageTypePath.IsEmpty()) {
      FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath);
    }
    FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
    return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"),
                               FoliagePayload, RequestingSocket);
  }
  // Dispatch landscape operations
  else if (LowerSub == TEXT("paint_landscape") ||
           LowerSub == TEXT("paint_landscape_layer")) {
    return HandlePaintLandscapeLayer(RequestId, TEXT("paint_landscape_layer"),
                                     Payload, RequestingSocket);
  } else if (LowerSub == TEXT("sculpt_landscape")) {
    return HandleSculptLandscape(RequestId, TEXT("sculpt_landscape"), Payload,
                                 RequestingSocket);
  } else if (LowerSub == TEXT("modify_heightmap")) {
    return HandleModifyHeightmap(RequestId, TEXT("modify_heightmap"), Payload,
                                 RequestingSocket);
  } else if (LowerSub == TEXT("set_landscape_material")) {
    return HandleSetLandscapeMaterial(RequestId, TEXT("set_landscape_material"),
                                      Payload, RequestingSocket);
  } else if (LowerSub == TEXT("create_landscape_grass_type")) {
    return HandleCreateLandscapeGrassType(RequestId,
                                          TEXT("create_landscape_grass_type"),
                                          Payload, RequestingSocket);
  } else if (LowerSub == TEXT("generate_lods")) {
    return HandleGenerateLODs(RequestId, TEXT("generate_lods"), Payload,
                              RequestingSocket);
  } else if (LowerSub == TEXT("bake_lightmap")) {
    return HandleBakeLightmap(RequestId, TEXT("bake_lightmap"), Payload,
                              RequestingSocket);
  }

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message =
      FString::Printf(TEXT("Environment action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (LowerSub == TEXT("export_snapshot")) {
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("path required for export_snapshot");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
      Snapshot->SetStringField(TEXT("timestamp"),
                               FDateTime::UtcNow().ToString());
      Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

      FString JsonString;
      TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&JsonString);
      if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer)) {
        if (FFileHelper::SaveStringToFile(JsonString, *Path)) {
          Resp->SetStringField(TEXT("exportPath"), Path);
          Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
        } else {
          bSuccess = false;
          Message = TEXT("Failed to write snapshot file");
          ErrorCode = TEXT("WRITE_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        }
      } else {
        bSuccess = false;
        Message = TEXT("Failed to serialize snapshot");
        ErrorCode = TEXT("SERIALIZE_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  } else if (LowerSub == TEXT("import_snapshot")) {
    FString Path;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (Path.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("path required for import_snapshot");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      FString JsonString;
      if (!FFileHelper::LoadFileToString(JsonString, *Path)) {
        bSuccess = false;
        Message = TEXT("Failed to read snapshot file");
        ErrorCode = TEXT("LOAD_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        TSharedPtr<FJsonObject> SnapshotObj;
        TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, SnapshotObj) ||
            !SnapshotObj.IsValid()) {
          bSuccess = false;
          Message = TEXT("Failed to parse snapshot");
          ErrorCode = TEXT("PARSE_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Resp->SetObjectField(TEXT("snapshot"), SnapshotObj.ToSharedRef());
          Resp->SetStringField(TEXT("message"), TEXT("Snapshot imported"));
        }
      }
    }
  } else if (LowerSub == TEXT("delete")) {
    const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray) {
      bSuccess = false;
      Message = TEXT("names array required for delete");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else if (!GEditor) {
      bSuccess = false;
      Message = TEXT("Editor not available");
      ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (!ActorSS) {
        bSuccess = false;
        Message = TEXT("EditorActorSubsystem not available");
        ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        TArray<FString> Deleted;
        TArray<FString> Missing;
        for (const TSharedPtr<FJsonValue> &Val : *NamesArray) {
          if (Val.IsValid() && Val->Type == EJson::String) {
            FString Name = Val->AsString();
            TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
            bool bRemoved = false;
            for (AActor *A : AllActors) {
              if (A &&
                  A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) {
                if (ActorSS->DestroyActor(A)) {
                  Deleted.Add(Name);
                  bRemoved = true;
                }
                break;
              }
            }
            if (!bRemoved) {
              Missing.Add(Name);
            }
          }
        }

        TArray<TSharedPtr<FJsonValue>> DeletedArray;
        for (const FString &Name : Deleted) {
          DeletedArray.Add(MakeShared<FJsonValueString>(Name));
        }
        Resp->SetArrayField(TEXT("deleted"), DeletedArray);
        Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

        if (Missing.Num() > 0) {
          TArray<TSharedPtr<FJsonValue>> MissingArray;
          for (const FString &Name : Missing) {
            MissingArray.Add(MakeShared<FJsonValueString>(Name));
          }
          Resp->SetArrayField(TEXT("missing"), MissingArray);
          bSuccess = false;
          Message = TEXT("Some environment actors could not be removed");
          ErrorCode = TEXT("DELETE_PARTIAL");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          Message = TEXT("Environment actors deleted");
        }
      }
    }
  } else if (LowerSub == TEXT("create_sky_sphere")) {
    if (GEditor) {
      UClass *SkySphereClass = LoadClass<AActor>(
          nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/"
                        "SkySphere.SkySphere_C'"));
      if (SkySphereClass) {
        AActor *SkySphere = SpawnActorInActiveWorld<AActor>(
            SkySphereClass, FVector::ZeroVector, FRotator::ZeroRotator,
            TEXT("SkySphere"));
        if (SkySphere) {
          bSuccess = true;
          Message = TEXT("Sky sphere created");
          Resp->SetStringField(TEXT("actorName"), SkySphere->GetActorLabel());
        }
      }
    }
    if (!bSuccess) {
      bSuccess = false;
      Message = TEXT("Failed to create sky sphere");
      ErrorCode = TEXT("CREATION_FAILED");
    }
  } else if (LowerSub == TEXT("set_time_of_day")) {
    float TimeOfDay = 12.0f;
    Payload->TryGetNumberField(TEXT("time"), TimeOfDay);

    if (GEditor) {
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (ActorSS) {
        for (AActor *Actor : ActorSS->GetAllLevelActors()) {
          if (Actor->GetClass()->GetName().Contains(TEXT("SkySphere"))) {
            UFunction *SetTimeFunction =
                Actor->FindFunction(TEXT("SetTimeOfDay"));
            if (SetTimeFunction) {
              float TimeParam = TimeOfDay;
              Actor->ProcessEvent(SetTimeFunction, &TimeParam);
              bSuccess = true;
              Message =
                  FString::Printf(TEXT("Time of day set to %.2f"), TimeOfDay);
              break;
            }
          }
        }
      }
    }
    if (!bSuccess) {
      bSuccess = false;
      Message = TEXT("Sky sphere not found or time function not available");
      ErrorCode = TEXT("SET_TIME_FAILED");
    }
  } else if (LowerSub == TEXT("create_fog_volume")) {
    FVector Location(0, 0, 0);
    Payload->TryGetNumberField(TEXT("x"), Location.X);
    Payload->TryGetNumberField(TEXT("y"), Location.Y);
    Payload->TryGetNumberField(TEXT("z"), Location.Z);

    if (GEditor) {
      UClass *FogClass = LoadClass<AActor>(
          nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
      if (FogClass) {
        AActor *FogVolume = SpawnActorInActiveWorld<AActor>(
            FogClass, Location, FRotator::ZeroRotator, TEXT("FogVolume"));
        if (FogVolume) {
          bSuccess = true;
          Message = TEXT("Fog volume created");
          Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
        }
      }
    }
    if (!bSuccess) {
      bSuccess = false;
      Message = TEXT("Failed to create fog volume");
      ErrorCode = TEXT("CREATION_FAILED");
    }
  } else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Environment action '%s' not implemented"),
                              *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Environment building actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEnvironmentAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_environment"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_environment"))) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_environment payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  auto SendResult = [&](bool bSuccess, const TCHAR *Message,
                        const FString &ErrorCode,
                        const TSharedPtr<FJsonObject> &Result) {
    if (bSuccess) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             Message ? Message
                                     : TEXT("Environment control succeeded."),
                             Result, FString());
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             Message ? Message
                                     : TEXT("Environment control failed."),
                             Result, ErrorCode);
    }
  };

  UWorld *World = nullptr;
  if (GEditor) {
    World = GEditor->GetEditorWorldContext().World();
  }

  if (!World) {
    SendResult(false, TEXT("Editor world is unavailable"),
               TEXT("WORLD_NOT_AVAILABLE"), nullptr);
    return true;
  }

  auto FindFirstDirectionalLight = [&]() -> ADirectionalLight * {
    for (TActorIterator<ADirectionalLight> It(World); It; ++It) {
      if (ADirectionalLight *Light = *It) {
        if (IsValid(Light)) {
          return Light;
        }
      }
    }
    return nullptr;
  };

  auto FindFirstSkyLight = [&]() -> ASkyLight * {
    for (TActorIterator<ASkyLight> It(World); It; ++It) {
      if (ASkyLight *Sky = *It) {
        if (IsValid(Sky)) {
          return Sky;
        }
      }
    }
    return nullptr;
  };

  if (LowerSub == TEXT("set_time_of_day")) {
    double Hour = 0.0;
    const bool bHasHour = Payload->TryGetNumberField(TEXT("hour"), Hour);
    if (!bHasHour) {
      SendResult(false, TEXT("Missing hour parameter"),
                 TEXT("INVALID_ARGUMENT"), nullptr);
      return true;
    }

    ADirectionalLight *SunLight = FindFirstDirectionalLight();
    if (!SunLight) {
      SendResult(false, TEXT("No directional light found"),
                 TEXT("SUN_NOT_FOUND"), nullptr);
      return true;
    }

    const float ClampedHour =
        FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);
    const float SolarPitch = (ClampedHour / 24.0f) * 360.0f - 90.0f;

    SunLight->Modify();
    FRotator NewRotation = SunLight->GetActorRotation();
    NewRotation.Pitch = SolarPitch;
    SunLight->SetActorRotation(NewRotation);

    if (UDirectionalLightComponent *LightComp =
            Cast<UDirectionalLightComponent>(SunLight->GetLightComponent())) {
      LightComp->MarkRenderStateDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("hour"), ClampedHour);
    Result->SetNumberField(TEXT("pitch"), SolarPitch);
    Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
    SendResult(true, TEXT("Time of day updated"), FString(), Result);
    return true;
  }

  if (LowerSub == TEXT("set_sun_intensity")) {
    double Intensity = 0.0;
    if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
      SendResult(false, TEXT("Missing intensity parameter"),
                 TEXT("INVALID_ARGUMENT"), nullptr);
      return true;
    }

    ADirectionalLight *SunLight = FindFirstDirectionalLight();
    if (!SunLight) {
      SendResult(false, TEXT("No directional light found"),
                 TEXT("SUN_NOT_FOUND"), nullptr);
      return true;
    }

    if (UDirectionalLightComponent *LightComp =
            Cast<UDirectionalLightComponent>(SunLight->GetLightComponent())) {
      LightComp->SetIntensity(static_cast<float>(Intensity));
      LightComp->MarkRenderStateDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("intensity"), Intensity);
    Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
    SendResult(true, TEXT("Sun intensity updated"), FString(), Result);
    return true;
  }

  if (LowerSub == TEXT("set_skylight_intensity")) {
    double Intensity = 0.0;
    if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
      SendResult(false, TEXT("Missing intensity parameter"),
                 TEXT("INVALID_ARGUMENT"), nullptr);
      return true;
    }

    ASkyLight *SkyActor = FindFirstSkyLight();
    if (!SkyActor) {
      SendResult(false, TEXT("No skylight found"), TEXT("SKYLIGHT_NOT_FOUND"),
                 nullptr);
      return true;
    }

    if (USkyLightComponent *SkyComp = SkyActor->GetLightComponent()) {
      SkyComp->SetIntensity(static_cast<float>(Intensity));
      SkyComp->MarkRenderStateDirty();
      SkyActor->MarkComponentsRenderStateDirty();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("intensity"), Intensity);
    Result->SetStringField(TEXT("actor"), SkyActor->GetPathName());
    SendResult(true, TEXT("Skylight intensity updated"), FString(), Result);
    return true;
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("action"), LowerSub);
  SendResult(false, TEXT("Unsupported environment control action"),
             TEXT("UNSUPPORTED_ACTION"), Result);
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Environment control requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("system_control"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("system_control")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("System control requires valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("System control requires action parameter"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString LowerSub = SubAction.ToLower();
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

  // Profile commands
  if (LowerSub == TEXT("profile")) {
    FString ProfileType;
    bool bEnabled = true;
    Payload->TryGetStringField(TEXT("profileType"), ProfileType);
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    FString Command;
    if (ProfileType.ToLower() == TEXT("cpu")) {
      Command = bEnabled ? TEXT("stat cpu") : TEXT("stat cpu");
    } else if (ProfileType.ToLower() == TEXT("gpu")) {
      Command = bEnabled ? TEXT("stat gpu") : TEXT("stat gpu");
    } else if (ProfileType.ToLower() == TEXT("memory")) {
      Command = bEnabled ? TEXT("stat memory") : TEXT("stat memory");
    } else if (ProfileType.ToLower() == TEXT("fps")) {
      Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
    }

    if (!Command.IsEmpty()) {
      GEngine->Exec(nullptr, *Command);
      Result->SetStringField(TEXT("command"), Command);
      Result->SetBoolField(TEXT("enabled"), bEnabled);
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Executed profile command: %s"), *Command),
          Result, FString());
      return true;
    }
  }

  // Show FPS
  if (LowerSub == TEXT("show_fps")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    FString Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
    GEngine->Exec(nullptr, *Command);
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("enabled"), bEnabled);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("FPS display %s"),
                        bEnabled ? TEXT("enabled") : TEXT("disabled")),
        Result, FString());
    return true;
  }

  // Set quality
  if (LowerSub == TEXT("set_quality")) {
    FString Category;
    int32 Level = 1;
    Payload->TryGetStringField(TEXT("category"), Category);
    Payload->TryGetNumberField(TEXT("level"), Level);

    if (!Category.IsEmpty()) {
      FString Command = FString::Printf(TEXT("sg.%s %d"), *Category, Level);
      GEngine->Exec(nullptr, *Command);
      Result->SetStringField(TEXT("command"), Command);
      Result->SetStringField(TEXT("category"), Category);
      Result->SetNumberField(TEXT("level"), Level);
      SendAutomationResponse(
          RequestingSocket, RequestId, true,
          FString::Printf(TEXT("Set quality %s to %d"), *Category, Level),
          Result, FString());
      return true;
    }
  }

  // Screenshot
  if (LowerSub == TEXT("screenshot")) {
    FString Filename = TEXT("screenshot");
    Payload->TryGetStringField(TEXT("filename"), Filename);

    FString Command = FString::Printf(TEXT("screenshot %s"), *Filename);
    GEngine->Exec(nullptr, *Command);
    Result->SetStringField(TEXT("command"), Command);
    Result->SetStringField(TEXT("filename"), Filename);
    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Screenshot captured: %s"), *Filename), Result,
        FString());
    return true;
  }

  if (LowerSub == TEXT("get_project_settings")) {
#if WITH_EDITOR
    FString Category;
    Payload->TryGetStringField(TEXT("category"), Category);
    const FString LowerCategory = Category.ToLower();

    const UGeneralProjectSettings *ProjectSettings =
        GetDefault<UGeneralProjectSettings>();
    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
    if (ProjectSettings) {
      SettingsObj->SetStringField(TEXT("projectName"),
                                  ProjectSettings->ProjectName);
      SettingsObj->SetStringField(TEXT("companyName"),
                                  ProjectSettings->CompanyName);
      SettingsObj->SetStringField(TEXT("projectVersion"),
                                  ProjectSettings->ProjectVersion);
      SettingsObj->SetStringField(TEXT("description"),
                                  ProjectSettings->Description);
    }

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("category"),
                        Category.IsEmpty() ? TEXT("Project") : Category);
    Out->SetObjectField(TEXT("settings"), SettingsObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Project settings retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_project_settings requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSub == TEXT("get_engine_version")) {
#if WITH_EDITOR
    const FEngineVersion &EngineVer = FEngineVersion::Current();
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("version"), EngineVer.ToString());
    Out->SetNumberField(TEXT("major"), EngineVer.GetMajor());
    Out->SetNumberField(TEXT("minor"), EngineVer.GetMinor());
    Out->SetNumberField(TEXT("patch"), EngineVer.GetPatch());
    const bool bIs56OrAbove =
        (EngineVer.GetMajor() > 5) ||
        (EngineVer.GetMajor() == 5 && EngineVer.GetMinor() >= 6);
    Out->SetBoolField(TEXT("isUE56OrAbove"), bIs56OrAbove);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Engine version retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_engine_version requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSub == TEXT("get_feature_flags")) {
#if WITH_EDITOR
    bool bUnrealEditor = false;
    bool bLevelEditor = false;
    bool bEditorActor = false;

    if (GEditor) {
      if (UUnrealEditorSubsystem *UnrealEditorSS =
              GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
        bUnrealEditor = true;
      }
      if (ULevelEditorSubsystem *LevelEditorSS =
              GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()) {
        bLevelEditor = true;
      }
      if (UEditorActorSubsystem *ActorSS =
              GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
        bEditorActor = true;
      }
    }

    TSharedPtr<FJsonObject> SubsystemsObj = MakeShared<FJsonObject>();
    SubsystemsObj->SetBoolField(TEXT("unrealEditor"), bUnrealEditor);
    SubsystemsObj->SetBoolField(TEXT("levelEditor"), bLevelEditor);
    SubsystemsObj->SetBoolField(TEXT("editorActor"), bEditorActor);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("subsystems"), SubsystemsObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Feature flags retrieved"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_feature_flags requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSub == TEXT("set_project_setting")) {
#if WITH_EDITOR
    FString Section;
    FString Key;
    FString Value;
    FString ConfigName;

    if (!Payload->TryGetStringField(TEXT("section"), Section) ||
        !Payload->TryGetStringField(TEXT("key"), Key) ||
        !Payload->TryGetStringField(TEXT("value"), Value)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Missing section, key, or value"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Default to GGameIni (DefaultGame.ini) but allow overrides
    if (!Payload->TryGetStringField(TEXT("configName"), ConfigName) ||
        ConfigName.IsEmpty()) {
      ConfigName = GGameIni;
    } else if (ConfigName == TEXT("Engine")) {
      ConfigName = GEngineIni;
    } else if (ConfigName == TEXT("Input")) {
      ConfigName = GInputIni;
    } else if (ConfigName == TEXT("Game")) {
      ConfigName = GGameIni;
    }

    if (!GConfig) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("GConfig not available"), nullptr,
                             TEXT("ENGINE_ERROR"));
      return true;
    }

    GConfig->SetString(*Section, *Key, *Value, ConfigName);
    GConfig->Flush(false, ConfigName);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Project setting set: [%s] %s = %s"), *Section,
                        *Key, *Value),
        nullptr);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("set_project_setting requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  if (LowerSub == TEXT("validate_assets")) {
#if WITH_EDITOR
    const TArray<TSharedPtr<FJsonValue>> *PathsPtr = nullptr;
    if (!Payload->TryGetArrayField(TEXT("paths"), PathsPtr) || !PathsPtr) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("paths array required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> AssetPaths;
    for (const auto &Val : *PathsPtr) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }

    if (AssetPaths.Num() == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No paths provided"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (GEditor) {
      if (UEditorValidatorSubsystem *Validator =
              GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()) {
        FValidateAssetsSettings Settings;
        Settings.bSkipExcludedDirectories = true;
        Settings.bShowIfNoFailures = false;
        Settings.ValidationUsecase = EDataValidationUsecase::Script;

        TArray<FAssetData> AssetsToValidate;
        for (const FString &Path : AssetPaths) {
          // Simple logic: if it's a folder, list assets; if it's a file, try to
          // find it. We assume anything without a dot is a folder, effectively.
          // But UEditorAssetLibrary::ListAssets works recursively on module
          // paths.
          if (UEditorAssetLibrary::DoesDirectoryExist(Path)) {
            TArray<FString> FoundAssets =
                UEditorAssetLibrary::ListAssets(Path, true);
            for (const FString &AssetPath : FoundAssets) {
              FAssetData AssetData =
                  UEditorAssetLibrary::FindAssetData(AssetPath);
              if (AssetData.IsValid()) {
                AssetsToValidate.Add(AssetData);
              }
            }
          } else {
            FAssetData SpecificAsset = UEditorAssetLibrary::FindAssetData(Path);
            if (SpecificAsset.IsValid()) {
              AssetsToValidate.AddUnique(SpecificAsset);
            }
          }
        }

        if (AssetsToValidate.Num() == 0) {
          Result->SetBoolField(TEXT("success"), true);
          Result->SetStringField(TEXT("message"),
                                 TEXT("No assets found to validate"));
          SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Validation skipped (no assets)"), Result,
                                 FString());
          return true;
        }

        FValidateAssetsResults ValidationResults;
        int32 NumChecked = Validator->ValidateAssetsWithSettings(
            AssetsToValidate, Settings, ValidationResults);

        Result->SetNumberField(TEXT("checkedCount"), NumChecked);
        Result->SetNumberField(TEXT("failedCount"),
                               ValidationResults.NumInvalid);
        Result->SetNumberField(TEXT("warningCount"),
                               ValidationResults.NumWarnings);
        Result->SetNumberField(TEXT("skippedCount"),
                               ValidationResults.NumSkipped);

        bool bOverallSuccess = (ValidationResults.NumInvalid == 0);
        Result->SetStringField(
            TEXT("result"), bOverallSuccess ? TEXT("Valid") : TEXT("Invalid"));

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               bOverallSuccess ? TEXT("Validation Passed")
                                               : TEXT("Validation Failed"),
                               Result, FString());
        return true;
      } else {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("EditorValidatorSubsystem not available"),
                               nullptr, TEXT("SUBSYSTEM_MISSING"));
        return true;
      }
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("validate_assets requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // Engine quit (disabled for safety)
  if (LowerSub == TEXT("engine_quit")) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Engine quit command is disabled for safety"),
                           nullptr, TEXT("NOT_ALLOWED"));
    return true;
  }

  // Unknown sub-action: return false to allow other handlers (e.g.
  // HandleUiAction) to attempt handling it.
  // NOTE: Simple return false is not enough if the dispatcher doesn't fallback.
  // We explicitly try the UI handler here as system_control and ui actions
  // overlap.
  return HandleUiAction(RequestId, Action, Payload, RequestingSocket);
}

bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Console command requires valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Command;
  if (!Payload->TryGetStringField(TEXT("command"), Command)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Console command requires command parameter"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Block dangerous commands (Defense-in-Depth)
  FString LowerCommand = Command.ToLower();

  // 1. Explicit command blocking
  TArray<FString> ExplicitBlockedCommands = {
      TEXT("quit"),    TEXT("exit"),   TEXT("crash"),     TEXT("shutdown"),
      TEXT("restart"), TEXT("reboot"), TEXT("debug exec")};

  for (const FString &Blocked : ExplicitBlockedCommands) {
    if (LowerCommand.Equals(Blocked) ||
        LowerCommand.StartsWith(Blocked + TEXT(" "))) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Command '%s' is explicitly blocked for safety"),
                          *Command),
          nullptr, TEXT("COMMAND_BLOCKED"));
      return true;
    }
  }

  // 2. Token-based blocking (preventing system commands, file manipulation, and
  // python hacks)
  TArray<FString> ForbiddenTokens = {TEXT("rm "),
                                     TEXT("rm-"),
                                     TEXT("del "),
                                     TEXT("format "),
                                     TEXT("rmdir"),
                                     TEXT("mklink"),
                                     TEXT("copy "),
                                     TEXT("move "),
                                     TEXT("start \""),
                                     TEXT("system("),
                                     TEXT("import os"),
                                     TEXT("import subprocess"),
                                     TEXT("subprocess."),
                                     TEXT("os.system"),
                                     TEXT("exec("),
                                     TEXT("eval("),
                                     TEXT("__import__"),
                                     TEXT("import sys"),
                                     TEXT("import importlib"),
                                     TEXT("with open"),
                                     TEXT("open(")};

  for (const FString &Token : ForbiddenTokens) {
    if (LowerCommand.Contains(Token)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(
              TEXT("Command '%s' contains forbidden token '%s' and is blocked"),
              *Command, *Token),
          nullptr, TEXT("COMMAND_BLOCKED"));
      return true;
    }
  }

  // 3. Block Chaining
  if (LowerCommand.Contains(TEXT("&&")) || LowerCommand.Contains(TEXT("||"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Command chaining is blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 4. Block line breaks
  if (LowerCommand.Contains(TEXT("\n")) || LowerCommand.Contains(TEXT("\r"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Multi-line commands are blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 5. Block semicolon and pipe
  if (LowerCommand.Contains(TEXT(";")) || LowerCommand.Contains(TEXT("|"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Command chaining with semicolon or pipe is blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // 6. Block backticks
  if (LowerCommand.Contains(TEXT("`"))) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Commands containing backticks are blocked for safety"),
                           nullptr, TEXT("COMMAND_BLOCKED"));
    return true;
  }

  // Execute the command
  try {
    UWorld *TargetWorld = nullptr;
#if WITH_EDITOR
    if (GEditor) {
      // Prefer PIE world if active, otherwise Editor world
      TargetWorld = GEditor->PlayWorld;
      if (!TargetWorld) {
        TargetWorld = GEditor->GetEditorWorldContext().World();
      }
    }
#endif

    // Fallback to GWorld if no editor/PIE world found (e.g. game mode)
    if (!TargetWorld && GEngine) {
      // Note: In some contexts GWorld is a macro for a proxy, but here we need
      // a raw pointer. We'll rely on Exec handling nullptr if we really can't
      // find one, but explicitly passing the editor world fixes many "command
      // not handled" or crash issues.
    }

    GEngine->Exec(TargetWorld, *Command);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), Command);
    Result->SetBoolField(TEXT("executed"), true);

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Executed console command: %s"), *Command), Result,
        FString());
    return true;
  } catch (...) {
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Failed to execute command: %s"), *Command),
        nullptr, TEXT("EXECUTION_FAILED"));
    return true;
  }
}

bool UMcpAutomationBridgeSubsystem::HandleInspectAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Action.Equals(TEXT("inspect"), ESearchCase::IgnoreCase)) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Inspect action requires valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Inspect action requires action parameter"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString LowerSub = SubAction.ToLower();
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

  // Inspect object
  if (LowerSub == TEXT("inspect_object")) {
    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("inspect_object requires objectPath parameter"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);

    // Compatibility: allow passing actor label/name/path as objectPath.
    // Many callers use simple names like "MyActor".
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByName(ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("objectName"), TargetObject->GetName());
    Result->SetStringField(TEXT("objectClass"),
                           TargetObject->GetClass()->GetName());
    Result->SetStringField(TEXT("objectType"),
                           TargetObject->GetClass()->GetFName().ToString());

    SendAutomationResponse(
        RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Inspected object: %s"), *ObjectPath), Result,
        FString());
    return true;
  }

  // Get property
  if (LowerSub == TEXT("get_property")) {
    FString ObjectPath;
    FString PropertyName;

    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
        !Payload->TryGetStringField(TEXT("propertyName"), PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_property requires objectPath and propertyName parameters"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);

    // Compatibility: allow passing actor label/name/path as objectPath.
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByName(ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    UClass *ObjectClass = TargetObject->GetClass();
    FProperty *Property = ObjectClass->FindPropertyByName(*PropertyName);

    if (!Property) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property not found: %s"), *PropertyName),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    Result->SetStringField(TEXT("objectPath"), ObjectPath);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("propertyType"),
                           Property->GetClass()->GetName());

    // Return value as string for broad compatibility.
    FString ValueText;
    const void *ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
    Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, TargetObject,
                                    PPF_None);
    Result->SetStringField(TEXT("value"), ValueText);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("Retrieved property: %s.%s"),
                                           *ObjectPath, *PropertyName),
                           Result, FString());
    return true;
  }

  // Set property (simplified implementation)
  if (LowerSub == TEXT("set_property")) {
    FString ObjectPath;
    FString PropertyName;

    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) ||
        !Payload->TryGetStringField(TEXT("propertyName"), PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("set_property requires objectPath and propertyName parameters"),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Critical Property Protection
    TArray<FString> ProtectedProperties = {TEXT("Class"), TEXT("Outer"),
                                           TEXT("Archetype"), TEXT("Linker"),
                                           TEXT("LinkerIndex")};
    if (ProtectedProperties.Contains(PropertyName)) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(
              TEXT("Modification of critical property '%s' is blocked"),
              *PropertyName),
          nullptr, TEXT("PROPERTY_BLOCKED"));
      return true;
    }

    UObject *TargetObject = FindObject<UObject>(nullptr, *ObjectPath);

    // Compatibility: allow passing actor label/name/path as objectPath.
    if (!TargetObject) {
      if (AActor *FoundActor = FindActorByName(ObjectPath)) {
        TargetObject = FoundActor;
        ObjectPath = FoundActor->GetPathName();
      }
    }
    if (!TargetObject) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr,
          TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    // Get the property value from payload
    FString PropertyValue;
    if (!Payload->TryGetStringField(TEXT("value"), PropertyValue)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_property requires 'value' field"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Find the property using Unreal's reflection system
    FProperty *FoundProperty =
        TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!FoundProperty) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property '%s' not found on object '%s'"),
                          *PropertyName, *ObjectPath),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    // Set the property value based on type
    bool bSuccess = false;
    FString ErrorMessage;

    if (FStrProperty *StrProp = CastField<FStrProperty>(FoundProperty)) {
      void *PropAddr = StrProp->ContainerPtrToValuePtr<void>(TargetObject);
      StrProp->SetPropertyValue(PropAddr, PropertyValue);
      bSuccess = true;
    } else if (FFloatProperty *FloatProp =
                   CastField<FFloatProperty>(FoundProperty)) {
      void *PropAddr = FloatProp->ContainerPtrToValuePtr<void>(TargetObject);
      float Value = FCString::Atof(*PropertyValue);
      FloatProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FDoubleProperty *DoubleProp =
                   CastField<FDoubleProperty>(FoundProperty)) {
      void *PropAddr = DoubleProp->ContainerPtrToValuePtr<void>(TargetObject);
      double Value = FCString::Atod(*PropertyValue);
      DoubleProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FIntProperty *IntProp = CastField<FIntProperty>(FoundProperty)) {
      void *PropAddr = IntProp->ContainerPtrToValuePtr<void>(TargetObject);
      int32 Value = FCString::Atoi(*PropertyValue);
      IntProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FInt64Property *Int64Prop =
                   CastField<FInt64Property>(FoundProperty)) {
      void *PropAddr = Int64Prop->ContainerPtrToValuePtr<void>(TargetObject);
      int64 Value = FCString::Atoi64(*PropertyValue);
      Int64Prop->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FBoolProperty *BoolProp =
                   CastField<FBoolProperty>(FoundProperty)) {
      void *PropAddr = BoolProp->ContainerPtrToValuePtr<void>(TargetObject);
      bool Value = PropertyValue.ToBool();
      BoolProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FObjectProperty *ObjProp =
                   CastField<FObjectProperty>(FoundProperty)) {
      // Try to find the object by path
      UObject *ObjValue = FindObject<UObject>(nullptr, *PropertyValue);
      if (ObjValue || PropertyValue.IsEmpty()) {
        void *PropAddr = ObjProp->ContainerPtrToValuePtr<void>(TargetObject);
        ObjProp->SetPropertyValue(PropAddr, ObjValue);
        bSuccess = true;
      } else {
        ErrorMessage = FString::Printf(
            TEXT("Object property requires valid object path, got: %s"),
            *PropertyValue);
      }
    } else if (FStructProperty *StructProp =
                   CastField<FStructProperty>(FoundProperty)) {
      // Handle struct properties (FVector, FVector2D, FLinearColor, etc.)
      void *PropAddr = StructProp->ContainerPtrToValuePtr<void>(TargetObject);
      FString StructName =
          StructProp->Struct ? StructProp->Struct->GetName() : FString();

      // Try to parse JSON object value from payload
      const TSharedPtr<FJsonObject> *JsonObjValue = nullptr;
      if (Payload->TryGetObjectField(TEXT("value"), JsonObjValue) &&
          JsonObjValue->IsValid()) {
        // Handle FVector explicitly
        if (StructName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
          FVector *Vec = static_cast<FVector *>(PropAddr);
          double X = 0, Y = 0, Z = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("X"), X);
          (*JsonObjValue)->TryGetNumberField(TEXT("Y"), Y);
          (*JsonObjValue)->TryGetNumberField(TEXT("Z"), Z);
          if (X == 0 && Y == 0 && Z == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("x"), X);
            (*JsonObjValue)->TryGetNumberField(TEXT("y"), Y);
            (*JsonObjValue)->TryGetNumberField(TEXT("z"), Z);
          }
          *Vec = FVector(X, Y, Z);
          bSuccess = true;
        }
        // Handle FVector2D
        else if (StructName.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase)) {
          FVector2D *Vec = static_cast<FVector2D *>(PropAddr);
          double X = 0, Y = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("X"), X);
          (*JsonObjValue)->TryGetNumberField(TEXT("Y"), Y);
          if (X == 0 && Y == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("x"), X);
            (*JsonObjValue)->TryGetNumberField(TEXT("y"), Y);
          }
          *Vec = FVector2D(X, Y);
          bSuccess = true;
        }
        // Handle FLinearColor
        else if (StructName.Equals(TEXT("LinearColor"),
                                   ESearchCase::IgnoreCase)) {
          FLinearColor *Color = static_cast<FLinearColor *>(PropAddr);
          double R = 0, G = 0, B = 0, A = 1;
          (*JsonObjValue)->TryGetNumberField(TEXT("R"), R);
          (*JsonObjValue)->TryGetNumberField(TEXT("G"), G);
          (*JsonObjValue)->TryGetNumberField(TEXT("B"), B);
          (*JsonObjValue)->TryGetNumberField(TEXT("A"), A);
          if (R == 0 && G == 0 && B == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("r"), R);
            (*JsonObjValue)->TryGetNumberField(TEXT("g"), G);
            (*JsonObjValue)->TryGetNumberField(TEXT("b"), B);
            (*JsonObjValue)->TryGetNumberField(TEXT("a"), A);
          }
          *Color = FLinearColor(R, G, B, A);
          bSuccess = true;
        }
        // Handle FRotator
        else if (StructName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) {
          FRotator *Rot = static_cast<FRotator *>(PropAddr);
          double Pitch = 0, Yaw = 0, Roll = 0;
          (*JsonObjValue)->TryGetNumberField(TEXT("Pitch"), Pitch);
          (*JsonObjValue)->TryGetNumberField(TEXT("Yaw"), Yaw);
          (*JsonObjValue)->TryGetNumberField(TEXT("Roll"), Roll);
          if (Pitch == 0 && Yaw == 0 && Roll == 0) {
            (*JsonObjValue)->TryGetNumberField(TEXT("pitch"), Pitch);
            (*JsonObjValue)->TryGetNumberField(TEXT("yaw"), Yaw);
            (*JsonObjValue)->TryGetNumberField(TEXT("roll"), Roll);
          }
          *Rot = FRotator(Pitch, Yaw, Roll);
          bSuccess = true;
        }
      }

      // Fallback: try ImportText for string representation
      if (!bSuccess && !PropertyValue.IsEmpty() && StructProp->Struct) {
        const TCHAR *Buffer = *PropertyValue;
        // Use UScriptStruct::ImportText (not FStructProperty)
        const TCHAR *ImportResult = StructProp->Struct->ImportText(
            Buffer, PropAddr, nullptr, PPF_None, GLog, StructName);
        bSuccess = (ImportResult != nullptr);
        if (!bSuccess) {
          ErrorMessage = FString::Printf(
              TEXT("Failed to parse struct value '%s' for property '%s' of "
                   "type '%s'. For FVector use {\"X\":val,\"Y\":val,\"Z\":val} "
                   "or string \"(X=val,Y=val,Z=val)\""),
              *PropertyValue, *PropertyName, *StructName);
        }
      }

      if (!bSuccess && ErrorMessage.IsEmpty()) {
        ErrorMessage = FString::Printf(
            TEXT("Struct property '%s' of type '%s' requires JSON object "
                 "value like {\"X\":val,\"Y\":val,\"Z\":val}"),
            *PropertyName, *StructName);
      }
    } else {
      ErrorMessage =
          FString::Printf(TEXT("Property type '%s' not supported for setting"),
                          *FoundProperty->GetClass()->GetName());
    }

    if (bSuccess) {
      Result->SetStringField(TEXT("objectPath"), ObjectPath);
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("value"), PropertyValue);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Property set successfully"), Result,
                             FString());
    } else {
      Result->SetStringField(TEXT("objectPath"), ObjectPath);
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("error"), ErrorMessage);
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to set property"), Result,
                             TEXT("PROPERTY_SET_FAILED"));
    }
    return true;
  }

  // Get bounding box (get_bounding_box)
  if (LowerSub == TEXT("get_bounding_box")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);

    if (ActorName.IsEmpty() && ObjectPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_bounding_box requires actorName or objectPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *TargetActor = nullptr;
    UPrimitiveComponent *PrimComp = nullptr;

#if WITH_EDITOR
    if (GEditor && !ActorName.IsEmpty()) {
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (ActorSS) {
        TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
        for (AActor *A : Actors) {
          if (A &&
              (A->GetActorLabel() == ActorName || A->GetName() == ActorName)) {
            TargetActor = A;
            break;
          }
        }
      }
    }
#endif

    if (!TargetActor && !ObjectPath.IsEmpty()) {
      UObject *Obj = FindObject<UObject>(nullptr, *ObjectPath);
      if (Obj) {
        if (AActor *A = Cast<AActor>(Obj)) {
          TargetActor = A;
        } else if (UPrimitiveComponent *PC = Cast<UPrimitiveComponent>(Obj)) {
          PrimComp = PC;
        }
      }
    }

    FBox Box(ForceInit);
    bool bFound = false;

    if (TargetActor) {
      Box = TargetActor->GetComponentsBoundingBox(true);
      bFound = true;
    } else if (PrimComp) {
      Box = PrimComp->Bounds.GetBox();
      bFound = true;
    }

    if (bFound) {
      FVector Origin = Box.GetCenter();
      FVector Extent = Box.GetExtent();
      TSharedPtr<FJsonObject> BoxObj = MakeShared<FJsonObject>();

      TSharedPtr<FJsonObject> OrgObj = MakeShared<FJsonObject>();
      OrgObj->SetNumberField(TEXT("x"), Origin.X);
      OrgObj->SetNumberField(TEXT("y"), Origin.Y);
      OrgObj->SetNumberField(TEXT("z"), Origin.Z);
      BoxObj->SetObjectField(TEXT("origin"), OrgObj);

      TSharedPtr<FJsonObject> ExtObj = MakeShared<FJsonObject>();
      ExtObj->SetNumberField(TEXT("x"), Extent.X);
      ExtObj->SetNumberField(TEXT("y"), Extent.Y);
      ExtObj->SetNumberField(TEXT("z"), Extent.Z);
      BoxObj->SetObjectField(TEXT("extent"), ExtObj);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Bounding box retrieved"), BoxObj, FString());
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Object not found or has no bounds"), nullptr,
                             TEXT("OBJECT_NOT_FOUND"));
    }
    return true;
  }

  // Get components (get_components)
  if (LowerSub == TEXT("get_components")) {
    FString ObjectPath;
    if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath)) {
      Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
    }

    if (ObjectPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_components requires objectPath or actorName"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *FoundActor = FindActorByName(ObjectPath);
    if (!FoundActor) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(ObjectPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
          if (BP->GeneratedClass) {
            FoundActor = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
          }
        }
      }
    }

    if (!FoundActor) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Actor or Blueprint not found: %s"),
                          *ObjectPath),
          nullptr, TEXT("OBJECT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> ComponentsObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> ComponentList;

    for (UActorComponent *Comp : FoundActor->GetComponents()) {
      if (!Comp)
        continue;
      TSharedPtr<FJsonObject> CompData = MakeShared<FJsonObject>();
      CompData->SetStringField(TEXT("name"), Comp->GetName());
      CompData->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
      CompData->SetStringField(TEXT("path"), Comp->GetPathName());

      if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp)) {
        CompData->SetBoolField(TEXT("isSceneComponent"), true);
        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        FVector Loc = SceneComp->GetRelativeLocation();
        LocObj->SetNumberField("x", Loc.X);
        LocObj->SetNumberField("y", Loc.Y);
        LocObj->SetNumberField("z", Loc.Z);
        CompData->SetObjectField("relativeLocation", LocObj);
      }

      ComponentList.Add(MakeShared<FJsonValueObject>(CompData));
    }

    TSharedPtr<FJsonObject> ComponentsResult = MakeShared<FJsonObject>();
    ComponentsResult->SetArrayField(TEXT("components"), ComponentList);
    ComponentsResult->SetNumberField(TEXT("count"), ComponentList.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actor components retrieved"), ComponentsResult,
                           FString());
    return true;
  }

  // Find by class (find_by_class)
  if (LowerSub == TEXT("find_by_class")) {
#if WITH_EDITOR
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    // Also accept classPath as alias
    if (ClassName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("classPath"), ClassName);
    }

    if (GEditor) {
      UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
      if (ActorSS) {
        TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
        TArray<TSharedPtr<FJsonValue>> Matches;

        // Normalize class name for matching
        FString SearchName = ClassName;
        FString SearchNameLower = ClassName.ToLower();

        // Common prefix variants to try
        TArray<FString> SearchVariants;
        SearchVariants.Add(SearchName);
        if (!SearchName.StartsWith(TEXT("A")) &&
            !SearchName.Contains(TEXT("/"))) {
          SearchVariants.Add(TEXT("A") + SearchName); // AActor pattern
        }
        if (!SearchName.StartsWith(TEXT("U")) &&
            !SearchName.Contains(TEXT("/"))) {
          SearchVariants.Add(TEXT("U") + SearchName); // UObject pattern
        }

        for (AActor *Actor : Actors) {
          if (!Actor)
            continue;
          FString ActorClassName = Actor->GetClass()->GetName();
          FString ActorClassPath = Actor->GetClass()->GetPathName();
          FString ActorClassLower = ActorClassName.ToLower();

          bool bMatches = false;
          if (ClassName.IsEmpty()) {
            bMatches = true; // Return all actors if no filter
          } else {
            // Check all variants
            for (const FString &Variant : SearchVariants) {
              if (ActorClassName.Equals(Variant, ESearchCase::IgnoreCase) ||
                  ActorClassName.Contains(Variant, ESearchCase::IgnoreCase) ||
                  ActorClassPath.Contains(Variant, ESearchCase::IgnoreCase)) {
                bMatches = true;
                break;
              }
            }
          }

          if (bMatches) {
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
            Entry->SetStringField(TEXT("path"), Actor->GetPathName());
            Entry->SetStringField(TEXT("class"), ActorClassPath);
            Entry->SetStringField(TEXT("classShort"), ActorClassName);
            Matches.Add(MakeShared<FJsonValueObject>(Entry));
          }
        }

        Result->SetBoolField(TEXT("success"), true);
        Result->SetArrayField(TEXT("actors"), Matches);
        Result->SetNumberField(TEXT("count"), Matches.Num());
        Result->SetStringField(TEXT("searchedFor"), ClassName);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Found actors by class"), Result,
                               FString());
        return true;
      }
    }
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("find_by_class requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // Inspect class (inspect_class)
  if (LowerSub == TEXT("inspect_class")) {
    FString ClassPath;
    if (!Payload->TryGetStringField(TEXT("classPath"), ClassPath)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("classPath required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UClass *ResolvedClass = ResolveClassByName(ClassPath);
    if (!ResolvedClass) {
      // Try loading as asset
      if (UObject *Found =
              StaticLoadObject(UObject::StaticClass(), nullptr, *ClassPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Found))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Found))
          ResolvedClass = C;
      }
    }

    if (ResolvedClass) {
      Result->SetStringField(TEXT("className"), ResolvedClass->GetName());
      Result->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
      if (ResolvedClass->GetSuperClass())
        Result->SetStringField(TEXT("parentClass"),
                               ResolvedClass->GetSuperClass()->GetName());

      // List properties
      TArray<TSharedPtr<FJsonValue>> Props;
      for (TFieldIterator<FProperty> PropIt(ResolvedClass); PropIt; ++PropIt) {
        FProperty *Prop = *PropIt;
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Prop->GetName());
        P->SetStringField(TEXT("type"), Prop->GetClass()->GetName());
        Props.Add(MakeShared<FJsonValueObject>(P));
      }
      Result->SetArrayField(TEXT("properties"), Props);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Class inspected"), Result, FString());
      return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Class not found"), nullptr,
                           TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  // Get components (get_components) - enumerate all components on an actor
  if (LowerSub == TEXT("get_components")) {
#if WITH_EDITOR
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);

    if (ActorName.IsEmpty() && ObjectPath.IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("get_components requires actorName or objectPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *TargetActor = nullptr;
    if (!ActorName.IsEmpty()) {
      TargetActor = FindActorByName(ActorName);
    }
    if (!TargetActor && !ObjectPath.IsEmpty()) {
      TargetActor = FindActorByName(ObjectPath);
    }

    if (!TargetActor) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to get components for actor %s"),
                          ActorName.IsEmpty() ? *ObjectPath : *ActorName),
          nullptr, TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    for (UActorComponent *Comp : TargetActor->GetComponents()) {
      if (!Comp)
        continue;
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("name"), Comp->GetName());
      Entry->SetStringField(TEXT("readableName"), Comp->GetReadableName());
      Entry->SetStringField(TEXT("class"), Comp->GetClass()
                                               ? Comp->GetClass()->GetPathName()
                                               : TEXT(""));
      Entry->SetStringField(TEXT("path"), Comp->GetPathName());
      if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp)) {
        FVector Loc = SceneComp->GetRelativeLocation();
        FRotator Rot = SceneComp->GetRelativeRotation();
        FVector Scale = SceneComp->GetRelativeScale3D();

        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Loc.X);
        LocObj->SetNumberField(TEXT("y"), Loc.Y);
        LocObj->SetNumberField(TEXT("z"), Loc.Z);
        Entry->SetObjectField(TEXT("relativeLocation"), LocObj);

        TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
        RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
        RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
        RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
        Entry->SetObjectField(TEXT("relativeRotation"), RotObj);

        TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
        ScaleObj->SetNumberField(TEXT("x"), Scale.X);
        ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
        ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
        Entry->SetObjectField(TEXT("relativeScale"), ScaleObj);
      }
      ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    Result->SetArrayField(TEXT("components"), ComponentsArray);
    Result->SetNumberField(TEXT("count"), ComponentsArray.Num());
    Result->SetStringField(TEXT("actorName"), TargetActor->GetActorLabel());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Actor components retrieved"), Result,
                           FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_components requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // Get component property (get_component_property)
  if (LowerSub == TEXT("get_component_property")) {
#if WITH_EDITOR
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);

    if ((ActorName.IsEmpty() && ObjectPath.IsEmpty()) ||
        ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("get_component_property requires "
                                  "actorName/objectPath, componentName, and "
                                  "propertyName"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *TargetActor = nullptr;
    if (!ActorName.IsEmpty()) {
      TargetActor = FindActorByName(ActorName);
    }
    if (!TargetActor && !ObjectPath.IsEmpty()) {
      TargetActor = FindActorByName(ObjectPath);
    }

    if (!TargetActor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor not found"), nullptr,
                             TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    // Find component by name (fuzzy matching)
    UActorComponent *TargetComponent = nullptr;
    for (UActorComponent *Comp : TargetActor->GetComponents()) {
      if (!Comp)
        continue;
      if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
          Comp->GetReadableName().Equals(ComponentName,
                                         ESearchCase::IgnoreCase) ||
          Comp->GetName().Contains(ComponentName, ESearchCase::IgnoreCase)) {
        TargetComponent = Comp;
        break;
      }
    }

    if (!TargetComponent) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Component not found on actor '%s': %s"),
                          *TargetActor->GetActorLabel(), *ComponentName),
          nullptr, TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }

    FProperty *Property =
        TargetComponent->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property not found: %s"), *PropertyName),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    FString ValueText;
    const void *ValuePtr =
        Property->ContainerPtrToValuePtr<void>(TargetComponent);
    Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr,
                                    TargetComponent, PPF_None);

    Result->SetStringField(TEXT("componentName"), TargetComponent->GetName());
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("value"), ValueText);
    Result->SetStringField(TEXT("propertyType"),
                           Property->GetClass()->GetName());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Component property retrieved"), Result,
                           FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("get_component_property requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  // Set component property (set_component_property)
  if (LowerSub == TEXT("set_component_property")) {
#if WITH_EDITOR
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ObjectPath;
    Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);

    if ((ActorName.IsEmpty() && ObjectPath.IsEmpty()) ||
        ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_component_property requires "
                                  "actorName/objectPath, componentName, and "
                                  "propertyName"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    AActor *TargetActor = nullptr;
    if (!ActorName.IsEmpty()) {
      TargetActor = FindActorByName(ActorName);
    }
    if (!TargetActor && !ObjectPath.IsEmpty()) {
      TargetActor = FindActorByName(ObjectPath);
    }

    if (!TargetActor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Actor not found"), nullptr,
                             TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    // Find component by name (fuzzy matching)
    UActorComponent *TargetComponent = nullptr;
    for (UActorComponent *Comp : TargetActor->GetComponents()) {
      if (!Comp)
        continue;
      if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase) ||
          Comp->GetReadableName().Equals(ComponentName,
                                         ESearchCase::IgnoreCase) ||
          Comp->GetName().Contains(ComponentName, ESearchCase::IgnoreCase)) {
        TargetComponent = Comp;
        break;
      }
    }

    if (!TargetComponent) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Component not found on actor '%s': %s"),
                          *TargetActor->GetActorLabel(), *ComponentName),
          nullptr, TEXT("COMPONENT_NOT_FOUND"));
      return true;
    }

    FString PropertyValue;
    if (!Payload->TryGetStringField(TEXT("value"), PropertyValue)) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("set_component_property requires 'value'"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FProperty *FoundProperty =
        TargetComponent->GetClass()->FindPropertyByName(FName(*PropertyName));
    if (!FoundProperty) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Property '%s' not found on component"),
                          *PropertyName),
          nullptr, TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }

    bool bSuccess = false;
    FString ErrorMessage;

    if (FStrProperty *StrProp = CastField<FStrProperty>(FoundProperty)) {
      void *PropAddr = StrProp->ContainerPtrToValuePtr<void>(TargetComponent);
      StrProp->SetPropertyValue(PropAddr, PropertyValue);
      bSuccess = true;
    } else if (FFloatProperty *FloatProp =
                   CastField<FFloatProperty>(FoundProperty)) {
      void *PropAddr = FloatProp->ContainerPtrToValuePtr<void>(TargetComponent);
      float Value = FCString::Atof(*PropertyValue);
      FloatProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FDoubleProperty *DoubleProp =
                   CastField<FDoubleProperty>(FoundProperty)) {
      void *PropAddr =
          DoubleProp->ContainerPtrToValuePtr<void>(TargetComponent);
      double Value = FCString::Atod(*PropertyValue);
      DoubleProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FIntProperty *IntProp = CastField<FIntProperty>(FoundProperty)) {
      void *PropAddr = IntProp->ContainerPtrToValuePtr<void>(TargetComponent);
      int32 Value = FCString::Atoi(*PropertyValue);
      IntProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else if (FBoolProperty *BoolProp =
                   CastField<FBoolProperty>(FoundProperty)) {
      void *PropAddr = BoolProp->ContainerPtrToValuePtr<void>(TargetComponent);
      bool Value = PropertyValue.ToBool();
      BoolProp->SetPropertyValue(PropAddr, Value);
      bSuccess = true;
    } else {
      ErrorMessage =
          FString::Printf(TEXT("Property type '%s' not supported for setting"),
                          *FoundProperty->GetClass()->GetName());
    }

    if (bSuccess) {
      if (USceneComponent *SceneComponent =
              Cast<USceneComponent>(TargetComponent)) {
        SceneComponent->MarkRenderStateDirty();
        SceneComponent->UpdateComponentToWorld();
      }
      TargetComponent->MarkPackageDirty();

      Result->SetStringField(TEXT("componentName"), TargetComponent->GetName());
      Result->SetStringField(TEXT("propertyName"), PropertyName);
      Result->SetStringField(TEXT("value"), PropertyValue);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Component property set"), Result, FString());
    } else {
      Result->SetStringField(TEXT("error"), ErrorMessage);
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Failed to set component property"), Result,
                             TEXT("PROPERTY_SET_FAILED"));
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("set_component_property requires editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
  }

  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralTerrain(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_procedural_terrain"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_procedural_terrain payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString Name;
  if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FVector Location(0, 0, 0);
  const TArray<TSharedPtr<FJsonValue>> *LocArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
      LocArr->Num() >= 3) {
    Location.X = (*LocArr)[0]->AsNumber();
    Location.Y = (*LocArr)[1]->AsNumber();
    Location.Z = (*LocArr)[2]->AsNumber();
  }

  double SizeX = 2000.0;
  double SizeY = 2000.0;
  Payload->TryGetNumberField(TEXT("sizeX"), SizeX);
  Payload->TryGetNumberField(TEXT("sizeY"), SizeY);

  int32 Subdivisions = 50;
  Payload->TryGetNumberField(TEXT("subdivisions"), Subdivisions);
  Subdivisions = FMath::Clamp(Subdivisions, 2, 255);

  FString MaterialPath;
  Payload->TryGetStringField(TEXT("material"), MaterialPath);

  if (!GEditor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  AActor *NewActor = SpawnActorInActiveWorld<AActor>(
      AActor::StaticClass(), Location, FRotator::ZeroRotator, Name);
  if (!NewActor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn actor"), TEXT("SPAWN_FAILED"));
    return true;
  }

  UProceduralMeshComponent *ProcMesh = NewObject<UProceduralMeshComponent>(
      NewActor, FName(TEXT("ProceduralTerrain")));
  if (!ProcMesh) {
    ActorSS->DestroyActor(NewActor);
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create ProceduralMeshComponent"),
                        TEXT("COMPONENT_CREATION_FAILED"));
    return true;
  }

  ProcMesh->RegisterComponent();
  NewActor->SetRootComponent(ProcMesh);
  NewActor->AddInstanceComponent(ProcMesh);

  // Generate grid
  TArray<FVector> Vertices;
  TArray<int32> Triangles;
  TArray<FVector> Normals;
  TArray<FVector2D> UV0;
  TArray<FColor> VertexColors;
  TArray<FProcMeshTangent> Tangents;

  const float StepX = SizeX / Subdivisions;
  const float StepY = SizeY / Subdivisions;
  const float UVStep = 1.0f / Subdivisions;

  for (int32 Y = 0; Y <= Subdivisions; Y++) {
    for (int32 X = 0; X <= Subdivisions; X++) {
      float Z = 0.0f;
      // Simple sine wave terrain as default since we can't easily parse the
      // math string
      Z = FMath::Sin(X * 0.1f) * 50.0f + FMath::Cos(Y * 0.1f) * 30.0f;

      Vertices.Add(FVector(X * StepX - SizeX / 2, Y * StepY - SizeY / 2, Z));
      Normals.Add(FVector(0, 0, 1)); // Simplified normal
      UV0.Add(FVector2D(X * UVStep, Y * UVStep));
      VertexColors.Add(FColor::White);
      Tangents.Add(FProcMeshTangent(1, 0, 0));
    }
  }

  for (int32 Y = 0; Y < Subdivisions; Y++) {
    for (int32 X = 0; X < Subdivisions; X++) {
      int32 TopLeft = Y * (Subdivisions + 1) + X;
      int32 TopRight = TopLeft + 1;
      int32 BottomLeft = (Y + 1) * (Subdivisions + 1) + X;
      int32 BottomRight = BottomLeft + 1;

      Triangles.Add(TopLeft);
      Triangles.Add(BottomLeft);
      Triangles.Add(TopRight);

      Triangles.Add(TopRight);
      Triangles.Add(BottomLeft);
      Triangles.Add(BottomRight);
    }
  }

  ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0,
                              VertexColors, Tangents, true);

  if (!MaterialPath.IsEmpty()) {
    UMaterialInterface *Mat =
        LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (Mat) {
      ProcMesh->SetMaterial(0, Mat);
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actor_name"), NewActor->GetActorLabel());
  Resp->SetNumberField(TEXT("vertices"), Vertices.Num());
  Resp->SetNumberField(TEXT("triangles"), Triangles.Num() / 3);

  TSharedPtr<FJsonObject> SizeObj = MakeShared<FJsonObject>();
  SizeObj->SetNumberField(TEXT("x"), SizeX);
  SizeObj->SetNumberField(TEXT("y"), SizeY);
  Resp->SetObjectField(TEXT("size"), SizeObj);
  Resp->SetNumberField(TEXT("subdivisions"), Subdivisions);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Procedural terrain created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("create_procedural_terrain requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBakeLightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bake_lightmap"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  FString QualityStr = TEXT("Preview");
  if (Payload.IsValid())
    Payload->TryGetStringField(TEXT("quality"), QualityStr);

  // Reuse HandleExecuteEditorFunction logic
  TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
  P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
  P->SetStringField(TEXT("quality"), QualityStr);

  return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"),
                                     P, RequestingSocket);

#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Requires editor"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
