// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 28: Weather System Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

// Wind Component
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/WindDirectionalSource.h"

// Directional Light for sun position / time of day
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"

// Niagara for particle effects (rain/snow)
#if __has_include("NiagaraComponent.h")
#define MCP_HAS_NIAGARA 1
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#else
#define MCP_HAS_NIAGARA 0
#endif

// Post Process for lightning flash effects
#include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"

#endif // WITH_EDITOR

// ============================================================================
// Helper function for efficient actor lookup by label
// Uses TActorIterator to avoid O(N) GetAllLevelActors()
// ============================================================================
namespace {
#if WITH_EDITOR
static AActor* FindActorByLabel(UWorld* World, const FString& ActorLabel)
{
    if (!World || ActorLabel.IsEmpty()) return nullptr;
    
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->GetActorLabel().Equals(ActorLabel, ESearchCase::IgnoreCase))
        {
            return Actor;
        }
    }
    return nullptr;
}
#endif
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleWeatherAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_weather"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_weather")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_weather payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Weather action '%s' completed"), *LowerSub);
  FString ErrorCode;

  if (!GEditor) {
    bSuccess = false;
    Message = TEXT("Editor not available");
    ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    bSuccess = false;
    Message = TEXT("EditorActorSubsystem not available");
    ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    bSuccess = false;
    Message = TEXT("Editor world not available");
    ErrorCode = TEXT("WORLD_NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
  }

  // ========================================================================
  // CONFIGURE WIND
  // ========================================================================
  if (LowerSub == TEXT("configure_wind")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    // Find existing WindDirectionalSource actor (optionally filtered by name)
    AActor *WindActor = nullptr;
    for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
      if (AWindDirectionalSource *Wind = *It) {
        if (ActorName.IsEmpty() || Wind->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
          WindActor = Wind;
          break;
        }
      }
    }

    if (!WindActor) {
      // Spawn new WindDirectionalSource if not found
      UClass *WindClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.WindDirectionalSource"));
      if (WindClass) {
        FVector Location(0, 0, 0);
        const TSharedPtr<FJsonObject> *LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
          (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
          (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
          (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
        WindActor = SpawnActorInActiveWorld<AActor>(
            WindClass, Location, FRotator::ZeroRotator,
            ActorName.IsEmpty() ? TEXT("WindDirectionalSource") : *ActorName);
      }
    }

    if (WindActor) {
      UWindDirectionalSourceComponent *WindComp = WindActor->FindComponentByClass<UWindDirectionalSourceComponent>();
      if (WindComp) {
        int32 PropertiesSet = 0;

        // Strength
        double Strength = 0.0;
        if (Payload->TryGetNumberField(TEXT("strength"), Strength)) {
          WindComp->SetStrength(static_cast<float>(Strength));
          PropertiesSet++;
        }

        // Speed
        double Speed = 0.0;
        if (Payload->TryGetNumberField(TEXT("speed"), Speed)) {
          WindComp->SetSpeed(static_cast<float>(Speed));
          PropertiesSet++;
        }

        // Min Gust Amount
        double MinGustAmount = 0.0;
        if (Payload->TryGetNumberField(TEXT("minGustAmount"), MinGustAmount)) {
          WindComp->SetMinimumGustAmount(static_cast<float>(MinGustAmount));
          PropertiesSet++;
        }

        // Max Gust Amount
        double MaxGustAmount = 0.0;
        if (Payload->TryGetNumberField(TEXT("maxGustAmount"), MaxGustAmount)) {
          WindComp->SetMaximumGustAmount(static_cast<float>(MaxGustAmount));
          PropertiesSet++;
        }

        // Radius (for point wind)
        double Radius = 0.0;
        if (Payload->TryGetNumberField(TEXT("radius"), Radius)) {
          WindComp->SetRadius(static_cast<float>(Radius));
          PropertiesSet++;
        }

        // Wind Type (directional or point)
        FString WindType;
        if (Payload->TryGetStringField(TEXT("windType"), WindType)) {
          if (WindType.ToLower() == TEXT("point")) {
            WindComp->SetWindType(EWindSourceType::Point);
          } else {
            WindComp->SetWindType(EWindSourceType::Directional);
          }
          PropertiesSet++;
        }

        // Rotation (wind direction)
        const TSharedPtr<FJsonObject> *RotObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj) {
          double Pitch = 0, Yaw = 0, Roll = 0;
          (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
          (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
          (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
          WindActor->SetActorRotation(FRotator(Pitch, Yaw, Roll));
          PropertiesSet++;
        }

        bSuccess = true;
        Message = FString::Printf(TEXT("Wind configured with %d properties"), PropertiesSet);
        Resp->SetStringField(TEXT("actorName"), WindActor->GetActorLabel());
        Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
      } else {
        bSuccess = false;
        Message = TEXT("WindDirectionalSourceComponent not found on actor");
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to find or create WindDirectionalSource actor");
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE WEATHER SYSTEM (Master Controller Blueprint)
  // ========================================================================
  else if (LowerSub == TEXT("create_weather_system")) {
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty()) {
      Name = TEXT("WeatherSystem");
    }

    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }

    // Create an empty actor to serve as weather system controller
    // Users can attach Blueprint logic or Niagara systems to this
    AActor *WeatherActor = SpawnActorInActiveWorld<AActor>(
        AActor::StaticClass(), Location, FRotator::ZeroRotator, *Name);

    if (WeatherActor) {
      // Add a scene component as root
      USceneComponent *SceneRoot = NewObject<USceneComponent>(
          WeatherActor, USceneComponent::StaticClass(), TEXT("WeatherRoot"));
      if (SceneRoot) {
        SceneRoot->RegisterComponent();
        WeatherActor->SetRootComponent(SceneRoot);
      }

      // Optionally add wind source if requested
      bool bIncludeWind = false;
      Payload->TryGetBoolField(TEXT("includeWind"), bIncludeWind);
      if (bIncludeWind) {
        UWindDirectionalSourceComponent *WindComp = NewObject<UWindDirectionalSourceComponent>(
            WeatherActor, UWindDirectionalSourceComponent::StaticClass(), TEXT("WindSource"));
        if (WindComp) {
          WindComp->RegisterComponent();
          WindComp->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
          Resp->SetBoolField(TEXT("hasWindComponent"), true);
        }
      }

      bSuccess = true;
      Message = TEXT("Weather system actor created");
      Resp->SetStringField(TEXT("actorName"), WeatherActor->GetActorLabel());
      Resp->SetStringField(TEXT("actorClass"), WeatherActor->GetClass()->GetName());
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create weather system actor");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
  }
  // ========================================================================
  // CONFIGURE RAIN PARTICLES (Niagara)
  // ========================================================================
  else if (LowerSub == TEXT("configure_rain_particles")) {
#if MCP_HAS_NIAGARA
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString NiagaraSystemPath;
    Payload->TryGetStringField(TEXT("niagaraSystemPath"), NiagaraSystemPath);

    // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
    // Note: World already available from function scope (line 112)
    FString SearchLabel = ActorName.IsEmpty() ? TEXT("RainParticles") : ActorName;
    AActor* RainActor = FindActorByLabel(World, SearchLabel);

    if (!RainActor) {
      // Create new actor with Niagara component
      FVector Location(0, 0, 5000); // Default high up for rain
      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
      }

      RainActor = SpawnActorInActiveWorld<AActor>(
          AActor::StaticClass(), Location, FRotator::ZeroRotator,
          ActorName.IsEmpty() ? TEXT("RainParticles") : *ActorName);
    }

    if (RainActor) {
      // Find or add Niagara component
      UNiagaraComponent *NiagaraComp = RainActor->FindComponentByClass<UNiagaraComponent>();
      if (!NiagaraComp) {
        NiagaraComp = NewObject<UNiagaraComponent>(RainActor, UNiagaraComponent::StaticClass(), TEXT("RainNiagara"));
        if (NiagaraComp) {
          NiagaraComp->RegisterComponent();
          if (!RainActor->GetRootComponent()) {
            RainActor->SetRootComponent(NiagaraComp);
          } else {
            NiagaraComp->AttachToComponent(RainActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
          }
        }
      }

      if (NiagaraComp) {
        // Set Niagara system if path provided
        if (!NiagaraSystemPath.IsEmpty()) {
          UNiagaraSystem *NiagaraSys = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemPath);
          if (NiagaraSys) {
            NiagaraComp->SetAsset(NiagaraSys);
            Resp->SetStringField(TEXT("niagaraSystem"), NiagaraSystemPath);
          }
        }

        // Configure rain parameters
        double Intensity = 1.0;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
          NiagaraComp->SetFloatParameter(FName("RainIntensity"), static_cast<float>(Intensity));
        }

        double DropSize = 1.0;
        if (Payload->TryGetNumberField(TEXT("dropSize"), DropSize)) {
          NiagaraComp->SetFloatParameter(FName("DropSize"), static_cast<float>(DropSize));
        }

        double WindInfluence = 0.5;
        if (Payload->TryGetNumberField(TEXT("windInfluence"), WindInfluence)) {
          NiagaraComp->SetFloatParameter(FName("WindInfluence"), static_cast<float>(WindInfluence));
        }

        // Color
        const TSharedPtr<FJsonObject> *ColorObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
          double R = 0.7, G = 0.7, B = 0.8, A = 0.5;
          (*ColorObj)->TryGetNumberField(TEXT("r"), R);
          (*ColorObj)->TryGetNumberField(TEXT("g"), G);
          (*ColorObj)->TryGetNumberField(TEXT("b"), B);
          (*ColorObj)->TryGetNumberField(TEXT("a"), A);
          NiagaraComp->SetColorParameter(FName("RainColor"), FLinearColor(R, G, B, A));
        }

        // Activate
        bool bActivate = true;
        Payload->TryGetBoolField(TEXT("activate"), bActivate);
        if (bActivate) {
          NiagaraComp->Activate(true);
        }

        bSuccess = true;
        Message = TEXT("Rain particles configured");
        Resp->SetStringField(TEXT("actorName"), RainActor->GetActorLabel());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create Niagara component for rain");
        ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create rain particles actor");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
#else
    bSuccess = false;
    Message = TEXT("Niagara plugin not available");
    ErrorCode = TEXT("NIAGARA_NOT_AVAILABLE");
#endif
  }
  // ========================================================================
  // CONFIGURE SNOW PARTICLES (Niagara)
  // ========================================================================
  else if (LowerSub == TEXT("configure_snow_particles")) {
#if MCP_HAS_NIAGARA
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString NiagaraSystemPath;
    Payload->TryGetStringField(TEXT("niagaraSystemPath"), NiagaraSystemPath);

    // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
    // Note: World already available from function scope (line 112)
    FString SearchLabel = ActorName.IsEmpty() ? TEXT("SnowParticles") : ActorName;
    AActor* SnowActor = FindActorByLabel(World, SearchLabel);

    if (!SnowActor) {
      // Create new actor with Niagara component
      FVector Location(0, 0, 5000); // Default high up for snow
      const TSharedPtr<FJsonObject> *LocObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
      }

      SnowActor = SpawnActorInActiveWorld<AActor>(
          AActor::StaticClass(), Location, FRotator::ZeroRotator,
          ActorName.IsEmpty() ? TEXT("SnowParticles") : *ActorName);
    }

    if (SnowActor) {
      // Find or add Niagara component
      UNiagaraComponent *NiagaraComp = SnowActor->FindComponentByClass<UNiagaraComponent>();
      if (!NiagaraComp) {
        NiagaraComp = NewObject<UNiagaraComponent>(SnowActor, UNiagaraComponent::StaticClass(), TEXT("SnowNiagara"));
        if (NiagaraComp) {
          NiagaraComp->RegisterComponent();
          if (!SnowActor->GetRootComponent()) {
            SnowActor->SetRootComponent(NiagaraComp);
          } else {
            NiagaraComp->AttachToComponent(SnowActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
          }
        }
      }

      if (NiagaraComp) {
        // Set Niagara system if path provided
        if (!NiagaraSystemPath.IsEmpty()) {
          UNiagaraSystem *NiagaraSys = LoadObject<UNiagaraSystem>(nullptr, *NiagaraSystemPath);
          if (NiagaraSys) {
            NiagaraComp->SetAsset(NiagaraSys);
            Resp->SetStringField(TEXT("niagaraSystem"), NiagaraSystemPath);
          }
        }

        // Configure snow parameters
        double Intensity = 1.0;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity)) {
          NiagaraComp->SetFloatParameter(FName("SnowIntensity"), static_cast<float>(Intensity));
        }

        double FlakeSize = 1.0;
        if (Payload->TryGetNumberField(TEXT("flakeSize"), FlakeSize)) {
          NiagaraComp->SetFloatParameter(FName("FlakeSize"), static_cast<float>(FlakeSize));
        }

        double Turbulence = 0.3;
        if (Payload->TryGetNumberField(TEXT("turbulence"), Turbulence)) {
          NiagaraComp->SetFloatParameter(FName("Turbulence"), static_cast<float>(Turbulence));
        }

        double FallSpeed = 1.0;
        if (Payload->TryGetNumberField(TEXT("fallSpeed"), FallSpeed)) {
          NiagaraComp->SetFloatParameter(FName("FallSpeed"), static_cast<float>(FallSpeed));
        }

        // Color
        const TSharedPtr<FJsonObject> *ColorObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
          double R = 1.0, G = 1.0, B = 1.0, A = 0.8;
          (*ColorObj)->TryGetNumberField(TEXT("r"), R);
          (*ColorObj)->TryGetNumberField(TEXT("g"), G);
          (*ColorObj)->TryGetNumberField(TEXT("b"), B);
          (*ColorObj)->TryGetNumberField(TEXT("a"), A);
          NiagaraComp->SetColorParameter(FName("SnowColor"), FLinearColor(R, G, B, A));
        }

        // Activate
        bool bActivate = true;
        Payload->TryGetBoolField(TEXT("activate"), bActivate);
        if (bActivate) {
          NiagaraComp->Activate(true);
        }

        bSuccess = true;
        Message = TEXT("Snow particles configured");
        Resp->SetStringField(TEXT("actorName"), SnowActor->GetActorLabel());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create Niagara component for snow");
        ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create snow particles actor");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
#else
    bSuccess = false;
    Message = TEXT("Niagara plugin not available");
    ErrorCode = TEXT("NIAGARA_NOT_AVAILABLE");
#endif
  }
  // ========================================================================
  // CONFIGURE LIGHTNING (Flash Effect via Post Process)
  // ========================================================================
  else if (LowerSub == TEXT("configure_lightning")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    // Find existing or create new post process volume for lightning
    APostProcessVolume *LightningPPV = nullptr;
    for (TActorIterator<APostProcessVolume> It(World); It; ++It) {
      if (APostProcessVolume *PPV = *It) {
        if (ActorName.IsEmpty() || PPV->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
          LightningPPV = PPV;
          break;
        }
      }
    }

    if (!LightningPPV) {
      // Spawn new PostProcessVolume for lightning
      UClass *PPVClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.PostProcessVolume"));
      if (PPVClass) {
        FVector Location(0, 0, 0);
        LightningPPV = Cast<APostProcessVolume>(SpawnActorInActiveWorld<AActor>(
            PPVClass, Location, FRotator::ZeroRotator,
            ActorName.IsEmpty() ? TEXT("LightningEffect") : *ActorName));
      }
    }

    if (LightningPPV) {
      // Configure as unbound (affects entire scene)
      bool bUnbound = true;
      Payload->TryGetBoolField(TEXT("unbound"), bUnbound);
      LightningPPV->bUnbound = bUnbound;

      // Configure lightning flash intensity (exposure bias)
      double FlashIntensity = 2.0;
      if (Payload->TryGetNumberField(TEXT("flashIntensity"), FlashIntensity)) {
        LightningPPV->Settings.bOverride_AutoExposureBias = true;
        LightningPPV->Settings.AutoExposureBias = static_cast<float>(FlashIntensity);
      }

      // Color grading for lightning tint
      const TSharedPtr<FJsonObject> *TintObj = nullptr;
      if (Payload->TryGetObjectField(TEXT("lightningTint"), TintObj) && TintObj) {
        double R = 0.8, G = 0.85, B = 1.0, A = 1.0;
        (*TintObj)->TryGetNumberField(TEXT("r"), R);
        (*TintObj)->TryGetNumberField(TEXT("g"), G);
        (*TintObj)->TryGetNumberField(TEXT("b"), B);
        (*TintObj)->TryGetNumberField(TEXT("a"), A);
        LightningPPV->Settings.bOverride_ColorGain = true;
        LightningPPV->Settings.ColorGain = FVector4(R, G, B, A);
      }

      // Bloom for lightning glow
      double BloomIntensity = 0.0;
      if (Payload->TryGetNumberField(TEXT("bloomIntensity"), BloomIntensity)) {
        LightningPPV->Settings.bOverride_BloomIntensity = true;
        LightningPPV->Settings.BloomIntensity = static_cast<float>(BloomIntensity);
      }

      // Priority
      double Priority = 100.0;
      if (Payload->TryGetNumberField(TEXT("priority"), Priority)) {
        LightningPPV->Priority = static_cast<float>(Priority);
      }

      // Blend weight (0 = off, 1 = full effect)
      double BlendWeight = 0.0; // Start at 0, animate up during flash
      if (Payload->TryGetNumberField(TEXT("blendWeight"), BlendWeight)) {
        LightningPPV->BlendWeight = static_cast<float>(BlendWeight);
      }

      bSuccess = true;
      Message = TEXT("Lightning effect configured");
      Resp->SetStringField(TEXT("actorName"), LightningPPV->GetActorLabel());
      Resp->SetBoolField(TEXT("isUnbound"), LightningPPV->bUnbound);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to find or create PostProcessVolume for lightning");
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE WEATHER PRESET (Apply preset configurations)
  // ========================================================================
  else if (LowerSub == TEXT("configure_weather_preset")) {
    FString PresetName;
    Payload->TryGetStringField(TEXT("presetName"), PresetName);
    
    if (PresetName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("presetName required for configure_weather_preset");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Define common weather presets
      const FString LowerPreset = PresetName.ToLower();
      int32 ComponentsConfigured = 0;
      
      // Apply preset by configuring wind, rain/snow, and lighting
      if (LowerPreset == TEXT("clear") || LowerPreset == TEXT("sunny")) {
        // Clear skies - minimal wind, no precipitation
        for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
          if (UWindDirectionalSourceComponent* WindComp = (*It)->FindComponentByClass<UWindDirectionalSourceComponent>()) {
            WindComp->SetStrength(0.1f);
            WindComp->SetSpeed(50.0f);
            ComponentsConfigured++;
          }
        }
        Resp->SetStringField(TEXT("preset"), TEXT("clear"));
        Resp->SetStringField(TEXT("description"), TEXT("Clear skies with light breeze"));
        
      } else if (LowerPreset == TEXT("rainy") || LowerPreset == TEXT("rain")) {
        // Rainy - moderate wind, overcast
        for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
          if (UWindDirectionalSourceComponent* WindComp = (*It)->FindComponentByClass<UWindDirectionalSourceComponent>()) {
            WindComp->SetStrength(0.5f);
            WindComp->SetSpeed(200.0f);
            ComponentsConfigured++;
          }
        }
        Resp->SetStringField(TEXT("preset"), TEXT("rainy"));
        Resp->SetStringField(TEXT("description"), TEXT("Rainy weather with moderate wind"));
        
      } else if (LowerPreset == TEXT("stormy") || LowerPreset == TEXT("storm")) {
        // Stormy - strong wind, heavy precipitation
        for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
          if (UWindDirectionalSourceComponent* WindComp = (*It)->FindComponentByClass<UWindDirectionalSourceComponent>()) {
            WindComp->SetStrength(1.0f);
            WindComp->SetSpeed(500.0f);
            WindComp->SetMinimumGustAmount(0.3f);
            WindComp->SetMaximumGustAmount(0.8f);
            ComponentsConfigured++;
          }
        }
        Resp->SetStringField(TEXT("preset"), TEXT("stormy"));
        Resp->SetStringField(TEXT("description"), TEXT("Stormy weather with strong gusting winds"));
        
      } else if (LowerPreset == TEXT("snowy") || LowerPreset == TEXT("snow")) {
        // Snowy - light wind, cold atmosphere
        for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
          if (UWindDirectionalSourceComponent* WindComp = (*It)->FindComponentByClass<UWindDirectionalSourceComponent>()) {
            WindComp->SetStrength(0.3f);
            WindComp->SetSpeed(100.0f);
            ComponentsConfigured++;
          }
        }
        Resp->SetStringField(TEXT("preset"), TEXT("snowy"));
        Resp->SetStringField(TEXT("description"), TEXT("Snowy weather with light wind"));
        
      } else if (LowerPreset == TEXT("foggy") || LowerPreset == TEXT("fog")) {
        // Foggy - minimal wind
        for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
          if (UWindDirectionalSourceComponent* WindComp = (*It)->FindComponentByClass<UWindDirectionalSourceComponent>()) {
            WindComp->SetStrength(0.05f);
            WindComp->SetSpeed(20.0f);
            ComponentsConfigured++;
          }
        }
        Resp->SetStringField(TEXT("preset"), TEXT("foggy"));
        Resp->SetStringField(TEXT("description"), TEXT("Foggy conditions with still air"));
        
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Unknown weather preset: '%s'. Available: clear, rainy, stormy, snowy, foggy"), *PresetName);
        ErrorCode = TEXT("UNKNOWN_PRESET");
        Resp->SetStringField(TEXT("error"), Message);
      }
      
      if (bSuccess) {
        Message = FString::Printf(TEXT("Weather preset '%s' applied, configured %d components"), *PresetName, ComponentsConfigured);
        Resp->SetNumberField(TEXT("componentsConfigured"), ComponentsConfigured);
      }
    }
  }
  // ========================================================================
  // CONFIGURE WIND DIRECTIONAL (Specific wind direction settings)
  // ========================================================================
  else if (LowerSub == TEXT("configure_wind_directional")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    // Find or create wind directional source
    AActor* WindActor = nullptr;
    for (TActorIterator<AWindDirectionalSource> It(World); It; ++It) {
      if (AWindDirectionalSource* Wind = *It) {
        if (ActorName.IsEmpty() || Wind->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
          WindActor = Wind;
          break;
        }
      }
    }
    
    if (!WindActor) {
      // Spawn new WindDirectionalSource
      UClass* WindClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.WindDirectionalSource"));
      if (WindClass) {
        FVector Location(0, 0, 0);
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
          (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
          (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
          (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
        }
        WindActor = SpawnActorInActiveWorld<AActor>(
            WindClass, Location, FRotator::ZeroRotator,
            ActorName.IsEmpty() ? TEXT("WindDirectionalSource") : *ActorName);
      }
    }
    
    if (WindActor) {
      UWindDirectionalSourceComponent* WindComp = WindActor->FindComponentByClass<UWindDirectionalSourceComponent>();
      if (WindComp) {
        int32 PropertiesSet = 0;
        
        // Force directional type
        WindComp->SetWindType(EWindSourceType::Directional);
        PropertiesSet++;
        
        // Direction (yaw angle)
        double Direction = 0.0;
        if (Payload->TryGetNumberField(TEXT("direction"), Direction)) {
          WindActor->SetActorRotation(FRotator(0, static_cast<float>(Direction), 0));
          PropertiesSet++;
        }
        
        // Strength
        double Strength = 0.0;
        if (Payload->TryGetNumberField(TEXT("strength"), Strength)) {
          WindComp->SetStrength(static_cast<float>(Strength));
          PropertiesSet++;
        }
        
        // Speed
        double Speed = 0.0;
        if (Payload->TryGetNumberField(TEXT("speed"), Speed)) {
          WindComp->SetSpeed(static_cast<float>(Speed));
          PropertiesSet++;
        }
        
        // Gust settings
        double MinGust = 0.0;
        if (Payload->TryGetNumberField(TEXT("minGustAmount"), MinGust)) {
          WindComp->SetMinimumGustAmount(static_cast<float>(MinGust));
          PropertiesSet++;
        }
        
        double MaxGust = 0.0;
        if (Payload->TryGetNumberField(TEXT("maxGustAmount"), MaxGust)) {
          WindComp->SetMaximumGustAmount(static_cast<float>(MaxGust));
          PropertiesSet++;
        }
        
        bSuccess = true;
        Message = FString::Printf(TEXT("Wind directional configured with %d properties"), PropertiesSet);
        Resp->SetStringField(TEXT("actorName"), WindActor->GetActorLabel());
        Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
        Resp->SetStringField(TEXT("windType"), TEXT("Directional"));
      } else {
        bSuccess = false;
        Message = TEXT("WindDirectionalSourceComponent not found on actor");
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to find or create WindDirectionalSource actor");
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
  }
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Weather action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Not in editor
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Weather actions require editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
