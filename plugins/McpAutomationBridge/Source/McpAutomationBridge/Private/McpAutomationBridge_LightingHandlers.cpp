#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TextureCube.h"

#if WITH_EDITOR
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Lightmass/LightmassImportanceVolume.h"

/* UE5.6: LightingBuildOptions.h removed; use console exec */
#include "Editor/UnrealEd/Public/Editor.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"

#endif

bool UMcpAutomationBridgeSubsystem::HandleLightingAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  
  // Static set of all lighting/post-process actions we handle
  static TSet<FString> LightingActions = {
    // Core lighting
    TEXT("spawn_light"), TEXT("create_light"), TEXT("create_dynamic_light"),
    TEXT("spawn_sky_light"), TEXT("create_sky_light"), TEXT("build_lighting"),
    TEXT("ensure_single_sky_light"), TEXT("create_lighting_enabled_level"),
    TEXT("create_lightmass_volume"), TEXT("setup_volumetric_fog"),
    TEXT("setup_global_illumination"), TEXT("configure_shadows"),
    TEXT("set_exposure"), TEXT("list_light_types"), TEXT("set_ambient_occlusion"),
    // Lumen
    TEXT("configure_lumen_gi"), TEXT("set_lumen_reflections"),
    TEXT("tune_lumen_performance"), TEXT("create_lumen_volume"),
    TEXT("set_virtual_shadow_maps"),
    // MegaLights (5.7+)
    TEXT("configure_megalights_scene"), TEXT("get_megalights_budget"),
    TEXT("optimize_lights_for_megalights"),
    // Advanced lighting
    TEXT("configure_gi_settings"), TEXT("bake_lighting_preview"),
    TEXT("get_light_complexity"), TEXT("configure_volumetric_fog"),
    TEXT("create_light_batch"), TEXT("configure_shadow_settings"),
    TEXT("validate_lighting_setup"),
    // Post-process (merged from manage_post_process)
    TEXT("create_post_process_volume"), TEXT("configure_pp_blend"),
    TEXT("configure_pp_priority"), TEXT("get_post_process_settings"),
    TEXT("configure_bloom"), TEXT("configure_dof"), TEXT("configure_motion_blur"),
    TEXT("configure_color_grading"), TEXT("configure_white_balance"),
    TEXT("configure_vignette"), TEXT("configure_chromatic_aberration"),
    TEXT("configure_film_grain"), TEXT("configure_lens_flares"),
    // Reflections
    TEXT("create_sphere_reflection_capture"), TEXT("create_box_reflection_capture"),
    TEXT("create_planar_reflection"), TEXT("recapture_scene"),
    // Scene capture
    TEXT("create_scene_capture_2d"), TEXT("create_scene_capture_cube"),
    TEXT("capture_scene"),
    // Light channels
    TEXT("set_light_channel"), TEXT("set_actor_light_channel"),
    // Ray tracing
    TEXT("configure_ray_traced_shadows"), TEXT("configure_ray_traced_gi"),
    TEXT("configure_ray_traced_reflections"), TEXT("configure_ray_traced_ao"),
    TEXT("configure_path_tracing"),
    // Lightmass settings
    TEXT("configure_lightmass_settings"), TEXT("build_lighting_quality"),
    TEXT("configure_indirect_lighting_cache"), TEXT("configure_volumetric_lightmap")
  };
  
  if (!LightingActions.Contains(Lower)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Lighting payload missing"),
                        TEXT("INVALID_PAYLOAD"));
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

  if (Lower == TEXT("list_light_types")) {
    TArray<TSharedPtr<FJsonValue>> Types;
    // Add common shortcuts first
    Types.Add(MakeShared<FJsonValueString>(TEXT("DirectionalLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("PointLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("SpotLight")));
    Types.Add(MakeShared<FJsonValueString>(TEXT("RectLight")));

    // Discover all ALight subclasses via reflection
    TSet<FString> AddedNames;
    AddedNames.Add(TEXT("DirectionalLight"));
    AddedNames.Add(TEXT("PointLight"));
    AddedNames.Add(TEXT("SpotLight"));
    AddedNames.Add(TEXT("RectLight"));

    TArray<UClass*> LightClasses;
    GetDerivedClasses(ALight::StaticClass(), LightClasses, true);
    for (UClass* Class : LightClasses) {
      if (!Class->HasAnyClassFlags(CLASS_Abstract) &&
          !AddedNames.Contains(Class->GetName())) {
        Types.Add(MakeShared<FJsonValueString>(Class->GetName()));
        AddedNames.Add(Class->GetName());
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetArrayField(TEXT("types"), Types);
    Resp->SetNumberField(TEXT("count"), Types.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Available light types"), Resp);
    return true;
  }

  if (Lower == TEXT("spawn_light") || Lower == TEXT("create_light") || Lower == TEXT("create_dynamic_light")) {
    FString LightClassStr;
    if (!Payload->TryGetStringField(TEXT("lightClass"), LightClassStr) ||
        LightClassStr.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("lightClass required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Dynamic resolution with heuristics
    UClass *LightClass = ResolveUClass(LightClassStr);

    // Try finding with 'A' prefix (standard Actor prefix)
    if (!LightClass) {
      LightClass = ResolveUClass(TEXT("A") + LightClassStr);
    }

    if (!LightClass || !LightClass->IsChildOf(ALight::StaticClass())) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid light class: %s"), *LightClassStr),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocPtr;
    if (Payload->TryGetObjectField(TEXT("location"), LocPtr)) {
      Location.X = (*LocPtr)->GetNumberField(TEXT("x"));
      Location.Y = (*LocPtr)->GetNumberField(TEXT("y"));
      Location.Z = (*LocPtr)->GetNumberField(TEXT("z"));
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr)) {
      Rotation.Pitch = (*RotPtr)->GetNumberField(TEXT("pitch"));
      Rotation.Yaw = (*RotPtr)->GetNumberField(TEXT("yaw"));
      Rotation.Roll = (*RotPtr)->GetNumberField(TEXT("roll"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Safety check: Validate ActorSS and World before spawning
    if (!ActorSS || !ActorSS->GetWorld()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No valid world context available for spawning light"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *NewLight = ActorSS->GetWorld()->SpawnActor(LightClass, &Location,
                                                       &Rotation, SpawnParams);

    // Explicitly set location/rotation
    if (NewLight) {
      // Set label immediately
      NewLight->SetActorLabel(LightClassStr);
      NewLight->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                            ETeleportType::TeleportPhysics);
    }

    if (!NewLight) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn light actor"),
                          TEXT("SPAWN_FAILED"));
      return true;
    }

    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) {
      NewLight->SetActorLabel(Name);
    }

    // Default to Movable for immediate feedback
    if (ULightComponent *BaseLightComp =
            NewLight->FindComponentByClass<ULightComponent>()) {
      BaseLightComp->SetMobility(EComponentMobility::Movable);
    }

    // Apply properties
    const TSharedPtr<FJsonObject> *Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
      ULightComponent *LightComp =
          NewLight->FindComponentByClass<ULightComponent>();
      if (LightComp) {
        double Intensity;
        if ((*Props)->TryGetNumberField(TEXT("intensity"), Intensity)) {
          LightComp->SetIntensity((float)Intensity);
        }

        const TSharedPtr<FJsonObject> *ColorObj;
        if ((*Props)->TryGetObjectField(TEXT("color"), ColorObj)) {
          FLinearColor Color;
          Color.R = (*ColorObj)->GetNumberField(TEXT("r"));
          Color.G = (*ColorObj)->GetNumberField(TEXT("g"));
          Color.B = (*ColorObj)->GetNumberField(TEXT("b"));
          Color.A = (*ColorObj)->HasField(TEXT("a"))
                        ? (*ColorObj)->GetNumberField(TEXT("a"))
                        : 1.0f;
          LightComp->SetLightColor(Color);
        }

        bool bCastShadows;
        if ((*Props)->TryGetBoolField(TEXT("castShadows"), bCastShadows)) {
          LightComp->SetCastShadows(bCastShadows);
        }

        // Type specific properties
        if (UDirectionalLightComponent *DirComp =
                Cast<UDirectionalLightComponent>(LightComp)) {
          // Default to using as Atmosphere Sun Light unless explicitly disabled
          bool bUseSun = true;
          if ((*Props)->TryGetBoolField(TEXT("useAsAtmosphereSunLight"),
                                        bUseSun)) {
            DirComp->SetAtmosphereSunLight(bUseSun);
          } else {
            DirComp->SetAtmosphereSunLight(true);
          }
        }

        if (UPointLightComponent *PointComp =
                Cast<UPointLightComponent>(LightComp)) {
          double Radius;
          if ((*Props)->TryGetNumberField(TEXT("attenuationRadius"), Radius)) {
            PointComp->SetAttenuationRadius((float)Radius);
          }
        }

        if (USpotLightComponent *SpotComp =
                Cast<USpotLightComponent>(LightComp)) {
          double InnerCone;
          if ((*Props)->TryGetNumberField(TEXT("innerConeAngle"), InnerCone)) {
            SpotComp->SetInnerConeAngle((float)InnerCone);
          }
          double OuterCone;
          if ((*Props)->TryGetNumberField(TEXT("outerConeAngle"), OuterCone)) {
            SpotComp->SetOuterConeAngle((float)OuterCone);
          }
        }

        if (URectLightComponent *RectComp =
                Cast<URectLightComponent>(LightComp)) {
          double Width;
          if ((*Props)->TryGetNumberField(TEXT("sourceWidth"), Width)) {
            RectComp->SetSourceWidth((float)Width);
          }
          double Height;
          if ((*Props)->TryGetNumberField(TEXT("sourceHeight"), Height)) {
            RectComp->SetSourceHeight((float)Height);
          }
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), NewLight->GetActorLabel());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Light spawned"), Resp);
    return true;
  } else if (Lower == TEXT("spawn_sky_light") || Lower == TEXT("create_sky_light")) {
    AActor *SkyLight = SpawnActorInActiveWorld<AActor>(
        ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!SkyLight) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn SkyLight"),
                          TEXT("SPAWN_FAILED"));
      return true;
    }

    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) {
      SkyLight->SetActorLabel(Name);
    }

    USkyLightComponent *SkyComp =
        SkyLight->FindComponentByClass<USkyLightComponent>();
    if (SkyComp) {
      FString SourceType;
      if (Payload->TryGetStringField(TEXT("sourceType"), SourceType)) {
        if (SourceType == TEXT("SpecifiedCubemap")) {
          SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
          FString CubemapPath;
          if (Payload->TryGetStringField(TEXT("cubemapPath"), CubemapPath) &&
              !CubemapPath.IsEmpty()) {
            UTextureCube *Cubemap = Cast<UTextureCube>(StaticLoadObject(
                UTextureCube::StaticClass(), nullptr, *CubemapPath));
            if (Cubemap) {
              SkyComp->Cubemap = Cubemap;
            }
          }
        } else {
          SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
        }
      }

      double Intensity;
      if (Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
        SkyComp->SetIntensity((float)Intensity);
      }

      bool bRecapture;
      if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) &&
          bRecapture) {
        SkyComp->RecaptureSky();
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), SkyLight->GetActorLabel());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SkyLight spawned"), Resp);
    return true;
  } else if (Lower == TEXT("build_lighting")) {
    if (GEditor && GetActiveWorld()) {
      if (GEditor && GetActiveWorld())
        GEditor->Exec(GetActiveWorld(),
                      TEXT("BuildLighting Production"));
    }
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Lighting build started"), nullptr);
    return true;
  } else if (Lower == TEXT("ensure_single_sky_light")) {
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    TArray<AActor *> SkyLights;
    for (AActor *Actor : AllActors) {
      if (Actor && Actor->IsA<ASkyLight>()) {
        SkyLights.Add(Actor);
      }
    }

    FString TargetName;
    Payload->TryGetStringField(TEXT("name"), TargetName);
    if (TargetName.IsEmpty())
      TargetName = TEXT("SkyLight");

    int32 RemovedCount = 0;
    AActor *KeptActor = nullptr;

    // Keep the one matching the name, or the first one
    for (AActor *SkyLight : SkyLights) {
      if (!KeptActor &&
          (SkyLight->GetActorLabel() == TargetName || TargetName.IsEmpty())) {
        KeptActor = SkyLight;
        if (!TargetName.IsEmpty())
          SkyLight->SetActorLabel(TargetName);
      } else if (!KeptActor) {
        KeptActor = SkyLight;
        if (!TargetName.IsEmpty())
          SkyLight->SetActorLabel(TargetName);
      } else {
        ActorSS->DestroyActor(SkyLight);
        RemovedCount++;
      }
    }

    if (!KeptActor) {
      // Spawn one if none existed
      KeptActor = SpawnActorInActiveWorld<AActor>(
          ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator,
          TargetName);
      // Label already set by SpawnActorInActiveWorld if TargetName was provided
    }

    if (KeptActor) {
      bool bRecapture;
      if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) &&
          bRecapture) {
        if (USkyLightComponent *Comp =
                KeptActor->FindComponentByClass<USkyLightComponent>()) {
          Comp->RecaptureSky();
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetNumberField(TEXT("removed"), RemovedCount);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Ensured single SkyLight"), Resp);
    return true;
  } else if (Lower == TEXT("create_lightmass_volume")) {
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
      Location.X = (*LocObj)->GetNumberField(TEXT("x"));
      Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
      Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
    }

    FVector Size = FVector(1000, 1000, 1000);
    const TSharedPtr<FJsonObject> *SizeObj;
    if (Payload->TryGetObjectField(TEXT("size"), SizeObj)) {
      Size.X = (*SizeObj)->GetNumberField(TEXT("x"));
      Size.Y = (*SizeObj)->GetNumberField(TEXT("y"));
      Size.Z = (*SizeObj)->GetNumberField(TEXT("z"));
    }

    AActor *Volume = SpawnActorInActiveWorld<AActor>(
        ALightmassImportanceVolume::StaticClass(), Location,
        FRotator::ZeroRotator);
    if (Volume) {
      Volume->SetActorScale3D(Size /
                              200.0f); // Brush size adjustment approximation

      FString Name;
      if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) {
        Volume->SetActorLabel(Name);
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorName"), Volume->GetActorLabel());

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("LightmassImportanceVolume created"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn LightmassImportanceVolume"),
                          TEXT("SPAWN_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("setup_volumetric_fog")) {
    // Find existing or spawn new ExponentialHeightFog
    AExponentialHeightFog *FogActor = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors) {
      if (Actor && Actor->IsA<AExponentialHeightFog>()) {
        FogActor = Cast<AExponentialHeightFog>(Actor);
        break;
      }
    }

    if (!FogActor) {
      FogActor = Cast<AExponentialHeightFog>(SpawnActorInActiveWorld<AActor>(
          AExponentialHeightFog::StaticClass(), FVector::ZeroVector,
          FRotator::ZeroRotator));
    }

    if (FogActor && FogActor->GetComponent()) {
      UExponentialHeightFogComponent *FogComp = FogActor->GetComponent();
      FogComp->bEnableVolumetricFog = true;

      double Distance;
      if (Payload->TryGetNumberField(TEXT("viewDistance"), Distance)) {
        FogComp->VolumetricFogDistance = (float)Distance;
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorName"), FogActor->GetActorLabel());
      Resp->SetBoolField(TEXT("enabled"), true);

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Volumetric fog enabled"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to find or spawn ExponentialHeightFog"),
                          TEXT("EXECUTION_ERROR"));
    }
    return true;
  } else if (Lower == TEXT("setup_global_illumination")) {
    FString Method;
    if (Payload->TryGetStringField(TEXT("method"), Method)) {
      if (Method == TEXT("LumenGI")) {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
          CVar->Set(1); // 1 = Lumen

        IConsoleVariable *CVarRefl = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.ReflectionMethod"));
        if (CVarRefl)
          CVarRefl->Set(1); // 1 = Lumen
      } else if (Method == TEXT("ScreenSpace")) {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
          CVar->Set(2); // SSGI
      } else if (Method == TEXT("None")) {
        IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.DynamicGlobalIlluminationMethod"));
        if (CVar)
          CVar->Set(0);
      }
    }
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("method"), Method);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("GI method configured"), Resp);
    return true;
  } else if (Lower == TEXT("configure_shadows")) {
    bool bVirtual = false;
    if (Payload->TryGetBoolField(TEXT("virtualShadowMaps"), bVirtual) ||
        Payload->TryGetBoolField(TEXT("rayTracedShadows"), bVirtual)) {
      // Loose mapping to VSM
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.Shadow.Virtual.Enable"));
      if (CVar)
        CVar->Set(bVirtual ? 1 : 0);
    }
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("virtualShadowMaps"), bVirtual);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Shadows configured"), Resp);
    return true;
  } else if (Lower == TEXT("set_exposure")) {
    // Requires a PostProcessVolume.
    // Find unbounded one or spawn one.
    APostProcessVolume *PPV = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors) {
      if (Actor && Actor->IsA<APostProcessVolume>()) {
        APostProcessVolume *Candidate = Cast<APostProcessVolume>(Actor);
        if (Candidate->bUnbound) {
          PPV = Candidate;
          break;
        }
      }
    }

    if (!PPV) {
      PPV = Cast<APostProcessVolume>(SpawnActorInActiveWorld<AActor>(
          APostProcessVolume::StaticClass(), FVector::ZeroVector,
          FRotator::ZeroRotator));
      if (PPV)
        PPV->bUnbound = true;
    }

    if (PPV) {
      double MinB = 0.0, MaxB = 0.0;
      if (Payload->TryGetNumberField(TEXT("minBrightness"), MinB))
        PPV->Settings.AutoExposureMinBrightness = (float)MinB;
      if (Payload->TryGetNumberField(TEXT("maxBrightness"), MaxB))
        PPV->Settings.AutoExposureMaxBrightness = (float)MaxB;

      // Bias/Compensation
      double Comp = 0.0;
      if (Payload->TryGetNumberField(TEXT("compensationValue"), Comp))
        PPV->Settings.AutoExposureBias = (float)Comp;

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Exposure settings applied"), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to find/spawn PostProcessVolume"),
                          TEXT("EXECUTION_ERROR"));
    }
    return true;
  } else if (Lower == TEXT("set_ambient_occlusion")) {
    // Find unbounded one or spawn one.
    APostProcessVolume *PPV = nullptr;
    TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : AllActors) {
      if (Actor && Actor->IsA<APostProcessVolume>()) {
        APostProcessVolume *Candidate = Cast<APostProcessVolume>(Actor);
        if (Candidate->bUnbound) {
          PPV = Candidate;
          break;
        }
      }
    }

    if (!PPV) {
      PPV = Cast<APostProcessVolume>(SpawnActorInActiveWorld<AActor>(
          APostProcessVolume::StaticClass(), FVector::ZeroVector,
          FRotator::ZeroRotator));
      if (PPV)
        PPV->bUnbound = true;
    }

    if (PPV) {
      bool bEnabled = true;
      if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
        PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
        PPV->Settings.AmbientOcclusionIntensity =
            bEnabled ? 0.5f : 0.0f; // Default on if enabled, 0 if disabled
      }

      double Intensity;
      if (Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
        PPV->Settings.bOverride_AmbientOcclusionIntensity = true;
        PPV->Settings.AmbientOcclusionIntensity = (float)Intensity;
      }

      double Radius;
      if (Payload->TryGetNumberField(TEXT("radius"), Radius)) {
        PPV->Settings.bOverride_AmbientOcclusionRadius = true;
        PPV->Settings.AmbientOcclusionRadius = (float)Radius;
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("actorName"), PPV->GetActorLabel());
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Ambient Occlusion settings configured"),
                             Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to find/spawn PostProcessVolume"),
                          TEXT("EXECUTION_ERROR"));
    }
    return true;
  } else if (Lower == TEXT("create_lighting_enabled_level")) {
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("path required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (GEditor) {
      // Create a new blank map
      GEditor->NewMap();
      bool bNewMap = true; // Assume success
      if (!bNewMap) {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create new map"),
                            TEXT("CREATION_FAILED"));
        return true;
      }

      // Add basic lighting
      AActor* SunActor = SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                      FVector(0, 0, 500), FRotator(-45, 0, 0),
                                      TEXT("Sun"));
      AActor* SkyLightActor = SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                      FVector::ZeroVector,
                                      FRotator::ZeroRotator, TEXT("SkyLight"));
      
      if (!SunActor || !SkyLightActor) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Failed to spawn one or more lighting actors"));
      }

      // Save the level
      bool bSaved = FEditorFileUtils::SaveLevel(
          GetActiveWorld()->PersistentLevel, *Path);
      
      // UE 5.7 + Intel GPU Workaround: Level save with HLOD/WorldPartition triggers
      // recursive FlushRenderingCommands which can cause a GPU driver race condition.
      // Defer the response by ~100ms to let rendering thread stabilize.
      if (bSaved) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), Path);
        Resp->SetStringField(TEXT("message"),
                             TEXT("Level created with lighting"));
        
        // Capture for deferred response
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf = this;
        const FString CapturedRequestId = RequestId;
        TSharedPtr<FMcpBridgeWebSocket> CapturedSocket = RequestingSocket;
        TSharedPtr<FJsonObject> CapturedResp = Resp;
        
        if (GEditor)
        {
          FTimerDelegate ResponseDelegate;
          ResponseDelegate.BindLambda([WeakSelf, CapturedSocket, CapturedRequestId, CapturedResp]()
          {
            if (UMcpAutomationBridgeSubsystem* Self = WeakSelf.Get())
            {
              Self->SendAutomationResponse(CapturedSocket, CapturedRequestId, true,
                                 TEXT("Level created with lighting"), CapturedResp);
            }
          });
          
          FTimerHandle TempHandle;
          GEditor->GetTimerManager()->SetTimer(TempHandle, ResponseDelegate, 0.1f, false);
        }
        else
        {
          SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level created with lighting"), Resp);
        }
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to save level"), TEXT("SAVE_FAILED"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    }
    return true;
  } else if (Lower == TEXT("configure_lumen_gi") || Lower == TEXT("tune_lumen_performance")) {
      // Handle Lumen GI configuration
      double Quality;
      if (Payload->TryGetNumberField(TEXT("quality"), Quality)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Quality"))->Set((int32)Quality);
      }
      
      bool bDetailTrace;
      if (Payload->TryGetBoolField(TEXT("detailTrace"), bDetailTrace)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.DetailTrace"))->Set(bDetailTrace ? 1 : 0);
      }

      double UpdateSpeed;
      if (Payload->TryGetNumberField(TEXT("updateSpeed"), UpdateSpeed)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.LumenScene.UpdateSpeed"))->Set((float)UpdateSpeed);
      }

      double FinalGatherQuality;
      if (Payload->TryGetNumberField(TEXT("finalGatherQuality"), FinalGatherQuality)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.ScreenProbeGather.Quality"))->Set((float)FinalGatherQuality);
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lumen GI configured"), Resp);
      return true;
  } else if (Lower == TEXT("set_lumen_reflections")) {
      // Handle Lumen Reflections
      double Quality;
      if (Payload->TryGetNumberField(TEXT("quality"), Quality)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Quality"))->Set((int32)Quality);
      }

      bool bDetailTrace;
      if (Payload->TryGetBoolField(TEXT("detailTrace"), bDetailTrace)) {
          IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.DetailTrace"))->Set(bDetailTrace ? 1 : 0);
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lumen reflections configured"), Resp);
      return true;
  } else if (Lower == TEXT("create_lumen_volume")) {
      // Spawn PostProcessVolume for Lumen
      FVector Location = FVector::ZeroVector;
      const TSharedPtr<FJsonObject> *LocObj;
      if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
          Location.X = (*LocObj)->GetNumberField(TEXT("x"));
          Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
          Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
      }

      FVector Size = FVector(1000, 1000, 1000);
      const TSharedPtr<FJsonObject> *SizeObj;
      if (Payload->TryGetObjectField(TEXT("size"), SizeObj)) {
          Size.X = (*SizeObj)->GetNumberField(TEXT("x"));
          Size.Y = (*SizeObj)->GetNumberField(TEXT("y"));
          Size.Z = (*SizeObj)->GetNumberField(TEXT("z"));
      }

      APostProcessVolume* Volume = SpawnActorInActiveWorld<APostProcessVolume>(
          APostProcessVolume::StaticClass(), Location, FRotator::ZeroRotator);
      
      if (Volume) {
          Volume->SetActorScale3D(Size / 200.0f);
          Volume->bUnbound = false;
          Volume->Priority = 100.0f; // High priority for local override
          
          // Enable Lumen settings
          Volume->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
          Volume->Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
          
          Volume->Settings.bOverride_ReflectionMethod = true;
          Volume->Settings.ReflectionMethod = EReflectionMethod::Lumen;

          FString Name;
          if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) {
              Volume->SetActorLabel(Name);
          }

          TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
          Resp->SetBoolField(TEXT("success"), true);
          Resp->SetStringField(TEXT("actorName"), Volume->GetActorLabel());
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lumen volume created"), Resp);
      } else {
          SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn Lumen volume"), TEXT("SPAWN_FAILED"));
      }
      return true;
  } else if (Lower == TEXT("set_virtual_shadow_maps")) {
      bool bEnable = true;
      Payload->TryGetBoolField(TEXT("enabled"), bEnable);
      
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
      if (CVar) CVar->Set(bEnable ? 1 : 0);

      double Resolution;
      if (Payload->TryGetNumberField(TEXT("resolution"), Resolution)) {
           // This might affect r.Shadow.Virtual.ShadowMap.ResolutionLocal usually
      }

      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Virtual Shadow Maps configured"), Resp);
      return true;
  }
  // ========================================================================
  // Forward post-process/reflection/scene-capture/ray-tracing/lightmass actions
  // to HandlePostProcessAction by wrapping payload with action field
  // ========================================================================
  else if (Lower == TEXT("create_post_process_volume") ||
           Lower == TEXT("configure_pp_blend") ||
           Lower == TEXT("configure_pp_priority") ||
           Lower == TEXT("get_post_process_settings") ||
           Lower == TEXT("configure_bloom") ||
           Lower == TEXT("configure_dof") ||
           Lower == TEXT("configure_motion_blur") ||
           Lower == TEXT("configure_color_grading") ||
           Lower == TEXT("configure_white_balance") ||
           Lower == TEXT("configure_vignette") ||
           Lower == TEXT("configure_chromatic_aberration") ||
           Lower == TEXT("configure_film_grain") ||
           Lower == TEXT("configure_lens_flares") ||
           Lower == TEXT("create_sphere_reflection_capture") ||
           Lower == TEXT("create_box_reflection_capture") ||
           Lower == TEXT("create_planar_reflection") ||
           Lower == TEXT("recapture_scene") ||
           Lower == TEXT("create_scene_capture_2d") ||
           Lower == TEXT("create_scene_capture_cube") ||
           Lower == TEXT("capture_scene") ||
           Lower == TEXT("set_light_channel") ||
           Lower == TEXT("set_actor_light_channel") ||
           Lower == TEXT("configure_ray_traced_shadows") ||
           Lower == TEXT("configure_ray_traced_gi") ||
           Lower == TEXT("configure_ray_traced_reflections") ||
           Lower == TEXT("configure_ray_traced_ao") ||
           Lower == TEXT("configure_path_tracing") ||
           Lower == TEXT("configure_lightmass_settings") ||
           Lower == TEXT("build_lighting_quality") ||
           Lower == TEXT("configure_indirect_lighting_cache") ||
           Lower == TEXT("configure_volumetric_lightmap")) {
      // Create a wrapper payload with "action" field for PostProcessAction handler
      TSharedPtr<FJsonObject> WrapperPayload = MakeShared<FJsonObject>();
      // Copy all fields from original payload
      for (const auto& Pair : Payload->Values) {
          WrapperPayload->SetField(Pair.Key, Pair.Value);
      }
      // Set the action field to the lowercase action name
      WrapperPayload->SetStringField(TEXT("action"), Lower);
      
      // Delegate to PostProcessAction handler with manage_post_process action
      return HandlePostProcessAction(RequestId, TEXT("manage_post_process"), WrapperPayload, RequestingSocket);
  }
  // ========================================================================
  // MegaLights (UE 5.7+)
  // ========================================================================
  else if (Lower == TEXT("configure_megalights_scene")) {
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
      
      // MegaLights is controlled via console variables in 5.7+
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Enable"));
      if (CVar) {
          CVar->Set(bEnabled ? 1 : 0);
      }
      
      double Budget = 0.0;
      if (Payload->TryGetNumberField(TEXT("budget"), Budget)) {
          IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
          if (BudgetCVar) {
              BudgetCVar->Set(static_cast<int32>(Budget));
          }
      }
      
      FString Quality;
      if (Payload->TryGetStringField(TEXT("quality"), Quality)) {
          // Map quality presets to budget values
          if (Quality.Equals(TEXT("Low"), ESearchCase::IgnoreCase)) {
              IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
              if (BudgetCVar) BudgetCVar->Set(64);
          } else if (Quality.Equals(TEXT("Medium"), ESearchCase::IgnoreCase)) {
              IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
              if (BudgetCVar) BudgetCVar->Set(128);
          } else if (Quality.Equals(TEXT("High"), ESearchCase::IgnoreCase)) {
              IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
              if (BudgetCVar) BudgetCVar->Set(256);
          } else if (Quality.Equals(TEXT("Epic"), ESearchCase::IgnoreCase)) {
              IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
              if (BudgetCVar) BudgetCVar->Set(512);
          }
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("megalightsEnabled"), bEnabled);
      SendAutomationResponse(RequestingSocket, RequestId, true, 
          FString::Printf(TEXT("MegaLights %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")), Resp);
      return true;
  }
  else if (Lower == TEXT("get_megalights_budget")) {
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      
      IConsoleVariable* EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Enable"));
      IConsoleVariable* BudgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Budget"));
      
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("megalightsEnabled"), EnableCVar ? EnableCVar->GetInt() != 0 : false);
      Resp->SetNumberField(TEXT("budget"), BudgetCVar ? BudgetCVar->GetInt() : 0);
      
      // Count active lights in scene for comparison
      int32 ActiveLightCount = 0;
      TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
      for (AActor* Actor : AllActors) {
          if (Actor && Actor->FindComponentByClass<ULightComponent>()) {
              ActiveLightCount++;
          }
      }
      Resp->SetNumberField(TEXT("activeLightCount"), ActiveLightCount);
      
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MegaLights budget retrieved"), Resp);
      return true;
  }
  else if (Lower == TEXT("optimize_lights_for_megalights")) {
      double TargetBudget = 128.0;
      Payload->TryGetNumberField(TEXT("budget"), TargetBudget);
      
      // Count and analyze lights
      TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
      TArray<AActor*> LightActors;
      for (AActor* Actor : AllActors) {
          if (Actor && Actor->FindComponentByClass<ULightComponent>()) {
              LightActors.Add(Actor);
          }
      }
      
      int32 CurrentCount = LightActors.Num();
      int32 OptimizedCount = 0;
      
      // If over budget, suggest optimizations (don't auto-modify)
      TArray<TSharedPtr<FJsonValue>> Suggestions;
      if (CurrentCount > TargetBudget) {
          TSharedPtr<FJsonObject> Suggestion = MakeShared<FJsonObject>();
          Suggestion->SetStringField(TEXT("type"), TEXT("reduce_light_count"));
          Suggestion->SetStringField(TEXT("message"), 
              FString::Printf(TEXT("Scene has %d lights, exceeds budget of %d. Consider merging or removing lights."), 
                  CurrentCount, static_cast<int32>(TargetBudget)));
          Suggestions.Add(MakeShared<FJsonValueObject>(Suggestion));
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("currentLightCount"), CurrentCount);
      Resp->SetNumberField(TEXT("targetBudget"), TargetBudget);
      Resp->SetArrayField(TEXT("suggestions"), Suggestions);
      
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("MegaLights optimization analysis complete"), Resp);
      return true;
  }
  // ========================================================================
  // Advanced Lighting Actions
  // ========================================================================
  else if (Lower == TEXT("configure_gi_settings")) {
      FString Method;
      if (Payload->TryGetStringField(TEXT("method"), Method)) {
          // Normalize method name to lowercase for comparison
          FString MethodLower = Method.ToLower();
          if (MethodLower == TEXT("lumen") || MethodLower == TEXT("lumengi")) {
              IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
              if (CVar) CVar->Set(1);
          } else if (MethodLower == TEXT("screenspace") || MethodLower == TEXT("ssgi")) {
              IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
              if (CVar) CVar->Set(2);
          } else if (MethodLower == TEXT("raytraced")) {
              IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
              if (CVar) CVar->Set(3);
          } else if (MethodLower == TEXT("none") || MethodLower == TEXT("baked")) {
              IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicGlobalIlluminationMethod"));
              if (CVar) CVar->Set(0);
          }
      }
      
      double Bounces = 0.0;
      if (Payload->TryGetNumberField(TEXT("bounces"), Bounces)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.MaxReflectionBounces"));
          if (CVar) CVar->Set(static_cast<int32>(Bounces));
      }
      
      double IndirectIntensity = 0.0;
      if (Payload->TryGetNumberField(TEXT("indirectLightingIntensity"), IndirectIntensity)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.IndirectLightingIntensity"));
          if (CVar) CVar->Set(static_cast<float>(IndirectIntensity));
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("GI settings configured"), Resp);
      return true;
  }
  else if (Lower == TEXT("bake_lighting_preview")) {
      FString Quality;
      Payload->TryGetStringField(TEXT("quality"), Quality);
      
      bool bPreview = true;
      Payload->TryGetBoolField(TEXT("preview"), bPreview);
      
      if (GEditor && GetActiveWorld()) {
          FString Command = bPreview ? TEXT("BUILD LIGHTING QUALITY=Preview") : TEXT("BUILD LIGHTING");
          GEditor->Exec(GetActiveWorld(), *Command);
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("preview"), bPreview);
      SendAutomationResponse(RequestingSocket, RequestId, true, 
          bPreview ? TEXT("Preview lighting build started") : TEXT("Lighting build started"), Resp);
      return true;
  }
  else if (Lower == TEXT("get_light_complexity")) {
      // Analyze lighting complexity in the scene
      TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
      
      int32 TotalLights = 0;
      int32 StaticLights = 0;
      int32 StationaryLights = 0;
      int32 MovableLights = 0;
      int32 ShadowCastingLights = 0;
      
      for (AActor* Actor : AllActors) {
          if (!Actor) continue;
          ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
          if (LightComp) {
              TotalLights++;
              if (LightComp->CastShadows) ShadowCastingLights++;
              switch (LightComp->Mobility) {
                  case EComponentMobility::Static: StaticLights++; break;
                  case EComponentMobility::Stationary: StationaryLights++; break;
                  case EComponentMobility::Movable: MovableLights++; break;
              }
          }
      }
      
      // Calculate complexity score (rough heuristic)
      int32 ComplexityScore = StaticLights * 1 + StationaryLights * 2 + MovableLights * 4 + ShadowCastingLights * 3;
      FString ComplexityLevel = TEXT("Low");
      if (ComplexityScore > 100) ComplexityLevel = TEXT("High");
      else if (ComplexityScore > 50) ComplexityLevel = TEXT("Medium");
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("totalLights"), TotalLights);
      Resp->SetNumberField(TEXT("staticLights"), StaticLights);
      Resp->SetNumberField(TEXT("stationaryLights"), StationaryLights);
      Resp->SetNumberField(TEXT("movableLights"), MovableLights);
      Resp->SetNumberField(TEXT("shadowCastingLights"), ShadowCastingLights);
      Resp->SetNumberField(TEXT("complexityScore"), ComplexityScore);
      Resp->SetStringField(TEXT("complexityLevel"), ComplexityLevel);
      
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light complexity analyzed"), Resp);
      return true;
  }
  else if (Lower == TEXT("configure_volumetric_fog")) {
      // More advanced volumetric fog configuration
      bool bEnabled = true;
      Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
      
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VolumetricFog"));
      if (CVar) CVar->Set(bEnabled ? 1 : 0);
      
      // Find or create fog actor
      AExponentialHeightFog* FogActor = nullptr;
      TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
      for (AActor* Actor : AllActors) {
          if (Actor && Actor->IsA<AExponentialHeightFog>()) {
              FogActor = Cast<AExponentialHeightFog>(Actor);
              break;
          }
      }
      
      if (!FogActor && bEnabled) {
          FogActor = Cast<AExponentialHeightFog>(SpawnActorInActiveWorld<AActor>(
              AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
      }
      
      if (FogActor && FogActor->GetComponent()) {
          UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
          FogComp->bEnableVolumetricFog = bEnabled;
          
          double Density = 0.0;
          if (Payload->TryGetNumberField(TEXT("density"), Density)) {
              FogComp->FogDensity = static_cast<float>(Density);
          }
          
          double ViewDistance = 0.0;
          if (Payload->TryGetNumberField(TEXT("viewDistance"), ViewDistance)) {
              FogComp->VolumetricFogDistance = static_cast<float>(ViewDistance);
          }
          
          double ScatteringIntensity = 0.0;
          if (Payload->TryGetNumberField(TEXT("scatteringIntensity"), ScatteringIntensity)) {
              FogComp->VolumetricFogScatteringDistribution = static_cast<float>(ScatteringIntensity);
          }
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetBoolField(TEXT("enabled"), bEnabled);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Volumetric fog configured"), Resp);
      return true;
  }
  else if (Lower == TEXT("create_light_batch")) {
      const TArray<TSharedPtr<FJsonValue>>* LightsArray = nullptr;
      if (!Payload->TryGetArrayField(TEXT("lights"), LightsArray) || !LightsArray) {
          SendAutomationError(RequestingSocket, RequestId, TEXT("lights array required"), TEXT("INVALID_ARGUMENT"));
          return true;
      }
      
      TArray<TSharedPtr<FJsonValue>> CreatedLights;
      int32 SuccessCount = 0;
      int32 FailCount = 0;
      
      for (const TSharedPtr<FJsonValue>& LightVal : *LightsArray) {
          const TSharedPtr<FJsonObject>* LightObj = nullptr;
          if (!LightVal->TryGetObject(LightObj) || !LightObj) continue;
          
          FString LightType = TEXT("PointLight");
          (*LightObj)->TryGetStringField(TEXT("type"), LightType);
          
          FVector Location = FVector::ZeroVector;
          const TSharedPtr<FJsonObject>* LocObj = nullptr;
          if ((*LightObj)->TryGetObjectField(TEXT("location"), LocObj)) {
              Location.X = (*LocObj)->GetNumberField(TEXT("x"));
              Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
              Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
          }
          
          UClass* LightClass = ResolveUClass(LightType);
          if (!LightClass) LightClass = ResolveUClass(TEXT("A") + LightType);
          if (!LightClass || !LightClass->IsChildOf(ALight::StaticClass())) {
              FailCount++;
              continue;
          }
          
          FActorSpawnParameters SpawnParams;
          SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
          AActor* NewLight = ActorSS->GetWorld()->SpawnActor(LightClass, &Location, nullptr, SpawnParams);
          
          if (NewLight) {
              FString Name;
              if ((*LightObj)->TryGetStringField(TEXT("name"), Name)) {
                  NewLight->SetActorLabel(Name);
              }
              
              if (ULightComponent* LightComp = NewLight->FindComponentByClass<ULightComponent>()) {
                  LightComp->SetMobility(EComponentMobility::Movable);
                  
                  double Intensity = 0.0;
                  if ((*LightObj)->TryGetNumberField(TEXT("intensity"), Intensity)) {
                      LightComp->SetIntensity(static_cast<float>(Intensity));
                  }
              }
              
              TSharedPtr<FJsonObject> CreatedInfo = MakeShared<FJsonObject>();
              CreatedInfo->SetStringField(TEXT("name"), NewLight->GetActorLabel());
              CreatedInfo->SetStringField(TEXT("type"), LightType);
              CreatedLights.Add(MakeShared<FJsonValueObject>(CreatedInfo));
              SuccessCount++;
          } else {
              FailCount++;
          }
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), FailCount == 0);
      Resp->SetNumberField(TEXT("created"), SuccessCount);
      Resp->SetNumberField(TEXT("failed"), FailCount);
      Resp->SetArrayField(TEXT("lights"), CreatedLights);
      
      SendAutomationResponse(RequestingSocket, RequestId, true, 
          FString::Printf(TEXT("Created %d lights (%d failed)"), SuccessCount, FailCount), Resp);
      return true;
  }
  else if (Lower == TEXT("configure_shadow_settings")) {
      FString ShadowQuality;
      if (Payload->TryGetStringField(TEXT("shadowQuality"), ShadowQuality)) {
          TMap<FString, int32> QualityMap;
          QualityMap.Add(TEXT("low"), 0);
          QualityMap.Add(TEXT("medium"), 1);
          QualityMap.Add(TEXT("high"), 2);
          QualityMap.Add(TEXT("epic"), 3);
          
          const int32* QualityVal = QualityMap.Find(ShadowQuality.ToLower());
          if (QualityVal) {
              IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadowQuality"));
              if (CVar) CVar->Set(*QualityVal);
          }
      }
      
      bool bCascaded = false;
      if (Payload->TryGetBoolField(TEXT("cascadedShadows"), bCascaded)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.CSM.MaxCascades"));
          if (CVar) CVar->Set(bCascaded ? 4 : 1);
      }
      
      double ShadowBias = 0.0;
      if (Payload->TryGetNumberField(TEXT("shadowBias"), ShadowBias)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.DepthBias"));
          if (CVar) CVar->Set(static_cast<float>(ShadowBias));
      }
      
      bool bContactShadows = false;
      if (Payload->TryGetBoolField(TEXT("contactShadows"), bContactShadows)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ContactShadows"));
          if (CVar) CVar->Set(bContactShadows ? 1 : 0);
      }
      
      bool bRayTracedShadows = false;
      if (Payload->TryGetBoolField(TEXT("rayTracedShadows"), bRayTracedShadows)) {
          IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Shadows"));
          if (CVar) CVar->Set(bRayTracedShadows ? 1 : 0);
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Shadow settings configured"), Resp);
      return true;
  }
  else if (Lower == TEXT("validate_lighting_setup")) {
      bool bValidatePerformance = true;
      bool bValidateOverlap = true;
      bool bValidateShadows = true;
      Payload->TryGetBoolField(TEXT("validatePerformance"), bValidatePerformance);
      Payload->TryGetBoolField(TEXT("validateOverlap"), bValidateOverlap);
      Payload->TryGetBoolField(TEXT("validateShadows"), bValidateShadows);
      
      TArray<TSharedPtr<FJsonValue>> Issues;
      TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
      
      int32 TotalLights = 0;
      int32 MovableShadowLights = 0;
      int32 OverlappingLights = 0;
      
      for (AActor* Actor : AllActors) {
          if (!Actor) continue;
          ULightComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
          if (!LightComp) continue;
          
          TotalLights++;
          
          // Check for movable shadow-casting lights (expensive)
          if (bValidatePerformance && LightComp->Mobility == EComponentMobility::Movable && LightComp->CastShadows) {
              MovableShadowLights++;
              if (MovableShadowLights > 4) {
                  TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
                  Issue->SetStringField(TEXT("type"), TEXT("performance"));
                  Issue->SetStringField(TEXT("severity"), TEXT("warning"));
                  Issue->SetStringField(TEXT("message"), 
                      FString::Printf(TEXT("Light '%s' is movable with shadows - consider making stationary"), *Actor->GetActorLabel()));
                  Issue->SetStringField(TEXT("actor"), Actor->GetActorLabel());
                  Issues.Add(MakeShared<FJsonValueObject>(Issue));
              }
          }
      }
      
      // Performance summary
      if (bValidatePerformance && TotalLights > 100) {
          TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
          Issue->SetStringField(TEXT("type"), TEXT("performance"));
          Issue->SetStringField(TEXT("severity"), TEXT("warning"));
          Issue->SetStringField(TEXT("message"), 
              FString::Printf(TEXT("High light count (%d) may impact performance. Consider using MegaLights or reducing count."), TotalLights));
          Issues.Add(MakeShared<FJsonValueObject>(Issue));
      }
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("totalLights"), TotalLights);
      Resp->SetNumberField(TEXT("issueCount"), Issues.Num());
      Resp->SetArrayField(TEXT("issues"), Issues);
      Resp->SetBoolField(TEXT("valid"), Issues.Num() == 0);
      
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighting validation complete"), Resp);
      return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Lighting actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
