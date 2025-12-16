#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
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
  if (!Lower.StartsWith(TEXT("spawn_light")) &&
      !Lower.StartsWith(TEXT("spawn_sky_light")) &&
      !Lower.StartsWith(TEXT("build_lighting")) &&
      !Lower.StartsWith(TEXT("ensure_single_sky_light")) &&
      !Lower.StartsWith(TEXT("create_lighting_enabled_level")) &&
      !Lower.StartsWith(TEXT("create_lightmass_volume")) &&
      !Lower.StartsWith(TEXT("setup_volumetric_fog")) &&
      !Lower.StartsWith(TEXT("setup_global_illumination")) &&
      !Lower.StartsWith(TEXT("configure_shadows")) &&
      !Lower.StartsWith(TEXT("set_exposure")) &&
      !Lower.StartsWith(TEXT("set_ambient_occlusion"))) {
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

  if (Lower == TEXT("spawn_light")) {
    FString LightClassStr;
    if (!Payload->TryGetStringField(TEXT("lightClass"), LightClassStr) ||
        LightClassStr.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("lightClass required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UClass *LightClass = nullptr;
    if (LightClassStr == TEXT("DirectionalLight"))
      LightClass = ADirectionalLight::StaticClass();
    else if (LightClassStr == TEXT("PointLight"))
      LightClass = APointLight::StaticClass();
    else if (LightClassStr == TEXT("SpotLight"))
      LightClass = ASpotLight::StaticClass();
    else if (LightClassStr == TEXT("RectLight"))
      LightClass = ARectLight::StaticClass();
    else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown light class: %s"), *LightClassStr),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
      Location.X = (*LocObj)->GetNumberField(TEXT("x"));
      Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
      Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotObj;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj)) {
      Rotation.Pitch = (*RotObj)->GetNumberField(TEXT("pitch"));
      Rotation.Yaw = (*RotObj)->GetNumberField(TEXT("yaw"));
      Rotation.Roll = (*RotObj)->GetNumberField(TEXT("roll"));
    }

    AActor *NewLight = nullptr;
    UWorld *TargetWorld = (GEditor->PlayWorld) ? GEditor->PlayWorld : nullptr;

    if (TargetWorld) {
      // PIE Path
      FActorSpawnParameters SpawnParams;
      SpawnParams.SpawnCollisionHandlingOverride =
          ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
      NewLight = TargetWorld->SpawnActor(LightClass, &Location, &Rotation,
                                         SpawnParams);
    } else {
      // Editor Path
      NewLight = ActorSS->SpawnActorFromClass(LightClass, Location, Rotation);
      // Explicitly set location/rotation
      if (NewLight) {
        NewLight->SetActorLocationAndRotation(
            Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
      }
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
  } else if (Lower == TEXT("spawn_sky_light")) {
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
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      if (GEditor && GEditor->GetEditorWorldContext().World())
        GEditor->Exec(GEditor->GetEditorWorldContext().World(),
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
      SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                      FVector(0, 0, 500), FRotator(-45, 0, 0),
                                      TEXT("Sun"));
      SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                      FVector::ZeroVector,
                                      FRotator::ZeroRotator, TEXT("SkyLight"));

      // Save the level
      bool bSaved = FEditorFileUtils::SaveLevel(
          GEditor->GetEditorWorldContext().World()->PersistentLevel, *Path);
      if (bSaved) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), Path);
        Resp->SetStringField(TEXT("message"),
                             TEXT("Level created with lighting"));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Level created with lighting"), Resp);
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
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Lighting actions require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
