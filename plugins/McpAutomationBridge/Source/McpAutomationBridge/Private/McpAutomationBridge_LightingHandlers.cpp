#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "UObject/UObjectIterator.h"

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
#include "GameFramework/WorldSettings.h"
#include "Lightmass/LightmassImportanceVolume.h"

/* UE5.6: LightingBuildOptions.h removed; use console exec */
#include "Editor/UnrealEd/Public/Editor.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "RenderingThread.h"  // FlushRenderingCommands for safe spawning

#endif

bool UMcpAutomationBridgeSubsystem::HandleLightingAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("spawn_light")) &&
      !Lower.StartsWith(TEXT("spawn_sky_light")) &&
      !Lower.StartsWith(TEXT("create_sky_light")) &&       // Added: was missing
      !Lower.StartsWith(TEXT("create_light")) &&          // Added: alias for spawn_light
      !Lower.StartsWith(TEXT("build_lighting")) &&
      !Lower.StartsWith(TEXT("bake_lightmap")) &&         // Added: was missing
      !Lower.StartsWith(TEXT("ensure_single_sky_light")) &&
      !Lower.StartsWith(TEXT("create_lighting_enabled_level")) &&
      !Lower.StartsWith(TEXT("create_lightmass_volume")) &&
      !Lower.StartsWith(TEXT("create_dynamic_light")) &&  // Added: was missing
      !Lower.StartsWith(TEXT("setup_volumetric_fog")) &&
      !Lower.StartsWith(TEXT("setup_global_illumination")) &&
      !Lower.StartsWith(TEXT("configure_shadows")) &&
      !Lower.StartsWith(TEXT("set_exposure")) &&
      !Lower.StartsWith(TEXT("list_light_types")) &&
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

    for (TObjectIterator<UClass> It; It; ++It) {
      if (It->IsChildOf(ALight::StaticClass()) &&
          !It->HasAnyClassFlags(CLASS_Abstract) &&
          !AddedNames.Contains(It->GetName())) {
        Types.Add(MakeShared<FJsonValueString>(It->GetName()));
        AddedNames.Add(It->GetName());
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
    
    // Support multiple parameter names: lightClass, lightType, type
    // Priority: lightClass > lightType > type
    if (!Payload->TryGetStringField(TEXT("lightClass"), LightClassStr) || LightClassStr.IsEmpty()) {
      // Try lightType parameter - map short names to Unreal class names
      FString LightType;
      if (Payload->TryGetStringField(TEXT("lightType"), LightType) && !LightType.IsEmpty()) {
        const FString LowerType = LightType.ToLower();
        if (LowerType == TEXT("point") || LowerType == TEXT("pointlight")) {
          LightClassStr = TEXT("PointLight");
        } else if (LowerType == TEXT("directional") || LowerType == TEXT("directionallight")) {
          LightClassStr = TEXT("DirectionalLight");
        } else if (LowerType == TEXT("spot") || LowerType == TEXT("spotlight")) {
          LightClassStr = TEXT("SpotLight");
        } else if (LowerType == TEXT("rect") || LowerType == TEXT("rectlight")) {
          LightClassStr = TEXT("RectLight");
        } else if (LowerType == TEXT("sky") || LowerType == TEXT("skylight")) {
          LightClassStr = TEXT("SkyLight");
        } else {
          // Unknown light type
          SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Invalid lightType: %s. Must be one of: point, directional, spot, rect, sky"), *LightType),
                              TEXT("INVALID_LIGHT_TYPE"));
          return true;
        }
      }
      // CRITICAL FIX: Also check for 'type' parameter (common shorthand)
      else if (Payload->TryGetStringField(TEXT("type"), LightType) && !LightType.IsEmpty()) {
        const FString LowerType = LightType.ToLower();
        if (LowerType == TEXT("point") || LowerType == TEXT("pointlight")) {
          LightClassStr = TEXT("PointLight");
        } else if (LowerType == TEXT("directional") || LowerType == TEXT("directionallight")) {
          LightClassStr = TEXT("DirectionalLight");
        } else if (LowerType == TEXT("spot") || LowerType == TEXT("spotlight")) {
          LightClassStr = TEXT("SpotLight");
        } else if (LowerType == TEXT("rect") || LowerType == TEXT("rectlight")) {
          LightClassStr = TEXT("RectLight");
        } else if (LowerType == TEXT("sky") || LowerType == TEXT("skylight")) {
          LightClassStr = TEXT("SkyLight");
        } else {
          // Unknown light type
          SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Invalid type: %s. Must be one of: point, directional, spot, rect, sky"), *LightType),
                              TEXT("INVALID_LIGHT_TYPE"));
          return true;
        }
      }
    }
    
    if (LightClassStr.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("lightClass or lightType required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // CRITICAL: Use explicit StaticClass() for native light types to avoid
    // ResolveUClass resolution issues where TObjectIterator may return wrong class.
    // This ensures SpotLight, DirectionalLight, etc. spawn correctly.
    UClass *LightClass = nullptr;
    const FString LowerClassStr = LightClassStr.ToLower();
    
    if (LowerClassStr == TEXT("pointlight") || LowerClassStr == TEXT("point")) {
      LightClass = APointLight::StaticClass();
    } else if (LowerClassStr == TEXT("directionallight") || LowerClassStr == TEXT("directional")) {
      LightClass = ADirectionalLight::StaticClass();
    } else if (LowerClassStr == TEXT("spotlight") || LowerClassStr == TEXT("spot")) {
      LightClass = ASpotLight::StaticClass();
    } else if (LowerClassStr == TEXT("rectlight") || LowerClassStr == TEXT("rect")) {
      LightClass = ARectLight::StaticClass();
    } else if (LowerClassStr == TEXT("skylight") || LowerClassStr == TEXT("sky")) {
      LightClass = ASkyLight::StaticClass();
    } else {
      // Fallback: Dynamic resolution with heuristics for custom light types
      LightClass = ResolveUClass(LightClassStr);
      
      // Try finding with 'A' prefix (standard Actor prefix)
      if (!LightClass) {
        LightClass = ResolveUClass(TEXT("A") + LightClassStr);
      }
    }

    if (!LightClass || !LightClass->IsChildOf(ALight::StaticClass())) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid light class: %s"), *LightClassStr),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("spawn_light: Resolved lightClass '%s' to %s (path: %s)"),
           *LightClassStr, *LightClass->GetName(), *LightClass->GetPathName());

    // Default location to a reasonable height above ground (z=300) to avoid burying lights in geometry
    // User can override by providing explicit location
    FVector Location = FVector(0.0f, 0.0f, 300.0f);
    const TSharedPtr<FJsonObject> *LocPtr;
    bool bHasExplicitLocation = Payload->TryGetObjectField(TEXT("location"), LocPtr);
    if (bHasExplicitLocation) {
      Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
      Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
      Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    } else {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("spawn_light: No location provided, using default (0, 0, 300)"));
    }

    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr)) {
      Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
      Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
      Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // CRITICAL FIX: Validate world before spawning to prevent crashes
    UWorld* World = ActorSS->GetWorld();
    if (!World || !World->IsValidLowLevel()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No valid world available for spawning light"),
                          TEXT("NO_WORLD"));
      return true;
    }

    // CRITICAL FIX: Flush rendering commands to prevent GPU driver crashes
    // during spawn operations (especially Intel MONZA drivers)
    FlushRenderingCommands();

    // CRITICAL FIX: Use SpawnActorDeferred for safer initialization
    FTransform SpawnTransform(Rotation, Location);
    AActor *NewLight = World->SpawnActorDeferred<AActor>(
        LightClass,
        SpawnTransform,
        nullptr,    // Owner
        nullptr,    // Instigator
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    // CRITICAL FIX: Finish spawning with proper transform
    if (NewLight) {
        UGameplayStatics::FinishSpawningActor(NewLight, SpawnTransform);
    }

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

    // Apply properties with validation
    const TSharedPtr<FJsonObject> *Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props)) {
      ULightComponent *LightComp =
          NewLight->FindComponentByClass<ULightComponent>();
      if (LightComp) {
        double Intensity;
        if ((*Props)->TryGetNumberField(TEXT("intensity"), Intensity)) {
          // Validate intensity: must be finite and non-negative
          if (!FMath::IsFinite(Intensity)) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("spawn_light: Invalid intensity (not finite), using 0"));
            Intensity = 0.0;
          } else if (Intensity < 0.0) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("spawn_light: Negative intensity %.2f clamped to 0"), Intensity);
            Intensity = 0.0;
          }
          LightComp->SetIntensity((float)Intensity);
        }

        const TSharedPtr<FJsonObject> *ColorObj;
        if ((*Props)->TryGetObjectField(TEXT("color"), ColorObj)) {
          FLinearColor Color;
          Color.R = GetJsonNumberField((*ColorObj), TEXT("r"));
          Color.G = GetJsonNumberField((*ColorObj), TEXT("g"));
          Color.B = GetJsonNumberField((*ColorObj), TEXT("b"));
          Color.A = (*ColorObj)->HasField(TEXT("a"))
                        ? GetJsonNumberField((*ColorObj), TEXT("a"))
                        : 1.0f;
          // Validate color components: must be finite
          if (!FMath::IsFinite(Color.R) || !FMath::IsFinite(Color.G) || 
              !FMath::IsFinite(Color.B) || !FMath::IsFinite(Color.A)) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                   TEXT("spawn_light: Invalid color components, using white"));
            Color = FLinearColor::White;
          }
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
            // Validate radius: must be positive and finite
            if (!FMath::IsFinite(Radius) || Radius <= 0.0) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_light: Invalid attenuationRadius %.2f, using 1000"), Radius);
              Radius = 1000.0;
            }
            PointComp->SetAttenuationRadius((float)Radius);
          }
        }

        if (USpotLightComponent *SpotComp =
                Cast<USpotLightComponent>(LightComp)) {
          double InnerCone;
          if ((*Props)->TryGetNumberField(TEXT("innerConeAngle"), InnerCone)) {
            // Validate cone angle: 0-180 degrees
            if (!FMath::IsFinite(InnerCone) || InnerCone < 0.0 || InnerCone > 180.0) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_light: Invalid innerConeAngle %.2f, clamping to 0-180"), InnerCone);
              InnerCone = FMath::Clamp(InnerCone, 0.0, 180.0);
            }
            SpotComp->SetInnerConeAngle((float)InnerCone);
          }
          double OuterCone;
          if ((*Props)->TryGetNumberField(TEXT("outerConeAngle"), OuterCone)) {
            // Validate cone angle: 0-180 degrees
            if (!FMath::IsFinite(OuterCone) || OuterCone < 0.0 || OuterCone > 180.0) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_light: Invalid outerConeAngle %.2f, clamping to 0-180"), OuterCone);
              OuterCone = FMath::Clamp(OuterCone, 0.0, 180.0);
            }
            SpotComp->SetOuterConeAngle((float)OuterCone);
          }
        }

        if (URectLightComponent *RectComp =
                Cast<URectLightComponent>(LightComp)) {
          double Width;
          if ((*Props)->TryGetNumberField(TEXT("sourceWidth"), Width)) {
            // Validate width: must be positive
            if (!FMath::IsFinite(Width) || Width <= 0.0) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_light: Invalid sourceWidth %.2f, using 100"), Width);
              Width = 100.0;
            }
            RectComp->SetSourceWidth((float)Width);
          }
          double Height;
          if ((*Props)->TryGetNumberField(TEXT("sourceHeight"), Height)) {
            // Validate height: must be positive
            if (!FMath::IsFinite(Height) || Height <= 0.0) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_light: Invalid sourceHeight %.2f, using 100"), Height);
              Height = 100.0;
            }
            RectComp->SetSourceHeight((float)Height);
          }
        }
      }
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), NewLight->GetActorLabel());

    // Add verification data
    AddActorVerification(Resp, NewLight);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Light spawned"), Resp);
    return true;
  } else if (Lower == TEXT("spawn_sky_light") || Lower == TEXT("create_sky_light")) {
    // Default location to a reasonable height (z=500) for sky lights
    // User can override by providing explicit location
    FVector Location = FVector(0.0f, 0.0f, 500.0f);
    const TSharedPtr<FJsonObject> *LocPtr;
    bool bHasExplicitLocation = Payload->TryGetObjectField(TEXT("location"), LocPtr);
    if (bHasExplicitLocation) {
      Location.X = GetJsonNumberField((*LocPtr), TEXT("x"));
      Location.Y = GetJsonNumberField((*LocPtr), TEXT("y"));
      Location.Z = GetJsonNumberField((*LocPtr), TEXT("z"));
    } else {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("spawn_sky_light: No location provided, using default (0, 0, 500)"));
    }
    
    // Parse rotation from payload (optional, defaults to ZeroRotator)
    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject> *RotPtr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotPtr)) {
      Rotation.Pitch = GetJsonNumberField((*RotPtr), TEXT("pitch"));
      Rotation.Yaw = GetJsonNumberField((*RotPtr), TEXT("yaw"));
      Rotation.Roll = GetJsonNumberField((*RotPtr), TEXT("roll"));
    }
    
    AActor *SkyLight = SpawnActorInActiveWorld<AActor>(
        ASkyLight::StaticClass(), Location, Rotation);
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
            // Security: Validate cubemap path to prevent traversal attacks
            FString SanitizedCubemapPath = SanitizeProjectRelativePath(CubemapPath);
            if (SanitizedCubemapPath.IsEmpty()) {
              UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                     TEXT("spawn_sky_light: Invalid cubemapPath rejected: %s"), *CubemapPath);
              // Don't fail the spawn, just skip the cubemap
            } else {
              UTextureCube *Cubemap = Cast<UTextureCube>(StaticLoadObject(
                  UTextureCube::StaticClass(), nullptr, *SanitizedCubemapPath));
              if (Cubemap) {
                SkyComp->Cubemap = Cubemap;
              }
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
    
    // Add verification data
    AddActorVerification(Resp, SkyLight);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("SkyLight spawned"), Resp);
    return true;
  } else if (Lower == TEXT("build_lighting") || Lower == TEXT("bake_lightmap")) {
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UWorld* World = GEditor->GetEditorWorldContext().World();
      
      // Check if precomputed lighting is disabled in WorldSettings
      if (AWorldSettings* WS = World->GetWorldSettings()) {
        if (WS->bForceNoPrecomputedLighting) {
          TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
          Resp->SetBoolField(TEXT("skipped"), true);
          Resp->SetStringField(TEXT("reason"), TEXT("bForceNoPrecomputedLighting is true"));
          Resp->SetStringField(TEXT("suggestion"), TEXT("Set WorldSettings.bForceNoPrecomputedLighting to false to enable lighting builds"));
          // Return success=false since the operation did NOT complete
          SendAutomationResponse(RequestingSocket, RequestId, false,
              TEXT("Lighting build skipped - precomputed lighting disabled in WorldSettings"), Resp,
              TEXT("OPERATION_SKIPPED"));
          return true;
        }
      }
      
      // Read quality parameter
      FString Quality;
      Payload->TryGetStringField(TEXT("quality"), Quality);
      
      // Map quality string to console command
      FString QualityCmd = TEXT("Production"); // Default
      if (!Quality.IsEmpty()) {
        const FString LowerQuality = Quality.ToLower();
        if (LowerQuality == TEXT("preview") || LowerQuality == TEXT("0")) {
          QualityCmd = TEXT("Preview");
        } else if (LowerQuality == TEXT("medium") || LowerQuality == TEXT("1")) {
          QualityCmd = TEXT("Medium");
        } else if (LowerQuality == TEXT("high") || LowerQuality == TEXT("2")) {
          QualityCmd = TEXT("High");
        } else if (LowerQuality == TEXT("production") || LowerQuality == TEXT("3")) {
          QualityCmd = TEXT("Production");
        } else {
          // Unknown quality - return error
          TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
          Err->SetStringField(TEXT("error"), TEXT("unknown_quality"));
          Err->SetStringField(TEXT("quality"), Quality);
          Err->SetStringField(TEXT("validValues"), TEXT("preview/0, medium/1, high/2, production/3"));
          SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("Unknown lighting quality"), Err,
                                 TEXT("UNKNOWN_QUALITY"));
          return true;
        }
      }
      
      FString Command = FString::Printf(TEXT("BuildLighting %s"), *QualityCmd);
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);
      
      TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
      Resp->SetStringField(TEXT("quality"), QualityCmd);
      Resp->SetBoolField(TEXT("started"), true);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Lighting build started with quality: %s"), *QualityCmd), Resp);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor world not available"),
                          TEXT("EDITOR_WORLD_NOT_AVAILABLE"));
    }
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

    // Two-pass approach: first find exact name match, then destroy others
    for (AActor *SkyLight : SkyLights) {
      if (SkyLight->GetActorLabel() == TargetName && !TargetName.IsEmpty()) {
        KeptActor = SkyLight;
        break;
      }
    }
    
    // If no exact match, keep first and destroy rest
    for (AActor *SkyLight : SkyLights) {
      // Skip the actor we want to keep
      if (SkyLight == KeptActor) {
        continue;
      }
      if (!KeptActor) {
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

    // Add verification data for the kept/spawned SkyLight
    if (KeptActor) {
      AddActorVerification(Resp, KeptActor);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Ensured single SkyLight"), Resp);
    return true;
  } else if (Lower == TEXT("create_lightmass_volume")) {
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject> *LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
      Location.X = GetJsonNumberField((*LocObj), TEXT("x"));
      Location.Y = GetJsonNumberField((*LocObj), TEXT("y"));
      Location.Z = GetJsonNumberField((*LocObj), TEXT("z"));
    }

    FVector Size = FVector(1000, 1000, 1000);
    const TSharedPtr<FJsonObject> *SizeObj;
    if (Payload->TryGetObjectField(TEXT("size"), SizeObj)) {
      Size.X = GetJsonNumberField((*SizeObj), TEXT("x"));
      Size.Y = GetJsonNumberField((*SizeObj), TEXT("y"));
      Size.Z = GetJsonNumberField((*SizeObj), TEXT("z"));
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

      // Add verification data
      AddActorVerification(Resp, Volume);

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

      // Add verification data
      AddActorVerification(Resp, FogActor);

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
    if (!Payload->TryGetStringField(TEXT("method"), Method) || Method.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
          TEXT("method parameter is required. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    
    bool bValidMethod = false;
    if (Method == TEXT("LumenGI")) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.DynamicGlobalIlluminationMethod"));
      if (CVar)
        CVar->Set(1); // 1 = Lumen

      IConsoleVariable *CVarRefl = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.ReflectionMethod"));
      if (CVarRefl)
        CVarRefl->Set(1); // 1 = Lumen
      bValidMethod = true;
    } else if (Method == TEXT("ScreenSpace")) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.DynamicGlobalIlluminationMethod"));
      if (CVar)
        CVar->Set(2); // SSGI
      bValidMethod = true;
    } else if (Method == TEXT("None")) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.DynamicGlobalIlluminationMethod"));
      if (CVar)
        CVar->Set(0);
      bValidMethod = true;
    } else if (Method == TEXT("RayTraced")) {
      IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.DynamicGlobalIlluminationMethod"));
      if (CVar)
        CVar->Set(3); // 3 = RayTraced (if supported)
      bValidMethod = true;
    } else if (Method == TEXT("Lightmass")) {
      // Lightmass requires disabling Lumen and enabling static lighting
      IConsoleVariable *CVarGI = IConsoleManager::Get().FindConsoleVariable(
          TEXT("r.DynamicGlobalIlluminationMethod"));
      if (CVarGI)
        CVarGI->Set(0); // Disable dynamic GI to use baked
      bValidMethod = true;
    } else {
      SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid GI method: %s. Valid values: LumenGI, ScreenSpace, None, RayTraced, Lightmass"), *Method),
          TEXT("INVALID_GI_METHOD"));
      return true;
    }
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), bValidMethod);
    Resp->SetStringField(TEXT("method"), Method);
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           FString::Printf(TEXT("GI method configured: %s"), *Method), Resp);
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

      // Add verification data
      AddActorVerification(Resp, PPV);

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
      
      // Add verification data
      AddActorVerification(Resp, PPV);
      
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

    // Security: Validate path to prevent traversal attacks
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid path: contains traversal or invalid characters"),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Path = SanitizedPath;

    if (GEditor) {
      // Create a new blank map
      GEditor->NewMap();

      // Add basic lighting
      SpawnActorInActiveWorld<AActor>(ADirectionalLight::StaticClass(),
                                      FVector(0, 0, 500), FRotator(-45, 0, 0),
                                      TEXT("Sun"));
      SpawnActorInActiveWorld<AActor>(ASkyLight::StaticClass(),
                                      FVector::ZeroVector,
                                      FRotator::ZeroRotator, TEXT("SkyLight"));

      // Save the level using McpSafeLevelSave to prevent Intel GPU driver crashes
      // Explicitly use 5 retries for Intel GPU resilience (max 7.75s total retry time)
      bool bSaved = McpSafeLevelSave(
          GEditor->GetEditorWorldContext().World()->PersistentLevel, *Path, 5);
      if (bSaved) {
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), Path);
        Resp->SetStringField(TEXT("message"),
                             TEXT("Level created with lighting"));
        
        // Add verification data
        Resp->SetBoolField(TEXT("existsAfter"), true);
        Resp->SetStringField(TEXT("levelPath"), Path);
        
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
