// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 28: Water Body Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Math/UnrealMathUtility.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

// Water Plugin Headers (conditionally included)
#if __has_include("WaterBodyActor.h")
#define MCP_HAS_WATER_PLUGIN 1
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyOceanActor.h"
#include "WaterBodyOceanComponent.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyLakeComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterBodyRiverComponent.h"
#include "WaterSplineComponent.h"
#include "GerstnerWaterWaves.h"
#include "WaterZoneActor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#else
#define MCP_HAS_WATER_PLUGIN 0
#endif

// Helper function to find water body actors more efficiently
// Uses class-based filtering instead of iterating all actors
namespace {
#if MCP_HAS_WATER_PLUGIN
// FindWaterBodyByName removed (replaced by FindActorCached)

static void CollectAllWaterBodies(UWorld* World, TArray<AWaterBody*>& OutWaterBodies)
{
    if (!World) return;

    // Use TActorIterator for more efficient typed iteration
    for (TActorIterator<AWaterBody> It(World); It; ++It)
    {
        if (AWaterBody* WaterActor = *It)
        {
            OutWaterBodies.Add(WaterActor);
        }
    }
}

/**
 * CRITICAL FIX: Enhanced dialog suppressor for UE 5.7 Water plugin compatibility.
 * The Water plugin crashes when spawning water bodies if GIsAutomationTesting alone
 * is set, because it still tries to show editor visualization dialogs.
 * Setting both GIsAutomationTesting and GIsRunningUnattendedScript prevents this.
 */
struct FEnhancedDialogSuppressor
{
    bool bPreviousAutomationTesting;
    bool bPreviousUnattendedScript;
    bool bWasActive;

    FEnhancedDialogSuppressor()
    {
        bPreviousAutomationTesting = GIsAutomationTesting;
        bPreviousUnattendedScript = GIsRunningUnattendedScript;

        GIsAutomationTesting = true;
        GIsRunningUnattendedScript = true;
        bWasActive = true;
    }

    ~FEnhancedDialogSuppressor()
    {
        if (bWasActive)
        {
            GIsAutomationTesting = bPreviousAutomationTesting;
            GIsRunningUnattendedScript = bPreviousUnattendedScript;
        }
    }

    // Prevent copying to avoid double-restore
    FEnhancedDialogSuppressor(const FEnhancedDialogSuppressor&) = delete;
    FEnhancedDialogSuppressor& operator=(const FEnhancedDialogSuppressor&) = delete;
};

/**
 * CRITICAL FIX: Validates that required collision profiles exist before spawning water bodies.
 * The WaterBodyCollision profile must exist or water body physics will fail.
 */
static bool ValidateWaterCollisionProfile(FString& OutError)
{
    // Check if WaterBodyCollision profile exists
    UCollisionProfile* CollisionProfile = UCollisionProfile::Get();
    if (!CollisionProfile)
    {
        OutError = TEXT("CollisionProfile not available - Water plugin may not be initialized");
        return false;
    }

    // Look for WaterBodyCollision profile using GetProfileTemplate
    FCollisionResponseTemplate Template;
    if (!CollisionProfile->GetProfileTemplate(FName("WaterBodyCollision"), Template))
    {
        OutError = TEXT("WaterBodyCollision profile not found. Ensure Water plugin is enabled and loaded.");
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("%s"), *OutError);
        return false;
    }

    return true;
}

/**
 * CRITICAL FIX: Disables water body mesh generation to prevent crashes in unattended mode.
 * Must be called between deferred spawn and FinishSpawning.
 */
static void DisableWaterBodyMeshGeneration(AActor* WaterBodyActor)
{
    if (!WaterBodyActor)
    {
        return;
    }

    // Find the water body component
    UWaterBodyComponent* WaterComp = WaterBodyActor->FindComponentByClass<UWaterBodyComponent>();
    if (!WaterComp)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("No WaterBodyComponent found on %s"), *WaterBodyActor->GetName());
        return;
    }

    // UE 5.7: Use SetWaterBodyStaticMeshEnabled to disable static mesh generation
    // The previous properties (bGenerateWaterInfoMesh, bRenderWaterInfoMesh) are no longer exposed
    WaterComp->SetWaterBodyStaticMeshEnabled(false);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Disabled static mesh generation for %s"), *WaterBodyActor->GetName());
}

/**
 * CRITICAL FIX: Ensures a WaterZone exists in the world.
 * The Water plugin requires a WaterZone actor to be present before any water bodies
 * can be spawned. Without it, the Water plugin will trigger assertions and crash.
 *
 * @param World The world to check/create the WaterZone in
 * @param ActorSS The EditorActorSubsystem for spawning
 * @return Pointer to the WaterZone actor, or nullptr if creation failed
 */
static AWaterZone* EnsureWaterZoneExists(UWorld* World, UEditorActorSubsystem* ActorSS)
{
    if (!World || !ActorSS)
        return nullptr;

    // Check if a WaterZone already exists
    for (TActorIterator<AWaterZone> It(World); It; ++It)
    {
        if (AWaterZone* ExistingZone = *It)
        {
            return ExistingZone;
        }
    }

    // No WaterZone found - create one
    UClass* WaterZoneClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterZone"));
    if (!WaterZoneClass)
        return nullptr;

    // CRITICAL FIX: Suppress modal dialogs during WaterZone spawning
    FModalDialogSuppressor DialogSuppressor;

    // Spawn the WaterZone at origin
    AActor* ZoneActor = ActorSS->SpawnActorFromClass(WaterZoneClass, FVector::ZeroVector, FRotator::ZeroRotator);
    if (ZoneActor)
    {
    // Set a recognizable name
    ZoneActor->SetActorLabel(TEXT("MCP_WaterZone"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Created WaterZone"));
    return Cast<AWaterZone>(ZoneActor);
    }

    return nullptr;
}

/**
 * CRITICAL FIX: Safe water body spawning with comprehensive error handling.
 * Wraps the spawn operation with additional safety checks to prevent crashes
 * from Water plugin internal assertions in UE 5.7 unattended mode.
 *
 * @param ActorSS The EditorActorSubsystem
 * @param WaterClass The water body class to spawn
 * @param Location Spawn location
 * @param OutError Receives error message if spawn fails
 * @return The spawned actor, or nullptr on failure
 */
static AActor* SafeSpawnWaterBody(UEditorActorSubsystem* ActorSS, UClass* WaterClass, const FVector& Location, FString& OutError)
{
    OutError.Empty();

    if (!ActorSS)
    {
        OutError = TEXT("EditorActorSubsystem not available");
        return nullptr;
    }

    if (!WaterClass)
    {
        OutError = TEXT("Water class is null");
        return nullptr;
    }

    // CRITICAL FIX: Double-check world validity before spawning
    UWorld* World = GetActiveWorld();
    if (!World)
    {
        OutError = TEXT("No active world available");
        return nullptr;
    }

    // CRITICAL FIX: Validate collision profile (non-blocking warning)
    FString CollisionError;
    if (!ValidateWaterCollisionProfile(CollisionError))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Collision profile validation: %s"), *CollisionError);
        // Continue anyway - may still work in some configurations
    }

    AActor* SpawnedActor = nullptr;

    {
        // CRITICAL FIX: Enhanced dialog suppression for UE 5.7
        // Uses both GIsAutomationTesting and GIsRunningUnattendedScript
        FEnhancedDialogSuppressor DialogSuppressor;

        // CRITICAL FIX: Deferred spawning to delay PostRegisterAllComponents
        FActorSpawnParameters SpawnParams;
        SpawnParams.bDeferConstruction = true;  // Delays component registration
        SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
        SpawnParams.Name = FName(*FString::Printf(TEXT("%s_%s"), *WaterClass->GetName(), *FGuid::NewGuid().ToString(EGuidFormats::Short)));

        // CRITICAL FIX: Use ActorSS->SpawnActorFromClass() instead of World->SpawnActor<AActor>()
        // The templated SpawnActor<AActor>() uses CastChecked<AActor>() which crashes with WaterBody classes
        // SpawnActorFromClass() is the safe editor-approved method that works correctly with deferred spawning
        SpawnedActor = ActorSS->SpawnActorFromClass(WaterClass, Location, FRotator::ZeroRotator, SpawnParams.bDeferConstruction);

        if (!SpawnedActor)
        {
            OutError = TEXT("SpawnActorFromClass returned null - Water plugin may have rejected the spawn");
            return nullptr;
        }

        // CRITICAL FIX: Disable mesh generation BEFORE finishing spawn (only if deferred)
        // This prevents the crash in UpdateWaterInfoMeshComponents
        DisableWaterBodyMeshGeneration(SpawnedActor);

        // Now complete the spawn if deferred - this triggers PostRegisterAllComponents
        if (SpawnedActor->IsPendingKillPending() == false && SpawnParams.bDeferConstruction)
        {
            SpawnedActor->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
        }

    } // FEnhancedDialogSuppressor destructor restores flags here

    // Verify the actor is valid after completion
    if (!IsValid(SpawnedActor))
    {
        OutError = TEXT("Spawned actor is not valid after construction");
        return nullptr;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Successfully spawned water body: %s"), *SpawnedActor->GetName());
    return SpawnedActor;
}
#endif
} // namespace

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleWaterAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_water"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_water")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_water payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR && MCP_HAS_WATER_PLUGIN
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Water action '%s' completed"), *LowerSub);
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

  // ========================================================================
  // CREATE WATER BODY OCEAN
  // ========================================================================
  if (LowerSub == TEXT("create_water_body_ocean")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    UClass *OceanClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterBodyOcean"));
    if (OceanClass) {
      // CRITICAL FIX: Check world validity before spawning
      UWorld* World = GetActiveWorld();
      if (!World) {
        bSuccess = false;
        Message = TEXT("No active world available for spawning");
        ErrorCode = TEXT("NO_WORLD");
        Resp->SetStringField(TEXT("error"), Message);
        SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
        return true;
      }

      // CRITICAL FIX: Ensure WaterZone exists before spawning water bodies
      // The Water plugin requires a WaterZone actor to be present, or it will crash
      AWaterZone* WaterZone = EnsureWaterZoneExists(World, ActorSS);
      if (!WaterZone) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Could not create WaterZone - water body may fail to spawn"));
      }

      // CRITICAL FIX: Use SafeSpawnWaterBody helper with comprehensive error handling
      FString SpawnError;
      AActor* OceanActor = SafeSpawnWaterBody(ActorSS, OceanClass, Location, SpawnError);

      if (OceanActor) {
        // Set actor name/label with verification
        FString ActualName = Name;
        if (!Name.IsEmpty()) {
          ActualName = SetActorLabelWithVerification(OceanActor, Name, true);
        }

        // Configure ocean-specific properties
        UWaterBodyOceanComponent *OceanComp = OceanActor->FindComponentByClass<UWaterBodyOceanComponent>();
        if (OceanComp) {
          // Height offset
          double HeightOffset = 0.0;
          if (Payload->TryGetNumberField(TEXT("heightOffset"), HeightOffset)) {
            OceanComp->SetHeightOffset(static_cast<float>(HeightOffset));
          }

          // Material
          FString MaterialPath;
          if (Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
            UMaterialInterface *WaterMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (WaterMaterial) {
              OceanComp->SetWaterMaterial(WaterMaterial);
            }
          }
        }

        bSuccess = true;
        Message = TEXT("Water body ocean created");
        Resp->SetStringField(TEXT("actorName"), ActualName.IsEmpty() ? OceanActor->GetActorLabel() : ActualName);
      } else {
        bSuccess = false;
        // CRITICAL FIX: Include specific error message from spawn attempt
        Message = SpawnError.IsEmpty() ? TEXT("Failed to spawn ocean actor") : FString::Printf(TEXT("Failed to spawn ocean actor: %s"), *SpawnError);
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("WaterBodyOcean class not found - ensure Water plugin is enabled");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE WATER BODY LAKE
  // ========================================================================
  else if (LowerSub == TEXT("create_water_body_lake")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    UClass *LakeClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterBodyLake"));
    if (LakeClass) {
      // CRITICAL FIX: Ensure WaterZone exists before spawning water bodies
      UWorld* World = GetActiveWorld();
      if (World) {
        AWaterZone* WaterZone = EnsureWaterZoneExists(World, ActorSS);
        if (!WaterZone) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Could not create WaterZone - water body may fail to spawn"));
        }
      }

      // CRITICAL FIX: Use SafeSpawnWaterBody helper with comprehensive error handling
      FString SpawnError;
      AActor* LakeActor = SafeSpawnWaterBody(ActorSS, LakeClass, Location, SpawnError);

      if (LakeActor) {
        // Set actor name/label with verification
        FString ActualName = Name;
        if (!Name.IsEmpty()) {
          ActualName = SetActorLabelWithVerification(LakeActor, Name, true);
        }

        // Configure lake-specific properties
        UWaterBodyLakeComponent *LakeComp = LakeActor->FindComponentByClass<UWaterBodyLakeComponent>();
        if (LakeComp) {
          // Material
          FString MaterialPath;
          if (Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
            UMaterialInterface *WaterMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (WaterMaterial) {
              LakeComp->SetWaterMaterial(WaterMaterial);
            }
          }
        }

        bSuccess = true;
        Message = TEXT("Water body lake created");
        Resp->SetStringField(TEXT("actorName"), ActualName.IsEmpty() ? LakeActor->GetActorLabel() : ActualName);
      } else {
        bSuccess = false;
        // CRITICAL FIX: Include specific error message from spawn attempt
        Message = SpawnError.IsEmpty() ? TEXT("Failed to spawn lake actor") : FString::Printf(TEXT("Failed to spawn lake actor: %s"), *SpawnError);
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("WaterBodyLake class not found - ensure Water plugin is enabled");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // CREATE WATER BODY RIVER
  // ========================================================================
  else if (LowerSub == TEXT("create_water_body_river")) {
    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }

    FString Name;
    Payload->TryGetStringField(TEXT("name"), Name);

    UClass *RiverClass = LoadClass<AActor>(nullptr, TEXT("/Script/Water.WaterBodyRiver"));
    if (RiverClass) {
      // CRITICAL FIX: Ensure WaterZone exists before spawning water bodies
      UWorld* World = GetActiveWorld();
      if (World) {
        AWaterZone* WaterZone = EnsureWaterZoneExists(World, ActorSS);
        if (!WaterZone) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Could not create WaterZone - water body may fail to spawn"));
        }
      }

      // CRITICAL FIX: Use SafeSpawnWaterBody helper with comprehensive error handling
      FString SpawnError;
      AActor* RiverActor = SafeSpawnWaterBody(ActorSS, RiverClass, Location, SpawnError);

      if (RiverActor) {
        // Set actor name/label with verification
        FString ActualName = Name;
        if (!Name.IsEmpty()) {
          ActualName = SetActorLabelWithVerification(RiverActor, Name, true);
        }

        // Configure river-specific properties
        UWaterBodyComponent *RiverComp = RiverActor->FindComponentByClass<UWaterBodyComponent>();
        if (RiverComp) {
          // Material
          FString MaterialPath;
          if (Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
            UMaterialInterface *WaterMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (WaterMaterial) {
              RiverComp->SetWaterMaterial(WaterMaterial);
            }
          }
        }

        bSuccess = true;
        Message = TEXT("Water body river created");
        Resp->SetStringField(TEXT("actorName"), ActualName.IsEmpty() ? RiverActor->GetActorLabel() : ActualName);
      } else {
        bSuccess = false;
        // CRITICAL FIX: Include specific error message from spawn attempt
        Message = SpawnError.IsEmpty() ? TEXT("Failed to spawn river actor") : FString::Printf(TEXT("Failed to spawn river actor: %s"), *SpawnError);
        ErrorCode = TEXT("SPAWN_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("WaterBodyRiver class not found - ensure Water plugin is enabled");
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  }
  // ========================================================================
  // CONFIGURE WATER BODY
  // ========================================================================
  else if (LowerSub == TEXT("configure_water_body")) {
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for configure_water_body");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          // Water material
          FString MaterialPath;
          if (Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
            UMaterialInterface *WaterMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
            if (WaterMaterial) {
              WaterComp->SetWaterMaterial(WaterMaterial);
            }
          }

          // Underwater post process material
          FString UnderwaterMaterialPath;
          if (Payload->TryGetStringField(TEXT("underwaterMaterialPath"), UnderwaterMaterialPath)) {
            UMaterialInterface *UnderwaterMaterial = LoadObject<UMaterialInterface>(nullptr, *UnderwaterMaterialPath);
            if (UnderwaterMaterial) {
              WaterComp->SetUnderwaterPostProcessMaterial(UnderwaterMaterial);
            }
          }

          // Water info material
          FString WaterInfoMaterialPath;
          if (Payload->TryGetStringField(TEXT("waterInfoMaterialPath"), WaterInfoMaterialPath)) {
            UMaterialInterface *WaterInfoMaterial = LoadObject<UMaterialInterface>(nullptr, *WaterInfoMaterialPath);
            if (WaterInfoMaterial) {
              WaterComp->SetWaterInfoMaterial(WaterInfoMaterial);
            }
          }

          // Static mesh material
          FString StaticMeshMaterialPath;
          if (Payload->TryGetStringField(TEXT("staticMeshMaterialPath"), StaticMeshMaterialPath)) {
            UMaterialInterface *StaticMeshMaterial = LoadObject<UMaterialInterface>(nullptr, *StaticMeshMaterialPath);
            if (StaticMeshMaterial) {
              WaterComp->SetWaterStaticMeshMaterial(StaticMeshMaterial);
            }
          }

          // Ocean-specific: height offset
          if (UWaterBodyOceanComponent *OceanComp = Cast<UWaterBodyOceanComponent>(WaterComp)) {
            double HeightOffset = 0.0;
            if (Payload->TryGetNumberField(TEXT("heightOffset"), HeightOffset)) {
              OceanComp->SetHeightOffset(static_cast<float>(HeightOffset));
            }
          }

          bSuccess = true;
          Message = TEXT("Water body configured");
          Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CONFIGURE WATER WAVES (Gerstner Waves)
  // ========================================================================
  else if (LowerSub == TEXT("configure_water_waves")) {
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for configure_water_waves");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          UWaterWavesBase *WaterWaves = WaterComp->GetWaterWaves();
          if (WaterWaves) {
            // Try to cast to GerstnerWaterWaves for detailed configuration
            if (UGerstnerWaterWaves *GerstnerWaves = Cast<UGerstnerWaterWaves>(WaterWaves)) {
              Resp->SetStringField(TEXT("waveType"), TEXT("GerstnerWaterWaves"));
              
              // Access the wave generator for configuration
              UGerstnerWaterWaveGeneratorBase *Generator = GerstnerWaves->GerstnerWaveGenerator;
              if (Generator) {
                // Try to cast to Simple generator for property access
                if (UGerstnerWaterWaveGeneratorSimple *SimpleGen = Cast<UGerstnerWaterWaveGeneratorSimple>(Generator)) {
                  int32 PropertiesSet = 0;
                  
                  // Wave count
                  double NumWaves = 0.0;
                  if (Payload->TryGetNumberField(TEXT("numWaves"), NumWaves)) {
                    SimpleGen->NumWaves = FMath::Clamp(static_cast<int32>(NumWaves), 1, 128);
                    PropertiesSet++;
                  }
                  
                  // Seed
                  double Seed = 0.0;
                  if (Payload->TryGetNumberField(TEXT("seed"), Seed)) {
                    SimpleGen->Seed = static_cast<int32>(Seed);
                    PropertiesSet++;
                  }
                  
                  // Randomness
                  double Randomness = 0.0;
                  if (Payload->TryGetNumberField(TEXT("randomness"), Randomness)) {
                    SimpleGen->Randomness = FMath::Max(0.0f, static_cast<float>(Randomness));
                    PropertiesSet++;
                  }
                  
                  // Wavelength range
                  double MinWavelength = 0.0;
                  if (Payload->TryGetNumberField(TEXT("minWavelength"), MinWavelength)) {
                    SimpleGen->MinWavelength = FMath::Max(0.0f, static_cast<float>(MinWavelength));
                    PropertiesSet++;
                  }
                  
                  double MaxWavelength = 0.0;
                  if (Payload->TryGetNumberField(TEXT("maxWavelength"), MaxWavelength)) {
                    SimpleGen->MaxWavelength = FMath::Max(0.0f, static_cast<float>(MaxWavelength));
                    PropertiesSet++;
                  }
                  
                  double WavelengthFalloff = 0.0;
                  if (Payload->TryGetNumberField(TEXT("wavelengthFalloff"), WavelengthFalloff)) {
                    SimpleGen->WavelengthFalloff = FMath::Max(0.0f, static_cast<float>(WavelengthFalloff));
                    PropertiesSet++;
                  }
                  
                  // Amplitude range
                  double MinAmplitude = 0.0;
                  if (Payload->TryGetNumberField(TEXT("minAmplitude"), MinAmplitude)) {
                    SimpleGen->MinAmplitude = FMath::Max(0.0001f, static_cast<float>(MinAmplitude));
                    PropertiesSet++;
                  }
                  
                  double MaxAmplitude = 0.0;
                  if (Payload->TryGetNumberField(TEXT("maxAmplitude"), MaxAmplitude)) {
                    SimpleGen->MaxAmplitude = FMath::Max(0.0001f, static_cast<float>(MaxAmplitude));
                    PropertiesSet++;
                  }
                  
                  double AmplitudeFalloff = 0.0;
                  if (Payload->TryGetNumberField(TEXT("amplitudeFalloff"), AmplitudeFalloff)) {
                    SimpleGen->AmplitudeFalloff = FMath::Max(0.0f, static_cast<float>(AmplitudeFalloff));
                    PropertiesSet++;
                  }
                  
                  // Direction
                  double WindAngle = 0.0;
                  if (Payload->TryGetNumberField(TEXT("windAngle"), WindAngle)) {
                    SimpleGen->WindAngleDeg = FMath::Clamp(static_cast<float>(WindAngle), -180.0f, 180.0f);
                    PropertiesSet++;
                  }
                  
                  double DirectionSpread = 0.0;
                  if (Payload->TryGetNumberField(TEXT("directionSpread"), DirectionSpread)) {
                    SimpleGen->DirectionAngularSpreadDeg = FMath::Max(0.0f, static_cast<float>(DirectionSpread));
                    PropertiesSet++;
                  }
                  
                  // Steepness
                  double SmallWaveSteepness = 0.0;
                  if (Payload->TryGetNumberField(TEXT("smallWaveSteepness"), SmallWaveSteepness)) {
                    SimpleGen->SmallWaveSteepness = FMath::Clamp(static_cast<float>(SmallWaveSteepness), 0.0f, 1.0f);
                    PropertiesSet++;
                  }
                  
                  double LargeWaveSteepness = 0.0;
                  if (Payload->TryGetNumberField(TEXT("largeWaveSteepness"), LargeWaveSteepness)) {
                    SimpleGen->LargeWaveSteepness = FMath::Clamp(static_cast<float>(LargeWaveSteepness), 0.0f, 1.0f);
                    PropertiesSet++;
                  }
                  
                  double SteepnessFalloff = 0.0;
                  if (Payload->TryGetNumberField(TEXT("steepnessFalloff"), SteepnessFalloff)) {
                    SimpleGen->SteepnessFalloff = FMath::Max(0.0f, static_cast<float>(SteepnessFalloff));
                    PropertiesSet++;
                  }
                  
                  // Mark the wave asset as modified to trigger regeneration
                  GerstnerWaves->Modify();
                  
                  bSuccess = true;
                  Message = FString::Printf(TEXT("Configured %d wave properties on SimpleGenerator"), PropertiesSet);
                  Resp->SetStringField(TEXT("generatorType"), TEXT("Simple"));
                  Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
                } else {
                  // Spectrum generator - report its type but can't configure easily
                  bSuccess = true;
                  Message = TEXT("Wave generator is Spectrum type - limited configuration available");
                  Resp->SetStringField(TEXT("generatorType"), Generator->GetClass()->GetName());
                }
              } else {
                bSuccess = false;
                Message = TEXT("No wave generator found on GerstnerWaterWaves");
                ErrorCode = TEXT("GENERATOR_NOT_FOUND");
              }
            } else {
              bSuccess = true;
              Message = TEXT("Water waves found but not Gerstner type");
              Resp->SetStringField(TEXT("waveType"), WaterWaves->GetClass()->GetName());
            }
          } else {
            bSuccess = false;
            Message = TEXT("No water waves configured on this water body");
            ErrorCode = TEXT("WAVES_NOT_FOUND");
          }
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET WATER BODY INFO
  // ========================================================================
  else if (LowerSub == TEXT("get_water_body_info")) {
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for get_water_body_info");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          // Get water body type
          EWaterBodyType WaterType = WaterComp->GetWaterBodyType();
          FString TypeName;
          switch (WaterType) {
            case EWaterBodyType::Ocean: TypeName = TEXT("Ocean"); break;
            case EWaterBodyType::Lake: TypeName = TEXT("Lake"); break;
            case EWaterBodyType::River: TypeName = TEXT("River"); break;
            case EWaterBodyType::Transition: TypeName = TEXT("Transition"); break;
            default: TypeName = TEXT("Unknown"); break;
          }
          Resp->SetStringField(TEXT("waterBodyType"), TypeName);

          // Wave support
          Resp->SetBoolField(TEXT("supportsWaves"), WaterComp->IsWaveSupported());
          Resp->SetBoolField(TEXT("hasWaves"), WaterComp->HasWaves());

          // Physical material
          if (UPhysicalMaterial *PhysMat = WaterComp->GetPhysicalMaterial()) {
            Resp->SetStringField(TEXT("physicalMaterial"), PhysMat->GetName());
          }

          // Overlap priority
          Resp->SetNumberField(TEXT("overlapMaterialPriority"), WaterComp->GetOverlapMaterialPriority());

          // Channel depth
          Resp->SetNumberField(TEXT("channelDepth"), WaterComp->GetChannelDepth());

          bSuccess = true;
          Message = TEXT("Water body info retrieved");
          Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // LIST WATER BODIES
  // ========================================================================
  else if (LowerSub == TEXT("list_water_bodies")) {
    TArray<TSharedPtr<FJsonValue>> WaterBodies;
    
    for (AActor *Actor : ActorSS->GetAllLevelActors()) {
      if (!Actor) continue;
      
      UWaterBodyComponent *WaterComp = Actor->FindComponentByClass<UWaterBodyComponent>();
      if (WaterComp) {
        TSharedPtr<FJsonObject> WaterInfo = MakeShared<FJsonObject>();
        WaterInfo->SetStringField(TEXT("name"), Actor->GetActorLabel());
        WaterInfo->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
        
        EWaterBodyType WaterType = WaterComp->GetWaterBodyType();
        FString TypeName;
        switch (WaterType) {
          case EWaterBodyType::Ocean: TypeName = TEXT("Ocean"); break;
          case EWaterBodyType::Lake: TypeName = TEXT("Lake"); break;
          case EWaterBodyType::River: TypeName = TEXT("River"); break;
          case EWaterBodyType::Transition: TypeName = TEXT("Transition"); break;
          default: TypeName = TEXT("Unknown"); break;
        }
        WaterInfo->SetStringField(TEXT("type"), TypeName);
        
        FVector Loc = Actor->GetActorLocation();
        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Loc.X);
        LocObj->SetNumberField(TEXT("y"), Loc.Y);
        LocObj->SetNumberField(TEXT("z"), Loc.Z);
        WaterInfo->SetObjectField(TEXT("location"), LocObj);
        
        WaterBodies.Add(MakeShared<FJsonValueObject>(WaterInfo));
      }
    }

    Resp->SetArrayField(TEXT("waterBodies"), WaterBodies);
    Resp->SetNumberField(TEXT("count"), WaterBodies.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d water bodies"), WaterBodies.Num());
  }
  // ========================================================================
  // SET RIVER DEPTH - Phase 28 Extended (Real API: SetRiverDepthAtSplineInputKey)
  // ========================================================================
  else if (LowerSub == TEXT("set_river_depth")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for set_river_depth");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        // Must be a river component for depth/width control
        UWaterBodyRiverComponent *RiverComp = WaterActor->FindComponentByClass<UWaterBodyRiverComponent>();
        if (RiverComp) {
          int32 PropertiesSet = 0;
          
          // Set depth at spline key (0.0 = start, 1.0 = end)
          double SplineKey = 0.0;
          double Depth = 0.0;
          if (Payload->TryGetNumberField(TEXT("splineKey"), SplineKey) &&
              Payload->TryGetNumberField(TEXT("depth"), Depth)) {
            RiverComp->SetRiverDepthAtSplineInputKey(static_cast<float>(SplineKey), static_cast<float>(Depth));
            PropertiesSet++;
          }
          
          // Optionally set width at same key
          double Width = 0.0;
          if (Payload->TryGetNumberField(TEXT("width"), Width)) {
            RiverComp->SetRiverWidthAtSplineInputKey(static_cast<float>(SplineKey), static_cast<float>(Width));
            PropertiesSet++;
          }
          
          // Optionally set velocity at same key
          double Velocity = 0.0;
          if (Payload->TryGetNumberField(TEXT("velocity"), Velocity)) {
            RiverComp->SetWaterVelocityAtSplineInputKey(static_cast<float>(SplineKey), static_cast<float>(Velocity));
            PropertiesSet++;
          }
          
          // Optionally set audio intensity at same key
          double AudioIntensity = 0.0;
          if (Payload->TryGetNumberField(TEXT("audioIntensity"), AudioIntensity)) {
            RiverComp->SetAudioIntensityAtSplineInputKey(static_cast<float>(SplineKey), static_cast<float>(AudioIntensity));
            PropertiesSet++;
          }
          
          if (PropertiesSet > 0) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Set %d river properties at spline key %.2f"), PropertiesSet, SplineKey);
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
            Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
            Resp->SetNumberField(TEXT("splineKey"), SplineKey);
            // Report current values
            Resp->SetNumberField(TEXT("currentDepth"), RiverComp->GetRiverDepthAtSplineInputKey(static_cast<float>(SplineKey)));
            Resp->SetNumberField(TEXT("currentWidth"), RiverComp->GetRiverWidthAtSplineInputKey(static_cast<float>(SplineKey)));
          } else {
            bSuccess = false;
            Message = TEXT("splineKey and depth required for set_river_depth");
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Actor is not a WaterBodyRiver - depth/width control only available for rivers");
          ErrorCode = TEXT("WRONG_WATER_TYPE");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET OCEAN EXTENT - Phase 28 Extended (Real API: SetOceanExtent, SetCollisionExtents)
  // ========================================================================
  else if (LowerSub == TEXT("set_ocean_extent")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for set_ocean_extent");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        // Must be an ocean component for extent control
        UWaterBodyOceanComponent *OceanComp = WaterActor->FindComponentByClass<UWaterBodyOceanComponent>();
        if (OceanComp) {
          int32 PropertiesSet = 0;
          
          // Set ocean extent (X, Y dimensions)
          const TSharedPtr<FJsonObject> *ExtentObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("extent"), ExtentObj) && ExtentObj) {
            double X = 0, Y = 0;
            (*ExtentObj)->TryGetNumberField(TEXT("x"), X);
            (*ExtentObj)->TryGetNumberField(TEXT("y"), Y);
            OceanComp->SetOceanExtent(FVector2D(X, Y));
            PropertiesSet++;
          }
          
          // Set collision extents (X, Y, Z)
          const TSharedPtr<FJsonObject> *CollisionObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("collisionExtents"), CollisionObj) && CollisionObj) {
            double X = 0, Y = 0, Z = 0;
            (*CollisionObj)->TryGetNumberField(TEXT("x"), X);
            (*CollisionObj)->TryGetNumberField(TEXT("y"), Y);
            (*CollisionObj)->TryGetNumberField(TEXT("z"), Z);
            OceanComp->SetCollisionExtents(FVector(X, Y, Z));
            PropertiesSet++;
          }
          
          // Set height offset
          double HeightOffset = 0.0;
          if (Payload->TryGetNumberField(TEXT("heightOffset"), HeightOffset)) {
            OceanComp->SetHeightOffset(static_cast<float>(HeightOffset));
            PropertiesSet++;
          }
          
          if (PropertiesSet > 0) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Set %d ocean properties"), PropertiesSet);
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
            Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
          } else {
            bSuccess = false;
            Message = TEXT("extent, collisionExtents, or heightOffset required for set_ocean_extent");
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Actor is not a WaterBodyOcean - extent control only available for oceans");
          ErrorCode = TEXT("WRONG_WATER_TYPE");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET WATER STATIC MESH - Phase 28 Extended (Real API: SetWaterBodyStaticMeshEnabled, SetWaterMeshOverride)
  // ========================================================================
  else if (LowerSub == TEXT("set_water_static_mesh")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for set_water_static_mesh");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          int32 PropertiesSet = 0;
          
          // Enable/disable static mesh rendering
          bool bEnabled = false;
          if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
            WaterComp->SetWaterBodyStaticMeshEnabled(bEnabled);
            PropertiesSet++;
          }
          
          // Set mesh override
          FString MeshPath;
          if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) && !MeshPath.IsEmpty()) {
            UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
            if (Mesh) {
              WaterComp->SetWaterMeshOverride(Mesh);
              PropertiesSet++;
            }
          }
          
          // Set HLOD material
          FString HLODMaterialPath;
          if (Payload->TryGetStringField(TEXT("hlodMaterialPath"), HLODMaterialPath) && !HLODMaterialPath.IsEmpty()) {
            UMaterialInterface *HLODMaterial = LoadObject<UMaterialInterface>(nullptr, *HLODMaterialPath);
            if (HLODMaterial) {
              WaterComp->SetHLODMaterial(HLODMaterial);
              PropertiesSet++;
            }
          }
          
          if (PropertiesSet > 0) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Set %d static mesh properties"), PropertiesSet);
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
            Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
          } else {
            bSuccess = false;
            Message = TEXT("enabled, meshPath, or hlodMaterialPath required");
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET RIVER TRANSITION MATERIALS - Phase 28 Extended (Real API: SetLakeTransitionMaterial, SetOceanTransitionMaterial)
  // ========================================================================
  else if (LowerSub == TEXT("set_river_transitions")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for set_river_transitions");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        // Must be a river component for transition materials
        UWaterBodyRiverComponent *RiverComp = WaterActor->FindComponentByClass<UWaterBodyRiverComponent>();
        if (RiverComp) {
          int32 PropertiesSet = 0;
          
          // Set lake transition material
          FString LakeTransitionPath;
          if (Payload->TryGetStringField(TEXT("lakeTransitionMaterial"), LakeTransitionPath) && !LakeTransitionPath.IsEmpty()) {
            UMaterialInterface *LakeMat = LoadObject<UMaterialInterface>(nullptr, *LakeTransitionPath);
            if (LakeMat) {
              RiverComp->SetLakeTransitionMaterial(LakeMat);
              PropertiesSet++;
            }
          }
          
          // Set ocean transition material
          FString OceanTransitionPath;
          if (Payload->TryGetStringField(TEXT("oceanTransitionMaterial"), OceanTransitionPath) && !OceanTransitionPath.IsEmpty()) {
            UMaterialInterface *OceanMat = LoadObject<UMaterialInterface>(nullptr, *OceanTransitionPath);
            if (OceanMat) {
              RiverComp->SetOceanTransitionMaterial(OceanMat);
              PropertiesSet++;
            }
          }
          
          if (PropertiesSet > 0) {
            bSuccess = true;
            Message = FString::Printf(TEXT("Set %d river transition materials"), PropertiesSet);
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
            Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
          } else {
            bSuccess = false;
            Message = TEXT("lakeTransitionMaterial or oceanTransitionMaterial required");
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Actor is not a WaterBodyRiver - transition materials only available for rivers");
          ErrorCode = TEXT("WRONG_WATER_TYPE");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // SET WATER ZONE OVERRIDE - Phase 28 Extended (Real API: SetWaterZoneOverride)
  // ========================================================================
  else if (LowerSub == TEXT("set_water_zone")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for set_water_zone");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          FString WaterZonePath;
          if (Payload->TryGetStringField(TEXT("waterZonePath"), WaterZonePath) && !WaterZonePath.IsEmpty()) {
            // Use TActorIterator for efficient AWaterZone lookup instead of O(N) loop
            AWaterZone* WaterZone = Cast<AWaterZone>(FindActorCached(FName(*WaterZonePath)));
            
            if (WaterZone) {
              TSoftObjectPtr<AWaterZone> WaterZonePtr(WaterZone);
              WaterComp->SetWaterZoneOverride(WaterZonePtr);
              
              bSuccess = true;
              Message = TEXT("Water zone override set");
              Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
              Resp->SetStringField(TEXT("waterZonePath"), WaterZonePath);
            } else {
              bSuccess = false;
              Message = FString::Printf(TEXT("Water zone '%s' not found"), *WaterZonePath);
              ErrorCode = TEXT("WATER_ZONE_NOT_FOUND");
            }
          } else {
            bSuccess = false;
            Message = TEXT("waterZonePath required for set_water_zone");
            ErrorCode = TEXT("INVALID_ARGUMENT");
          }
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET WATER SURFACE INFO - Phase 28 Extended (Real API: GetWaterSurfaceInfoAtLocation)
  // ========================================================================
  else if (LowerSub == TEXT("get_water_surface_info")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for get_water_surface_info");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          // Get query location
          FVector QueryLocation(0, 0, 0);
          const TSharedPtr<FJsonObject> *LocObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
            (*LocObj)->TryGetNumberField(TEXT("x"), QueryLocation.X);
            (*LocObj)->TryGetNumberField(TEXT("y"), QueryLocation.Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), QueryLocation.Z);
          }
          
          bool bIncludeDepth = true;
          Payload->TryGetBoolField(TEXT("includeDepth"), bIncludeDepth);
          
          FVector SurfaceLocation, SurfaceNormal, WaterVelocity;
          float WaterDepth = 0.0f;
          
          bool bFound = WaterComp->GetWaterSurfaceInfoAtLocation(
            QueryLocation, SurfaceLocation, SurfaceNormal, WaterVelocity, WaterDepth, bIncludeDepth);
          
          if (bFound) {
            TSharedPtr<FJsonObject> SurfaceLocObj = MakeShared<FJsonObject>();
            SurfaceLocObj->SetNumberField(TEXT("x"), SurfaceLocation.X);
            SurfaceLocObj->SetNumberField(TEXT("y"), SurfaceLocation.Y);
            SurfaceLocObj->SetNumberField(TEXT("z"), SurfaceLocation.Z);
            Resp->SetObjectField(TEXT("surfaceLocation"), SurfaceLocObj);
            
            TSharedPtr<FJsonObject> NormalObj = MakeShared<FJsonObject>();
            NormalObj->SetNumberField(TEXT("x"), SurfaceNormal.X);
            NormalObj->SetNumberField(TEXT("y"), SurfaceNormal.Y);
            NormalObj->SetNumberField(TEXT("z"), SurfaceNormal.Z);
            Resp->SetObjectField(TEXT("surfaceNormal"), NormalObj);
            
            TSharedPtr<FJsonObject> VelocityObj = MakeShared<FJsonObject>();
            VelocityObj->SetNumberField(TEXT("x"), WaterVelocity.X);
            VelocityObj->SetNumberField(TEXT("y"), WaterVelocity.Y);
            VelocityObj->SetNumberField(TEXT("z"), WaterVelocity.Z);
            Resp->SetObjectField(TEXT("waterVelocity"), VelocityObj);
            
            Resp->SetNumberField(TEXT("waterDepth"), WaterDepth);
            
            bSuccess = true;
            Message = TEXT("Water surface info retrieved");
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
          } else {
            bSuccess = false;
            Message = TEXT("Location not within water body bounds");
            ErrorCode = TEXT("LOCATION_NOT_IN_WATER");
          }
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET WAVE INFO - Phase 28 Extended (Real API: GetWaveInfoAtPosition)
  // ========================================================================
  else if (LowerSub == TEXT("get_wave_info")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for get_wave_info");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          // Get query position
          FVector QueryPosition(0, 0, 0);
          const TSharedPtr<FJsonObject> *PosObj = nullptr;
          if (Payload->TryGetObjectField(TEXT("position"), PosObj) && PosObj) {
            (*PosObj)->TryGetNumberField(TEXT("x"), QueryPosition.X);
            (*PosObj)->TryGetNumberField(TEXT("y"), QueryPosition.Y);
            (*PosObj)->TryGetNumberField(TEXT("z"), QueryPosition.Z);
          }
          
          double WaterDepth = 100.0;
          Payload->TryGetNumberField(TEXT("waterDepth"), WaterDepth);
          
          bool bSimpleWaves = false;
          Payload->TryGetBoolField(TEXT("simpleWaves"), bSimpleWaves);
          
          FWaveInfo WaveInfo;
          bool bFound = WaterComp->GetWaveInfoAtPosition(QueryPosition, static_cast<float>(WaterDepth), bSimpleWaves, WaveInfo);
          
          if (bFound) {
            Resp->SetNumberField(TEXT("waveHeight"), WaveInfo.Height);
            Resp->SetNumberField(TEXT("waveMaxHeight"), WaveInfo.MaxHeight);
            Resp->SetNumberField(TEXT("attenuationFactor"), WaveInfo.AttenuationFactor);
            Resp->SetNumberField(TEXT("referenceTime"), WaveInfo.ReferenceTime);
            
            TSharedPtr<FJsonObject> NormalObj = MakeShared<FJsonObject>();
            NormalObj->SetNumberField(TEXT("x"), WaveInfo.Normal.X);
            NormalObj->SetNumberField(TEXT("y"), WaveInfo.Normal.Y);
            NormalObj->SetNumberField(TEXT("z"), WaveInfo.Normal.Z);
            Resp->SetObjectField(TEXT("waveNormal"), NormalObj);
            
            bSuccess = true;
            Message = TEXT("Wave info retrieved");
            Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
          } else {
            bSuccess = false;
            Message = TEXT("Could not get wave info at position");
            ErrorCode = TEXT("WAVE_INFO_FAILED");
          }
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // GET WATER DEPTH INFO - Phase 28 Extended (Query-only)
  // ========================================================================
  else if (LowerSub == TEXT("get_water_depth_info")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for get_water_depth_info");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup instead of O(N) GetAllLevelActors()
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyComponent *WaterComp = WaterActor->FindComponentByClass<UWaterBodyComponent>();
        if (WaterComp) {
          Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
          Resp->SetNumberField(TEXT("channelDepth"), WaterComp->GetChannelDepth());
          Resp->SetNumberField(TEXT("constantDepth"), WaterComp->GetConstantDepth());
          Resp->SetNumberField(TEXT("overlapMaterialPriority"), WaterComp->GetOverlapMaterialPriority());
          Resp->SetBoolField(TEXT("supportsWaves"), WaterComp->IsWaveSupported());
          Resp->SetBoolField(TEXT("hasWaves"), WaterComp->HasWaves());
          
          bSuccess = true;
          Message = TEXT("Water depth info retrieved");
          Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
        } else {
          bSuccess = false;
          Message = TEXT("WaterBodyComponent not found on actor");
          ErrorCode = TEXT("COMPONENT_NOT_FOUND");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // CONFIGURE OCEAN WAVES - Phase 28 Extended (Configure wave parameters on ocean water bodies)
  // ========================================================================
  else if (LowerSub == TEXT("configure_ocean_waves")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required for configure_ocean_waves");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Use optimized TActorIterator-based lookup
      UWorld* World = GetActiveWorld();
      AWaterBody* WaterActor = Cast<AWaterBody>(FindActorCached(FName(*ActorName)));

      if (WaterActor) {
        UWaterBodyOceanComponent *OceanComp = WaterActor->FindComponentByClass<UWaterBodyOceanComponent>();
        if (OceanComp) {
          UWaterBodyComponent *WaterComp = OceanComp;
          UWaterWavesBase *WaterWaves = WaterComp->GetWaterWaves();
          
          int32 PropertiesSet = 0;
          
          if (WaterWaves) {
            if (UGerstnerWaterWaves *GerstnerWaves = Cast<UGerstnerWaterWaves>(WaterWaves)) {
              UGerstnerWaterWaveGeneratorBase *Generator = GerstnerWaves->GerstnerWaveGenerator;
              if (Generator) {
                if (UGerstnerWaterWaveGeneratorSimple *SimpleGen = Cast<UGerstnerWaterWaveGeneratorSimple>(Generator)) {
                  // Wave count
                  double NumWaves = 0.0;
                  if (Payload->TryGetNumberField(TEXT("numWaves"), NumWaves)) {
                    SimpleGen->NumWaves = FMath::Clamp(static_cast<int32>(NumWaves), 1, 128);
                    PropertiesSet++;
                  }
                  
                  // Wavelength range
                  double MinWavelength = 0.0;
                  if (Payload->TryGetNumberField(TEXT("minWavelength"), MinWavelength)) {
                    SimpleGen->MinWavelength = FMath::Max(0.0f, static_cast<float>(MinWavelength));
                    PropertiesSet++;
                  }
                  
                  double MaxWavelength = 0.0;
                  if (Payload->TryGetNumberField(TEXT("maxWavelength"), MaxWavelength)) {
                    SimpleGen->MaxWavelength = FMath::Max(0.0f, static_cast<float>(MaxWavelength));
                    PropertiesSet++;
                  }
                  
                  // Amplitude range  
                  double MinAmplitude = 0.0;
                  if (Payload->TryGetNumberField(TEXT("minAmplitude"), MinAmplitude)) {
                    SimpleGen->MinAmplitude = FMath::Max(0.0001f, static_cast<float>(MinAmplitude));
                    PropertiesSet++;
                  }
                  
                  double MaxAmplitude = 0.0;
                  if (Payload->TryGetNumberField(TEXT("maxAmplitude"), MaxAmplitude)) {
                    SimpleGen->MaxAmplitude = FMath::Max(0.0001f, static_cast<float>(MaxAmplitude));
                    PropertiesSet++;
                  }
                  
                  // Wind direction
                  double WindAngle = 0.0;
                  if (Payload->TryGetNumberField(TEXT("windAngle"), WindAngle)) {
                    SimpleGen->WindAngleDeg = FMath::Clamp(static_cast<float>(WindAngle), -180.0f, 180.0f);
                    PropertiesSet++;
                  }
                  
                  // Wave steepness
                  double SmallWaveSteepness = 0.0;
                  if (Payload->TryGetNumberField(TEXT("smallWaveSteepness"), SmallWaveSteepness)) {
                    SimpleGen->SmallWaveSteepness = FMath::Clamp(static_cast<float>(SmallWaveSteepness), 0.0f, 1.0f);
                    PropertiesSet++;
                  }
                  
                  double LargeWaveSteepness = 0.0;
                  if (Payload->TryGetNumberField(TEXT("largeWaveSteepness"), LargeWaveSteepness)) {
                    SimpleGen->LargeWaveSteepness = FMath::Clamp(static_cast<float>(LargeWaveSteepness), 0.0f, 1.0f);
                    PropertiesSet++;
                  }
                  
                  // Mark the wave asset as modified
                  GerstnerWaves->Modify();
                  
                  bSuccess = true;
                  Message = FString::Printf(TEXT("Configured %d ocean wave properties"), PropertiesSet);
                  Resp->SetStringField(TEXT("actorName"), WaterActor->GetActorLabel());
                  Resp->SetNumberField(TEXT("propertiesSet"), PropertiesSet);
                  Resp->SetStringField(TEXT("generatorType"), TEXT("Simple"));
                } else {
                  bSuccess = true;
                  Message = TEXT("Wave generator is Spectrum type - limited configuration available");
                  Resp->SetStringField(TEXT("generatorType"), Generator->GetClass()->GetName());
                }
              } else {
                bSuccess = false;
                Message = TEXT("No wave generator found on GerstnerWaterWaves");
                ErrorCode = TEXT("GENERATOR_NOT_FOUND");
              }
            } else {
              bSuccess = true;
              Message = TEXT("Water waves found but not Gerstner type");
              Resp->SetStringField(TEXT("waveType"), WaterWaves->GetClass()->GetName());
            }
          } else {
            bSuccess = false;
            Message = TEXT("No water waves configured on this water body");
            ErrorCode = TEXT("WAVES_NOT_FOUND");
          }
        } else {
          bSuccess = false;
          Message = TEXT("Actor is not a WaterBodyOcean - ocean wave configuration only available for oceans");
          ErrorCode = TEXT("WRONG_WATER_TYPE");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Water body actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      }
    }
  }
  // ========================================================================
  // QUERY WATER BODIES - Alias for list_water_bodies for semantic consistency
  // ========================================================================
  else if (LowerSub == TEXT("query_water_bodies")) {
    // Reuse list_water_bodies logic
    TArray<TSharedPtr<FJsonValue>> WaterBodies;
    
    for (AActor *Actor : ActorSS->GetAllLevelActors()) {
      if (!Actor) continue;
      
      UWaterBodyComponent *WaterComp = Actor->FindComponentByClass<UWaterBodyComponent>();
      if (WaterComp) {
        TSharedPtr<FJsonObject> WaterInfo = MakeShared<FJsonObject>();
        WaterInfo->SetStringField(TEXT("name"), Actor->GetActorLabel());
        WaterInfo->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
        
        EWaterBodyType WaterType = WaterComp->GetWaterBodyType();
        FString TypeName;
        switch (WaterType) {
          case EWaterBodyType::Ocean: TypeName = TEXT("Ocean"); break;
          case EWaterBodyType::Lake: TypeName = TEXT("Lake"); break;
          case EWaterBodyType::River: TypeName = TEXT("River"); break;
          case EWaterBodyType::Transition: TypeName = TEXT("Transition"); break;
          default: TypeName = TEXT("Unknown"); break;
        }
        WaterInfo->SetStringField(TEXT("type"), TypeName);
        
        FVector Loc = Actor->GetActorLocation();
        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Loc.X);
        LocObj->SetNumberField(TEXT("y"), Loc.Y);
        LocObj->SetNumberField(TEXT("z"), Loc.Z);
        WaterInfo->SetObjectField(TEXT("location"), LocObj);
        
        // Additional info for query
        WaterInfo->SetBoolField(TEXT("supportsWaves"), WaterComp->IsWaveSupported());
        WaterInfo->SetBoolField(TEXT("hasWaves"), WaterComp->HasWaves());
        WaterInfo->SetNumberField(TEXT("channelDepth"), WaterComp->GetChannelDepth());
        
        WaterBodies.Add(MakeShared<FJsonValueObject>(WaterInfo));
      }
    }

    Resp->SetArrayField(TEXT("waterBodies"), WaterBodies);
    Resp->SetNumberField(TEXT("count"), WaterBodies.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d water bodies"), WaterBodies.Num());
  }
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Water action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Water plugin not available or not in editor
  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Water actions require editor build with Water plugin enabled."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
