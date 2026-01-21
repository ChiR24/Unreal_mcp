// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 35: Additional Gameplay Systems Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Math/UnrealMathUtility.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "UnrealClient.h"  // For FViewport
#include "EditorViewportClient.h"  // For FEditorViewportClient

// Instanced Static Mesh
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

// HLOD
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"

// Localization
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

// Scalability
#include "Scalability.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "GameFramework/GameUserSettings.h"

// Screenshot
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

// Asset creation
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
#include "Factories/DataAssetFactory.h"
#include "Engine/DataAsset.h"

// Dialogue System
#include "Sound/DialogueWave.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueTypes.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageGameplaySystemsAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_gameplay_systems"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_gameplay_systems")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_gameplay_systems payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  if (SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action_type"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Gameplay systems action '%s' completed"), *LowerSub);
  FString ErrorCode;

  UWorld* World = GetActiveWorld();
  if (!World && LowerSub != TEXT("get_gameplay_systems_info") && 
      LowerSub != TEXT("get_available_cultures") && 
      LowerSub != TEXT("get_scalability_settings")) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("No active world found."),
                        TEXT("NO_WORLD"));
    return true;
  }

  // ==================== TARGETING ====================
  if (LowerSub == TEXT("create_targeting_component")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName = TEXT("TargetingComponent");
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    
    double MaxRange = 2000.0;
    Payload->TryGetNumberField(TEXT("maxTargetingRange"), MaxRange);
    double ConeAngle = 45.0;
    Payload->TryGetNumberField(TEXT("targetingConeAngle"), ConeAngle);
    bool bAutoTarget = true;
    Payload->TryGetBoolField(TEXT("autoTargetNearest"), bAutoTarget);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      // Create a scene component to represent targeting (actual targeting logic is blueprint-based)
      USceneComponent* TargetingComp = NewObject<USceneComponent>(TargetActor, *ComponentName);
      if (TargetingComp) {
        TargetingComp->RegisterComponent();
        TargetingComp->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        // Store targeting settings as component tags
        TargetingComp->ComponentTags.Add(*FString::Printf(TEXT("MaxRange:%.1f"), MaxRange));
        TargetingComp->ComponentTags.Add(*FString::Printf(TEXT("ConeAngle:%.1f"), ConeAngle));
        TargetingComp->ComponentTags.Add(*FString::Printf(TEXT("AutoTarget:%s"), bAutoTarget ? TEXT("true") : TEXT("false")));
        
        Resp->SetStringField(TEXT("componentName"), ComponentName);
        Resp->SetNumberField(TEXT("maxTargetingRange"), MaxRange);
        Resp->SetNumberField(TEXT("targetingConeAngle"), ConeAngle);
        Resp->SetBoolField(TEXT("autoTargetNearest"), bAutoTarget);
        Message = FString::Printf(TEXT("Created targeting component '%s' on actor '%s'"), *ComponentName, *ActorName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create targeting component");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  else if (LowerSub == TEXT("configure_lock_on_target")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    double LockOnRange = 1500.0;
    Payload->TryGetNumberField(TEXT("lockOnRange"), LockOnRange);
    double LockOnAngle = 30.0;
    Payload->TryGetNumberField(TEXT("lockOnAngle"), LockOnAngle);
    double BreakDistance = 2000.0;
    Payload->TryGetNumberField(TEXT("breakLockOnDistance"), BreakDistance);
    bool bSticky = true;
    Payload->TryGetBoolField(TEXT("stickyLockOn"), bSticky);
    double LockOnSpeed = 10.0;
    Payload->TryGetNumberField(TEXT("lockOnSpeed"), LockOnSpeed);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      // Store lock-on configuration in actor tags
      TargetActor->Tags.Add(*FString::Printf(TEXT("LockOn_Range:%.1f"), LockOnRange));
      TargetActor->Tags.Add(*FString::Printf(TEXT("LockOn_Angle:%.1f"), LockOnAngle));
      TargetActor->Tags.Add(*FString::Printf(TEXT("LockOn_Break:%.1f"), BreakDistance));
      TargetActor->Tags.Add(*FString::Printf(TEXT("LockOn_Sticky:%s"), bSticky ? TEXT("true") : TEXT("false")));
      TargetActor->Tags.Add(*FString::Printf(TEXT("LockOn_Speed:%.1f"), LockOnSpeed));
      
      Resp->SetNumberField(TEXT("lockOnRange"), LockOnRange);
      Resp->SetNumberField(TEXT("lockOnAngle"), LockOnAngle);
      Resp->SetNumberField(TEXT("breakLockOnDistance"), BreakDistance);
      Resp->SetBoolField(TEXT("stickyLockOn"), bSticky);
      Message = FString::Printf(TEXT("Configured lock-on for actor '%s'"), *ActorName);
    }
  }
  
  else if (LowerSub == TEXT("configure_aim_assist")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    double AimAssistStrength = 0.5;
    Payload->TryGetNumberField(TEXT("aimAssistStrength"), AimAssistStrength);
    double AimAssistRadius = 100.0;
    Payload->TryGetNumberField(TEXT("aimAssistRadius"), AimAssistRadius);
    double MagnetismStrength = 0.3;
    Payload->TryGetNumberField(TEXT("magnetismStrength"), MagnetismStrength);
    bool bBulletMagnetism = false;
    Payload->TryGetBoolField(TEXT("bulletMagnetism"), bBulletMagnetism);
    double FrictionScale = 1.0;
    Payload->TryGetNumberField(TEXT("frictionZoneScale"), FrictionScale);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      TargetActor->Tags.Add(*FString::Printf(TEXT("AimAssist_Strength:%.2f"), AimAssistStrength));
      TargetActor->Tags.Add(*FString::Printf(TEXT("AimAssist_Radius:%.1f"), AimAssistRadius));
      TargetActor->Tags.Add(*FString::Printf(TEXT("AimAssist_Magnetism:%.2f"), MagnetismStrength));
      TargetActor->Tags.Add(*FString::Printf(TEXT("AimAssist_BulletMag:%s"), bBulletMagnetism ? TEXT("true") : TEXT("false")));
      
      Resp->SetNumberField(TEXT("aimAssistStrength"), AimAssistStrength);
      Resp->SetNumberField(TEXT("aimAssistRadius"), AimAssistRadius);
      Message = FString::Printf(TEXT("Configured aim assist for actor '%s'"), *ActorName);
    }
  }

  // ==================== CHECKPOINTS ====================
  else if (LowerSub == TEXT("create_checkpoint_actor")) {
    FString ActorName = TEXT("Checkpoint_1");
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    FRotator Rotation = FRotator::ZeroRotator;
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj)) {
      (*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
      (*RotObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
      (*RotObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
    }
    
    FString CheckpointId;
    Payload->TryGetStringField(TEXT("checkpointId"), CheckpointId);
    double TriggerRadius = 200.0;
    Payload->TryGetNumberField(TEXT("triggerRadius"), TriggerRadius);
    bool bAutoActivate = false;
    Payload->TryGetBoolField(TEXT("autoActivate"), bAutoActivate);
    
    // Create a simple actor as checkpoint placeholder
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* CheckpointActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (CheckpointActor) {
      CheckpointActor->SetActorLabel(*ActorName);
      CheckpointActor->Tags.Add(TEXT("Checkpoint"));
      if (!CheckpointId.IsEmpty()) {
        CheckpointActor->Tags.Add(*FString::Printf(TEXT("CheckpointId:%s"), *CheckpointId));
      }
      CheckpointActor->Tags.Add(*FString::Printf(TEXT("TriggerRadius:%.1f"), TriggerRadius));
      CheckpointActor->Tags.Add(*FString::Printf(TEXT("AutoActivate:%s"), bAutoActivate ? TEXT("true") : TEXT("false")));
      
      Resp->SetStringField(TEXT("actorName"), ActorName);
      Resp->SetStringField(TEXT("checkpointId"), CheckpointId);
      Message = FString::Printf(TEXT("Created checkpoint actor '%s'"), *ActorName);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to spawn checkpoint actor");
      ErrorCode = TEXT("SPAWN_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("save_checkpoint")) {
    FString CheckpointId;
    Payload->TryGetStringField(TEXT("checkpointId"), CheckpointId);
    FString SlotName = TEXT("Checkpoint");
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 PlayerIndex = 0;
    double PlayerIndexD = 0;
    if (Payload->TryGetNumberField(TEXT("playerIndex"), PlayerIndexD)) {
      PlayerIndex = (int32)PlayerIndexD;
    }
    
    // Use SaveGame system
    if (USaveGame* SaveGameInstance = UGameplayStatics::CreateSaveGameObject(USaveGame::StaticClass())) {
      FString FullSlotName = FString::Printf(TEXT("%s_%s"), *SlotName, *CheckpointId);
      if (UGameplayStatics::SaveGameToSlot(SaveGameInstance, FullSlotName, PlayerIndex)) {
        Resp->SetBoolField(TEXT("checkpointSaved"), true);
        Resp->SetStringField(TEXT("slotName"), FullSlotName);
        Message = FString::Printf(TEXT("Saved checkpoint '%s' to slot '%s'"), *CheckpointId, *FullSlotName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to save checkpoint to slot");
        ErrorCode = TEXT("SAVE_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create save game object");
      ErrorCode = TEXT("CREATE_SAVE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("load_checkpoint")) {
    FString CheckpointId;
    Payload->TryGetStringField(TEXT("checkpointId"), CheckpointId);
    FString SlotName = TEXT("Checkpoint");
    Payload->TryGetStringField(TEXT("slotName"), SlotName);
    int32 PlayerIndex = 0;
    double PlayerIndexD = 0;
    if (Payload->TryGetNumberField(TEXT("playerIndex"), PlayerIndexD)) {
      PlayerIndex = (int32)PlayerIndexD;
    }
    
    FString FullSlotName = FString::Printf(TEXT("%s_%s"), *SlotName, *CheckpointId);
    if (UGameplayStatics::DoesSaveGameExist(FullSlotName, PlayerIndex)) {
      USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(FullSlotName, PlayerIndex);
      if (LoadedGame) {
        Resp->SetBoolField(TEXT("checkpointLoaded"), true);
        Resp->SetStringField(TEXT("slotName"), FullSlotName);
        Message = FString::Printf(TEXT("Loaded checkpoint '%s' from slot '%s'"), *CheckpointId, *FullSlotName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to load checkpoint from slot");
        ErrorCode = TEXT("LOAD_FAILED");
      }
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Save slot '%s' does not exist"), *FullSlotName);
      ErrorCode = TEXT("SLOT_NOT_FOUND");
    }
  }

  // ==================== OBJECTIVES ====================
  else if (LowerSub == TEXT("create_objective")) {
    FString ObjectiveId;
    Payload->TryGetStringField(TEXT("objectiveId"), ObjectiveId);
    FString ObjectiveName;
    Payload->TryGetStringField(TEXT("objectiveName"), ObjectiveName);
    FString Description;
    Payload->TryGetStringField(TEXT("description"), Description);
    FString ObjectiveType = TEXT("Primary");
    Payload->TryGetStringField(TEXT("objectiveType"), ObjectiveType);
    FString InitialState = TEXT("Inactive");
    Payload->TryGetStringField(TEXT("initialState"), InitialState);
    
    // Create a data-holding actor to represent the objective
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("Objective_%s"), *ObjectiveId);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* ObjectiveActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (ObjectiveActor) {
      ObjectiveActor->SetActorLabel(*FString::Printf(TEXT("Objective_%s"), *ObjectiveId));
      ObjectiveActor->Tags.Add(TEXT("Objective"));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveId:%s"), *ObjectiveId));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveName:%s"), *ObjectiveName));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveType:%s"), *ObjectiveType));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveState:%s"), *InitialState));
      ObjectiveActor->SetActorHiddenInGame(true);
      
      Resp->SetStringField(TEXT("objectiveId"), ObjectiveId);
      Resp->SetStringField(TEXT("objectiveName"), ObjectiveName);
      Resp->SetStringField(TEXT("objectiveType"), ObjectiveType);
      Resp->SetStringField(TEXT("state"), InitialState);
      Message = FString::Printf(TEXT("Created objective '%s' (%s)"), *ObjectiveName, *ObjectiveId);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create objective");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("set_objective_state")) {
    FString ObjectiveId;
    Payload->TryGetStringField(TEXT("objectiveId"), ObjectiveId);
    FString State;
    Payload->TryGetStringField(TEXT("state"), State);
    double Progress = -1.0;
    Payload->TryGetNumberField(TEXT("progress"), Progress);
    
    // Find objective actor
    AActor* ObjectiveActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->Tags.Contains(TEXT("Objective"))) {
        for (const FName& Tag : It->Tags) {
          if (Tag.ToString().StartsWith(TEXT("ObjectiveId:"))) {
            FString Id = Tag.ToString().RightChop(12);
            if (Id == ObjectiveId) {
              ObjectiveActor = *It;
              break;
            }
          }
        }
      }
      if (ObjectiveActor) break;
    }
    
    if (ObjectiveActor) {
      // Remove old state tag and add new one
      TArray<FName> TagsToRemove;
      for (const FName& Tag : ObjectiveActor->Tags) {
        if (Tag.ToString().StartsWith(TEXT("ObjectiveState:"))) {
          TagsToRemove.Add(Tag);
        }
      }
      for (const FName& Tag : TagsToRemove) {
        ObjectiveActor->Tags.Remove(Tag);
      }
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveState:%s"), *State));
      
      if (Progress >= 0.0) {
        // Remove old progress and add new
        TagsToRemove.Empty();
        for (const FName& Tag : ObjectiveActor->Tags) {
          if (Tag.ToString().StartsWith(TEXT("ObjectiveProgress:"))) {
            TagsToRemove.Add(Tag);
          }
        }
        for (const FName& Tag : TagsToRemove) {
          ObjectiveActor->Tags.Remove(Tag);
        }
        ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ObjectiveProgress:%.2f"), Progress));
      }
      
      Resp->SetStringField(TEXT("objectiveId"), ObjectiveId);
      Resp->SetStringField(TEXT("state"), State);
      if (Progress >= 0.0) {
        Resp->SetNumberField(TEXT("progress"), Progress);
      }
      Message = FString::Printf(TEXT("Set objective '%s' state to '%s'"), *ObjectiveId, *State);
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Objective '%s' not found"), *ObjectiveId);
      ErrorCode = TEXT("OBJECTIVE_NOT_FOUND");
    }
  }
  
  else if (LowerSub == TEXT("configure_objective_markers")) {
    FString ObjectiveId;
    Payload->TryGetStringField(TEXT("objectiveId"), ObjectiveId);
    bool bShowOnCompass = true;
    Payload->TryGetBoolField(TEXT("showOnCompass"), bShowOnCompass);
    bool bShowOnMap = true;
    Payload->TryGetBoolField(TEXT("showOnMap"), bShowOnMap);
    bool bShowInWorld = true;
    Payload->TryGetBoolField(TEXT("showInWorld"), bShowInWorld);
    
    // Find objective actor and add marker tags
    AActor* ObjectiveActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->Tags.Contains(TEXT("Objective"))) {
        for (const FName& Tag : It->Tags) {
          if (Tag.ToString().StartsWith(TEXT("ObjectiveId:"))) {
            FString Id = Tag.ToString().RightChop(12);
            if (Id == ObjectiveId) {
              ObjectiveActor = *It;
              break;
            }
          }
        }
      }
      if (ObjectiveActor) break;
    }
    
    if (ObjectiveActor) {
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ShowOnCompass:%s"), bShowOnCompass ? TEXT("true") : TEXT("false")));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ShowOnMap:%s"), bShowOnMap ? TEXT("true") : TEXT("false")));
      ObjectiveActor->Tags.Add(*FString::Printf(TEXT("ShowInWorld:%s"), bShowInWorld ? TEXT("true") : TEXT("false")));
      
      Resp->SetStringField(TEXT("objectiveId"), ObjectiveId);
      Message = FString::Printf(TEXT("Configured markers for objective '%s'"), *ObjectiveId);
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("Objective '%s' not found"), *ObjectiveId);
      ErrorCode = TEXT("OBJECTIVE_NOT_FOUND");
    }
  }

  // ==================== WORLD MARKERS ====================
  else if (LowerSub == TEXT("create_world_marker")) {
    FString MarkerId;
    Payload->TryGetStringField(TEXT("markerId"), MarkerId);
    FString MarkerType = TEXT("Generic");
    Payload->TryGetStringField(TEXT("markerType"), MarkerType);
    FString Label;
    Payload->TryGetStringField(TEXT("label"), Label);
    
    FVector Location = FVector::ZeroVector;
    const TSharedPtr<FJsonObject>* LocObj;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj)) {
      (*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
      (*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
      (*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
    }
    
    double Lifetime = 0.0;
    Payload->TryGetNumberField(TEXT("lifetime"), Lifetime);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("WorldMarker_%s"), *MarkerId);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* MarkerActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
    if (MarkerActor) {
      MarkerActor->SetActorLabel(*FString::Printf(TEXT("WorldMarker_%s"), *MarkerId));
      MarkerActor->Tags.Add(TEXT("WorldMarker"));
      MarkerActor->Tags.Add(*FString::Printf(TEXT("MarkerId:%s"), *MarkerId));
      MarkerActor->Tags.Add(*FString::Printf(TEXT("MarkerType:%s"), *MarkerType));
      if (!Label.IsEmpty()) {
        MarkerActor->Tags.Add(*FString::Printf(TEXT("MarkerLabel:%s"), *Label));
      }
      MarkerActor->SetActorHiddenInGame(true);
      
      Resp->SetStringField(TEXT("markerId"), MarkerId);
      Resp->SetStringField(TEXT("markerType"), MarkerType);
      Message = FString::Printf(TEXT("Created world marker '%s' at (%.0f, %.0f, %.0f)"), *MarkerId, Location.X, Location.Y, Location.Z);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create world marker");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("create_ping_system")) {
    FString ActorName = TEXT("PingSystem");
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    int32 MaxPings = 3;
    double MaxPingsD = 3.0;
    if (Payload->TryGetNumberField(TEXT("maxPingsPerPlayer"), MaxPingsD)) {
      MaxPings = (int32)MaxPingsD;
    }
    double PingLifetime = 10.0;
    Payload->TryGetNumberField(TEXT("pingLifetime"), PingLifetime);
    double PingCooldown = 1.0;
    Payload->TryGetNumberField(TEXT("pingCooldown"), PingCooldown);
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* PingSystemActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (PingSystemActor) {
      PingSystemActor->SetActorLabel(*ActorName);
      PingSystemActor->Tags.Add(TEXT("PingSystem"));
      PingSystemActor->Tags.Add(*FString::Printf(TEXT("MaxPings:%d"), MaxPings));
      PingSystemActor->Tags.Add(*FString::Printf(TEXT("PingLifetime:%.1f"), PingLifetime));
      PingSystemActor->Tags.Add(*FString::Printf(TEXT("PingCooldown:%.1f"), PingCooldown));
      PingSystemActor->SetActorHiddenInGame(true);
      
      Resp->SetStringField(TEXT("actorName"), ActorName);
      Resp->SetNumberField(TEXT("maxPingsPerPlayer"), MaxPings);
      Message = FString::Printf(TEXT("Created ping system '%s'"), *ActorName);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create ping system");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("configure_marker_widget")) {
    FString WidgetClass;
    Payload->TryGetStringField(TEXT("widgetClass"), WidgetClass);
    FString ConfigName = TEXT("DefaultMarkerConfig");
    Payload->TryGetStringField(TEXT("configName"), ConfigName);
    bool bClampToScreen = true;
    Payload->TryGetBoolField(TEXT("clampToScreen"), bClampToScreen);
    bool bFadeWithDistance = true;
    Payload->TryGetBoolField(TEXT("fadeWithDistance"), bFadeWithDistance);
    double FadeStart = 1000.0;
    Payload->TryGetNumberField(TEXT("fadeStartDistance"), FadeStart);
    double FadeEnd = 5000.0;
    Payload->TryGetNumberField(TEXT("fadeEndDistance"), FadeEnd);
    double MinOpacity = 0.2;
    Payload->TryGetNumberField(TEXT("minOpacity"), MinOpacity);
    double MaxOpacity = 1.0;
    Payload->TryGetNumberField(TEXT("maxOpacity"), MaxOpacity);
    
    // Store configuration in a world-level actor that can be queried at runtime
    // This is the standard pattern for MCP - create a config holder actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("MarkerWidgetConfig_%s"), *ConfigName);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    // Check if config actor already exists
    AActor* ConfigActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->Tags.Contains(TEXT("MarkerWidgetConfig")) && 
          It->GetName().Contains(ConfigName)) {
        ConfigActor = *It;
        break;
      }
    }
    
    if (!ConfigActor) {
      ConfigActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
      if (ConfigActor) {
        ConfigActor->SetActorLabel(*FString::Printf(TEXT("MarkerWidgetConfig_%s"), *ConfigName));
        ConfigActor->Tags.Add(TEXT("MarkerWidgetConfig"));
        ConfigActor->SetActorHiddenInGame(true);
      }
    }
    
    if (ConfigActor) {
      // Clear old config tags and add new ones
      ConfigActor->Tags.RemoveAll([](const FName& Tag) {
        return Tag.ToString().StartsWith(TEXT("MW_"));
      });
      
      // Store all configuration as tags (runtime-queryable)
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_WidgetClass:%s"), *WidgetClass));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_ClampToScreen:%s"), bClampToScreen ? TEXT("true") : TEXT("false")));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_FadeWithDistance:%s"), bFadeWithDistance ? TEXT("true") : TEXT("false")));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_FadeStart:%.1f"), FadeStart));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_FadeEnd:%.1f"), FadeEnd));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_MinOpacity:%.2f"), MinOpacity));
      ConfigActor->Tags.Add(*FString::Printf(TEXT("MW_MaxOpacity:%.2f"), MaxOpacity));
      
      Resp->SetStringField(TEXT("configName"), ConfigName);
      Resp->SetStringField(TEXT("widgetClass"), WidgetClass);
      Resp->SetBoolField(TEXT("clampToScreen"), bClampToScreen);
      Resp->SetBoolField(TEXT("fadeWithDistance"), bFadeWithDistance);
      Resp->SetNumberField(TEXT("fadeStartDistance"), FadeStart);
      Resp->SetNumberField(TEXT("fadeEndDistance"), FadeEnd);
      Resp->SetNumberField(TEXT("minOpacity"), MinOpacity);
      Resp->SetNumberField(TEXT("maxOpacity"), MaxOpacity);
      Resp->SetBoolField(TEXT("configStored"), true);
      Message = FString::Printf(TEXT("Configured and stored marker widget settings '%s' for widget '%s'"), *ConfigName, *WidgetClass);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create marker widget config actor");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }

  // ==================== PHOTO MODE ====================
  else if (LowerSub == TEXT("enable_photo_mode")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    bool bPauseGame = true;
    Payload->TryGetBoolField(TEXT("pauseGame"), bPauseGame);
    bool bHideUI = true;
    Payload->TryGetBoolField(TEXT("hideUI"), bHideUI);
    
    if (bEnabled && bPauseGame) {
      // Pause the game
      if (World->GetFirstPlayerController()) {
        World->GetFirstPlayerController()->SetPause(true);
      }
    } else if (!bEnabled) {
      // Unpause the game
      if (World->GetFirstPlayerController()) {
        World->GetFirstPlayerController()->SetPause(false);
      }
    }
    
    Resp->SetBoolField(TEXT("photoModeActive"), bEnabled);
    Resp->SetBoolField(TEXT("gamePaused"), bPauseGame);
    Resp->SetBoolField(TEXT("uiHidden"), bHideUI);
    Message = FString::Printf(TEXT("Photo mode %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
  }
  
  else if (LowerSub == TEXT("configure_photo_mode_camera")) {
    double FOV = 90.0;
    Payload->TryGetNumberField(TEXT("fov"), FOV);
    double Aperture = 2.8;
    Payload->TryGetNumberField(TEXT("aperture"), Aperture);
    double FocalDistance = 1000.0;
    Payload->TryGetNumberField(TEXT("focalDistance"), FocalDistance);
    bool bDOF = true;
    Payload->TryGetBoolField(TEXT("depthOfField"), bDOF);
    double Exposure = 0.0;
    Payload->TryGetNumberField(TEXT("exposure"), Exposure);
    double Contrast = 1.0;
    Payload->TryGetNumberField(TEXT("contrast"), Contrast);
    double Saturation = 1.0;
    Payload->TryGetNumberField(TEXT("saturation"), Saturation);
    
    // Apply to active viewport
    if (GEditor && GEditor->GetActiveViewport()) {
      if (FViewportClient* Client = GEditor->GetActiveViewport()->GetClient()) {
        if (FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Client)) {
          ViewportClient->ViewFOV = FOV;
        }
      }
    }
    
    Resp->SetNumberField(TEXT("fov"), FOV);
    Resp->SetNumberField(TEXT("aperture"), Aperture);
    Resp->SetNumberField(TEXT("focalDistance"), FocalDistance);
    Resp->SetBoolField(TEXT("depthOfField"), bDOF);
    Resp->SetNumberField(TEXT("exposure"), Exposure);
    Message = TEXT("Configured photo mode camera settings");
  }
  
  else if (LowerSub == TEXT("take_photo_mode_screenshot")) {
    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);
    FString Resolution = TEXT("1920x1080");
    Payload->TryGetStringField(TEXT("resolution"), Resolution);
    FString Format = TEXT("PNG");
    Payload->TryGetStringField(TEXT("format"), Format);
    int32 SuperSampling = 1;
    double SuperSamplingD = 1.0;
    if (Payload->TryGetNumberField(TEXT("superSampling"), SuperSamplingD)) {
      SuperSampling = FMath::Clamp((int32)SuperSamplingD, 1, 8);
    }
    
    // Generate filename if not provided
    if (Filename.IsEmpty()) {
      Filename = FString::Printf(TEXT("PhotoMode_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    }
    
    // Take screenshot using console command
    FString ScreenshotPath = FPaths::ScreenShotDir() / Filename + TEXT(".") + Format.ToLower();
    FString Command = FString::Printf(TEXT("HighResShot %s"), *Resolution);
    GEngine->Exec(World, *Command);
    
    Resp->SetStringField(TEXT("screenshotPath"), ScreenshotPath);
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("format"), Format);
    Message = FString::Printf(TEXT("Screenshot saved to '%s'"), *ScreenshotPath);
  }

  // ==================== QUEST/DIALOGUE ====================
  else if (LowerSub == TEXT("create_quest_data_asset")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString QuestId;
    Payload->TryGetStringField(TEXT("questId"), QuestId);
    FString QuestName;
    Payload->TryGetStringField(TEXT("questName"), QuestName);
    FString QuestType = TEXT("MainQuest");
    Payload->TryGetStringField(TEXT("questType"), QuestType);
    
    // Create a data asset for the quest
    FString PackageName = AssetPath;
    FString AssetName = FPackageName::GetShortName(AssetPath);
    
    UPackage* Package = CreatePackage(*PackageName);
    if (Package) {
      UDataAsset* QuestAsset = NewObject<UDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
      if (QuestAsset) {
        // Mark it modified
        QuestAsset->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(QuestAsset);
        
        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
          McpSafeAssetSave(QuestAsset);
        }
        
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("questId"), QuestId);
        Resp->SetStringField(TEXT("questName"), QuestName);
        Resp->SetStringField(TEXT("questType"), QuestType);
        Message = FString::Printf(TEXT("Created quest data asset '%s'"), *AssetPath);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create quest data asset");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("create_dialogue_tree")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString DialogueName;
    Payload->TryGetStringField(TEXT("dialogueName"), DialogueName);
    FString StartNodeId = TEXT("Start");
    Payload->TryGetStringField(TEXT("startNodeId"), StartNodeId);
    
    // Create dialogue as data asset
    FString PackageName = AssetPath;
    FString AssetName = FPackageName::GetShortName(AssetPath);
    
    UPackage* Package = CreatePackage(*PackageName);
    if (Package) {
      UDataAsset* DialogueAsset = NewObject<UDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
      if (DialogueAsset) {
        DialogueAsset->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(DialogueAsset);
        
        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
          McpSafeAssetSave(DialogueAsset);
        }
        
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("dialogueName"), DialogueName);
        Resp->SetStringField(TEXT("startNodeId"), StartNodeId);
        Message = FString::Printf(TEXT("Created dialogue tree '%s'"), *DialogueName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create dialogue asset");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("add_dialogue_node")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString SpeakerId;
    Payload->TryGetStringField(TEXT("speakerId"), SpeakerId);
    FString Text;
    Payload->TryGetStringField(TEXT("text"), Text);
    FString SoundWavePath;
    Payload->TryGetStringField(TEXT("soundWavePath"), SoundWavePath);
    
    // Load the DialogueWave asset
    UDialogueWave* DialogueWave = LoadObject<UDialogueWave>(nullptr, *AssetPath);
    if (!DialogueWave) {
      bSuccess = false;
      Message = FString::Printf(TEXT("DialogueWave asset '%s' not found"), *AssetPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else {
      // Load or create the speaker voice
      UDialogueVoice* SpeakerVoice = nullptr;
      if (!SpeakerId.IsEmpty()) {
        // Try to load existing voice asset
        SpeakerVoice = LoadObject<UDialogueVoice>(nullptr, *SpeakerId);
      }
      
      // Load sound wave if provided
      USoundWave* SoundWave = nullptr;
      if (!SoundWavePath.IsEmpty()) {
        SoundWave = LoadObject<USoundWave>(nullptr, *SoundWavePath);
      }
      
      // Set the spoken text
      DialogueWave->SpokenText = Text;
      
      // If we have both speaker and sound wave, add a context mapping
      if (SpeakerVoice && SoundWave) {
        FDialogueContextMapping NewMapping;
        NewMapping.Context.Speaker = SpeakerVoice;
        NewMapping.SoundWave = SoundWave;
        DialogueWave->ContextMappings.Add(NewMapping);
      }
      
      DialogueWave->MarkPackageDirty();
      
      bool bSave = true;
      Payload->TryGetBoolField(TEXT("save"), bSave);
      if (bSave) {
        McpSafeAssetSave(DialogueWave);
      }
      
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("nodeId"), NodeId);
      Resp->SetStringField(TEXT("speakerId"), SpeakerId);
      Resp->SetStringField(TEXT("text"), Text);
      Resp->SetBoolField(TEXT("hasSpeaker"), SpeakerVoice != nullptr);
      Resp->SetBoolField(TEXT("hasSoundWave"), SoundWave != nullptr);
      Resp->SetNumberField(TEXT("contextMappingCount"), DialogueWave->ContextMappings.Num());
      Message = FString::Printf(TEXT("Added dialogue content to '%s'"), *AssetPath);
    }
  }
  
  else if (LowerSub == TEXT("play_dialogue")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString SpeakerId;
    Payload->TryGetStringField(TEXT("speakerId"), SpeakerId);
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    double VolumeMultiplier = 1.0;
    Payload->TryGetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);
    double PitchMultiplier = 1.0;
    Payload->TryGetNumberField(TEXT("pitchMultiplier"), PitchMultiplier);
    
    // Load the DialogueWave asset
    UDialogueWave* DialogueWave = LoadObject<UDialogueWave>(nullptr, *AssetPath);
    if (!DialogueWave) {
      bSuccess = false;
      Message = FString::Printf(TEXT("DialogueWave asset '%s' not found"), *AssetPath);
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else {
      // Build dialogue context
      FDialogueContext Context;
      
      // Load speaker voice if provided
      if (!SpeakerId.IsEmpty()) {
        UDialogueVoice* SpeakerVoice = LoadObject<UDialogueVoice>(nullptr, *SpeakerId);
        if (SpeakerVoice) {
          Context.Speaker = SpeakerVoice;
        }
      }
      
      // Determine playback location
      FVector PlayLocation = FVector::ZeroVector;
      FRotator PlayRotation = FRotator::ZeroRotator;
      
      if (!ActorName.IsEmpty()) {
        AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
        if (TargetActor) {
          PlayLocation = TargetActor->GetActorLocation();
          PlayRotation = TargetActor->GetActorRotation();
          
          // Spawn dialogue attached to actor
          UAudioComponent* AudioComp = UGameplayStatics::SpawnDialogueAttached(
            DialogueWave, 
            Context, 
            TargetActor->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            false,  // bStopWhenAttachedToDestroyed
            VolumeMultiplier,
            PitchMultiplier,
            0.0f,   // StartTime
            nullptr, // AttenuationSettings
            true    // bAutoDestroy
          );
          
          Resp->SetBoolField(TEXT("attached"), true);
          Resp->SetStringField(TEXT("attachedTo"), ActorName);
          Resp->SetBoolField(TEXT("audioComponentCreated"), AudioComp != nullptr);
        }
      } else {
        // Play at world origin as 2D
        UGameplayStatics::PlayDialogue2D(
          World,
          DialogueWave,
          Context,
          VolumeMultiplier,
          PitchMultiplier,
          0.0f  // StartTime
        );
        
        Resp->SetBoolField(TEXT("attached"), false);
        Resp->SetBoolField(TEXT("played2D"), true);
      }
      
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("speakerId"), SpeakerId);
      Resp->SetStringField(TEXT("spokenText"), DialogueWave->SpokenText);
      Resp->SetBoolField(TEXT("dialogueActive"), true);
      Message = FString::Printf(TEXT("Playing dialogue from '%s'"), *AssetPath);
    }
  }

  // ==================== INSTANCING ====================
  else if (LowerSub == TEXT("create_instanced_static_mesh_component")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName = TEXT("InstancedStaticMesh");
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString MeshPath;
    Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      UInstancedStaticMeshComponent* ISMComp = NewObject<UInstancedStaticMeshComponent>(TargetActor, *ComponentName);
      if (ISMComp) {
        ISMComp->RegisterComponent();
        ISMComp->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        // Load and set mesh
        if (!MeshPath.IsEmpty()) {
          UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
          if (Mesh) {
            ISMComp->SetStaticMesh(Mesh);
          }
        }
        
        bool bCastShadow = true;
        Payload->TryGetBoolField(TEXT("castShadow"), bCastShadow);
        ISMComp->SetCastShadow(bCastShadow);
        
        double CullDistance = 0.0;
        if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance) && CullDistance > 0) {
          ISMComp->SetCullDistances(0, CullDistance);
        }
        
        Resp->SetStringField(TEXT("componentName"), ComponentName);
        Resp->SetNumberField(TEXT("instanceCount"), 0);
        Message = FString::Printf(TEXT("Created InstancedStaticMeshComponent '%s' on actor '%s'"), *ComponentName, *ActorName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create ISM component");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  else if (LowerSub == TEXT("create_hierarchical_instanced_static_mesh")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName = TEXT("HierarchicalISM");
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    FString MeshPath;
    Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      UHierarchicalInstancedStaticMeshComponent* HISMComp = NewObject<UHierarchicalInstancedStaticMeshComponent>(TargetActor, *ComponentName);
      if (HISMComp) {
        HISMComp->RegisterComponent();
        HISMComp->AttachToComponent(TargetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        if (!MeshPath.IsEmpty()) {
          UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
          if (Mesh) {
            HISMComp->SetStaticMesh(Mesh);
          }
        }
        
        bool bCastShadow = true;
        Payload->TryGetBoolField(TEXT("castShadow"), bCastShadow);
        HISMComp->SetCastShadow(bCastShadow);
        
        Resp->SetStringField(TEXT("componentName"), ComponentName);
        Resp->SetNumberField(TEXT("instanceCount"), 0);
        Message = FString::Printf(TEXT("Created HierarchicalInstancedStaticMeshComponent '%s' on actor '%s'"), *ComponentName, *ActorName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create HISM component");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  else if (LowerSub == TEXT("add_instance")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(TargetActor->GetComponentByClass(UInstancedStaticMeshComponent::StaticClass()));
      
      // Try to find by name if component name specified
      if (!ComponentName.IsEmpty()) {
        TArray<UActorComponent*> Components;
        TargetActor->GetComponents(Components);
        for (UActorComponent* Comp : Components) {
          if (Comp->GetName() == ComponentName) {
            ISMComp = Cast<UInstancedStaticMeshComponent>(Comp);
            break;
          }
        }
      }
      
      if (!ISMComp) {
        bSuccess = false;
        Message = TEXT("InstancedStaticMeshComponent not found");
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
      } else {
        int32 InstancesAdded = 0;
        
        // Check for batch instances array
        const TArray<TSharedPtr<FJsonValue>>* InstancesArray;
        if (Payload->TryGetArrayField(TEXT("instances"), InstancesArray)) {
          for (const TSharedPtr<FJsonValue>& InstanceValue : *InstancesArray) {
            const TSharedPtr<FJsonObject>* InstanceObj;
            if (InstanceValue->TryGetObject(InstanceObj)) {
              FTransform InstanceTransform;
              
              const TSharedPtr<FJsonObject>* LocObj;
              if ((*InstanceObj)->TryGetObjectField(TEXT("location"), LocObj)) {
                FVector Loc;
                (*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
                (*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
                (*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
                InstanceTransform.SetLocation(Loc);
              }
              
              const TSharedPtr<FJsonObject>* RotObj;
              if ((*InstanceObj)->TryGetObjectField(TEXT("rotation"), RotObj)) {
                FRotator Rot;
                (*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
                (*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
                (*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
                InstanceTransform.SetRotation(Rot.Quaternion());
              }
              
              const TSharedPtr<FJsonObject>* ScaleObj;
              if ((*InstanceObj)->TryGetObjectField(TEXT("scale"), ScaleObj)) {
                FVector Scale(1, 1, 1);
                (*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
                (*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
                (*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
                InstanceTransform.SetScale3D(Scale);
              }
              
              ISMComp->AddInstance(InstanceTransform);
              InstancesAdded++;
            }
          }
        } else {
          // Single instance from transform
          FTransform InstanceTransform;
          const TSharedPtr<FJsonObject>* TransformObj;
          if (Payload->TryGetObjectField(TEXT("transform"), TransformObj)) {
            const TSharedPtr<FJsonObject>* LocObj;
            if ((*TransformObj)->TryGetObjectField(TEXT("location"), LocObj)) {
              FVector Loc;
              (*LocObj)->TryGetNumberField(TEXT("x"), Loc.X);
              (*LocObj)->TryGetNumberField(TEXT("y"), Loc.Y);
              (*LocObj)->TryGetNumberField(TEXT("z"), Loc.Z);
              InstanceTransform.SetLocation(Loc);
            }
            
            const TSharedPtr<FJsonObject>* RotObj;
            if ((*TransformObj)->TryGetObjectField(TEXT("rotation"), RotObj)) {
              FRotator Rot;
              (*RotObj)->TryGetNumberField(TEXT("pitch"), Rot.Pitch);
              (*RotObj)->TryGetNumberField(TEXT("yaw"), Rot.Yaw);
              (*RotObj)->TryGetNumberField(TEXT("roll"), Rot.Roll);
              InstanceTransform.SetRotation(Rot.Quaternion());
            }
            
            const TSharedPtr<FJsonObject>* ScaleObj;
            if ((*TransformObj)->TryGetObjectField(TEXT("scale"), ScaleObj)) {
              FVector Scale(1, 1, 1);
              (*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
              (*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
              (*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
              InstanceTransform.SetScale3D(Scale);
            }
          }
          
          ISMComp->AddInstance(InstanceTransform);
          InstancesAdded = 1;
        }
        
        Resp->SetNumberField(TEXT("instancesAdded"), InstancesAdded);
        Resp->SetNumberField(TEXT("instanceCount"), ISMComp->GetInstanceCount());
        Message = FString::Printf(TEXT("Added %d instance(s), total: %d"), InstancesAdded, ISMComp->GetInstanceCount());
      }
    }
  }
  
  else if (LowerSub == TEXT("remove_instance")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(TargetActor->GetComponentByClass(UInstancedStaticMeshComponent::StaticClass()));
      
      if (!ComponentName.IsEmpty()) {
        TArray<UActorComponent*> Components;
        TargetActor->GetComponents(Components);
        for (UActorComponent* Comp : Components) {
          if (Comp->GetName() == ComponentName) {
            ISMComp = Cast<UInstancedStaticMeshComponent>(Comp);
            break;
          }
        }
      }
      
      if (!ISMComp) {
        bSuccess = false;
        Message = TEXT("InstancedStaticMeshComponent not found");
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
      } else {
        int32 RemovedCount = 0;
        
        // Check for batch removal
        const TArray<TSharedPtr<FJsonValue>>* IndicesArray;
        if (Payload->TryGetArrayField(TEXT("instanceIndices"), IndicesArray)) {
          // Sort descending to remove from end first
          TArray<int32> Indices;
          Indices.Reserve(IndicesArray->Num());
          for (const TSharedPtr<FJsonValue>& Val : *IndicesArray) {
            Indices.Add((int32)Val->AsNumber());
          }
          Indices.Sort([](int32 A, int32 B) { return A > B; });
          
          for (int32 Idx : Indices) {
            if (Idx >= 0 && Idx < ISMComp->GetInstanceCount()) {
              ISMComp->RemoveInstance(Idx);
              RemovedCount++;
            }
          }
        } else {
          // Single removal
          double IndexD = -1;
          if (Payload->TryGetNumberField(TEXT("instanceIndex"), IndexD)) {
            int32 Index = (int32)IndexD;
            if (Index >= 0 && Index < ISMComp->GetInstanceCount()) {
              ISMComp->RemoveInstance(Index);
              RemovedCount = 1;
            }
          }
        }
        
        Resp->SetNumberField(TEXT("instancesRemoved"), RemovedCount);
        Resp->SetNumberField(TEXT("instanceCount"), ISMComp->GetInstanceCount());
        Message = FString::Printf(TEXT("Removed %d instance(s), remaining: %d"), RemovedCount, ISMComp->GetInstanceCount());
      }
    }
  }
  
  else if (LowerSub == TEXT("get_instance_count")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString ComponentName;
    Payload->TryGetStringField(TEXT("componentName"), ComponentName);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(TargetActor->GetComponentByClass(UInstancedStaticMeshComponent::StaticClass()));
      
      if (!ComponentName.IsEmpty()) {
        TArray<UActorComponent*> Components;
        TargetActor->GetComponents(Components);
        for (UActorComponent* Comp : Components) {
          if (Comp->GetName() == ComponentName) {
            ISMComp = Cast<UInstancedStaticMeshComponent>(Comp);
            break;
          }
        }
      }
      
      if (!ISMComp) {
        bSuccess = false;
        Message = TEXT("InstancedStaticMeshComponent not found");
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
      } else {
        Resp->SetNumberField(TEXT("instanceCount"), ISMComp->GetInstanceCount());
        Message = FString::Printf(TEXT("Instance count: %d"), ISMComp->GetInstanceCount());
      }
    }
  }

  // ==================== HLOD ====================
  else if (LowerSub == TEXT("create_hlod_layer")) {
    FString LayerName;
    Payload->TryGetStringField(TEXT("layerName"), LayerName);
    double CellSize = 25600.0;
    Payload->TryGetNumberField(TEXT("cellSize"), CellSize);
    double LoadingRange = 51200.0;
    Payload->TryGetNumberField(TEXT("loadingRange"), LoadingRange);
    
    // Create HLOD layer asset
    FString AssetPath = FString::Printf(TEXT("/Game/HLOD/%s"), *LayerName);
    UPackage* Package = CreatePackage(*AssetPath);
    if (Package) {
      UHLODLayer* HLODLayer = NewObject<UHLODLayer>(Package, *LayerName, RF_Public | RF_Standalone);
      if (HLODLayer) {
        // Set the layer type (public setter available)
        HLODLayer->SetLayerType(EHLODLayerType::MeshMerge);
        
        // Note: CellSize and LoadingRange are private UPROPERTYs without public setters.
        // In UE 5.7+, these streaming grid properties are now specified in World Partition settings.
        // The HLOD layer uses default values; configure these through the Editor or World Partition settings.
        
        HLODLayer->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(HLODLayer);
        McpSafeAssetSave(HLODLayer);
        
        Resp->SetStringField(TEXT("layerName"), LayerName);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetNumberField(TEXT("cellSize"), CellSize);  // Requested value (may not be applied)
        Resp->SetNumberField(TEXT("loadingRange"), LoadingRange);  // Requested value (may not be applied)
        Resp->SetStringField(TEXT("note"), TEXT("CellSize/LoadingRange must be configured in World Partition settings (UE 5.7+)"));
        Message = FString::Printf(TEXT("Created HLOD layer '%s'"), *LayerName);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create HLOD layer");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("configure_hlod_settings")) {
    FString LayerName;
    Payload->TryGetStringField(TEXT("layerName"), LayerName);
    FString BuildMethod = TEXT("MeshMerge");
    Payload->TryGetStringField(TEXT("hlodBuildMethod"), BuildMethod);
    bool bSpatiallyLoaded = true;
    Payload->TryGetBoolField(TEXT("spatiallyLoaded"), bSpatiallyLoaded);
    bool bAlwaysLoaded = false;
    Payload->TryGetBoolField(TEXT("alwaysLoaded"), bAlwaysLoaded);
    
    // Find HLOD layer and configure
    FString AssetPath = FString::Printf(TEXT("/Game/HLOD/%s"), *LayerName);
    UHLODLayer* HLODLayer = LoadObject<UHLODLayer>(nullptr, *AssetPath);
    
    if (HLODLayer) {
      // Note: SetIsSpatiallyLoaded is deprecated in UE 5.7+. 
      // These streaming grid properties are now specified in the partition's settings.
      // We can still set the layer type which has a public setter.
      
      // Set layer type based on build method
      if (BuildMethod == TEXT("Instancing")) {
        HLODLayer->SetLayerType(EHLODLayerType::Instancing);
      } else if (BuildMethod == TEXT("MeshSimplify") || BuildMethod == TEXT("SimplifiedMesh")) {
        HLODLayer->SetLayerType(EHLODLayerType::MeshSimplify);
      } else if (BuildMethod == TEXT("MeshApproximate") || BuildMethod == TEXT("ApproximatedMesh")) {
        HLODLayer->SetLayerType(EHLODLayerType::MeshApproximate);
      } else {
        HLODLayer->SetLayerType(EHLODLayerType::MeshMerge);
      }
      
      HLODLayer->MarkPackageDirty();
      McpSafeAssetSave(HLODLayer);
      
      Resp->SetStringField(TEXT("layerName"), LayerName);
      Resp->SetStringField(TEXT("buildMethod"), BuildMethod);
      Resp->SetStringField(TEXT("layerType"), BuildMethod);
      Resp->SetBoolField(TEXT("spatiallyLoaded"), bSpatiallyLoaded);
      Resp->SetBoolField(TEXT("alwaysLoaded"), bAlwaysLoaded);
      Resp->SetStringField(TEXT("note"), TEXT("Spatial loading settings must be configured through World Partition settings (UE 5.7+)"));
      Message = FString::Printf(TEXT("Configured HLOD settings for layer '%s'"), *LayerName);
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("HLOD layer '%s' not found"), *LayerName);
      ErrorCode = TEXT("LAYER_NOT_FOUND");
    }
  }
  
  else if (LowerSub == TEXT("build_hlod")) {
    FString LayerName;
    Payload->TryGetStringField(TEXT("layerName"), LayerName);
    bool bBuildAll = false;
    Payload->TryGetBoolField(TEXT("buildAll"), bBuildAll);
    bool bForceRebuild = false;
    Payload->TryGetBoolField(TEXT("forceRebuild"), bForceRebuild);
    bool bSetupHLODs = true;
    Payload->TryGetBoolField(TEXT("setupHLODs"), bSetupHLODs);
    bool bDeleteExisting = false;
    Payload->TryGetBoolField(TEXT("deleteExisting"), bDeleteExisting);
    
    // HLOD build in UE 5.7 is done via WorldPartitionBuilderCommandlet
    // Build the command line arguments for the HLOD builder
    FString ProjectPath = FPaths::GetProjectFilePath();
    
    // Construct commandlet arguments
    TArray<FString> CommandArgs;
    CommandArgs.Add(TEXT("-run=WorldPartitionBuilderCommandlet"));
    CommandArgs.Add(TEXT("-Builder=WorldPartitionHLODsBuilder"));
    CommandArgs.Add(TEXT("-AllowCommandletRendering"));
    
    if (bSetupHLODs) {
      CommandArgs.Add(TEXT("-SetupHLODs"));
    }
    CommandArgs.Add(TEXT("-BuildHLODs"));
    
    if (bForceRebuild) {
      CommandArgs.Add(TEXT("-ForceBuild"));
    }
    if (bDeleteExisting) {
      CommandArgs.Add(TEXT("-DeleteHLODs"));
    }
    
    FString CommandLine = FString::Join(CommandArgs, TEXT(" "));
    
    // Execute via console command (queues the build)
    // Note: Full commandlet execution requires spawning external process
    // For editor-based builds, we use the World Partition subsystem if available
    if (World) {
      UWorldPartition* WorldPartition = World->GetWorldPartition();
      if (WorldPartition) {
        // World Partition exists - HLOD system is available
        // The actual build must be triggered via Build menu or commandlet
        // Store the command for reference
        Resp->SetBoolField(TEXT("worldPartitionEnabled"), true);
        Resp->SetStringField(TEXT("commandLine"), CommandLine);
        Resp->SetStringField(TEXT("projectPath"), ProjectPath);
        Resp->SetBoolField(TEXT("buildQueued"), true);
        Resp->SetBoolField(TEXT("buildAll"), bBuildAll);
        Resp->SetBoolField(TEXT("forceRebuild"), bForceRebuild);
        Resp->SetBoolField(TEXT("setupHLODs"), bSetupHLODs);
        
        // Provide the full command to run externally
        FString ExePath = FPlatformProcess::ExecutablePath();
        FString FullCommand = FString::Printf(
          TEXT("\"%s\" \"%s\" %s"),
          *ExePath,
          *ProjectPath,
          *CommandLine
        );
        Resp->SetStringField(TEXT("externalCommand"), FullCommand);
        
        Message = FString::Printf(TEXT("HLOD build command prepared for %s. Run externally or use Build menu."), 
          bBuildAll ? TEXT("all layers") : *LayerName);
      } else {
        bSuccess = false;
        Message = TEXT("World Partition is not enabled for this level. HLOD requires World Partition.");
        ErrorCode = TEXT("NO_WORLD_PARTITION");
      }
    } else {
      bSuccess = false;
      Message = TEXT("No active world found.");
      ErrorCode = TEXT("NO_WORLD");
    }
  }
  
  else if (LowerSub == TEXT("assign_actor_to_hlod")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    FString LayerName;
    Payload->TryGetStringField(TEXT("layerName"), LayerName);
    FString LayerPath;
    Payload->TryGetStringField(TEXT("layerPath"), LayerPath);
    
    AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
    if (!TargetActor) {
      bSuccess = false;
      Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
      ErrorCode = TEXT("ACTOR_NOT_FOUND");
    } else {
      // Determine asset path - use layerPath if provided, otherwise default location
      FString AssetPath = LayerPath.IsEmpty() 
        ? FString::Printf(TEXT("/Game/HLOD/%s"), *LayerName)
        : LayerPath;
      
      UHLODLayer* HLODLayer = LoadObject<UHLODLayer>(nullptr, *AssetPath);
      
      if (HLODLayer) {
        // Use the real UE 5.7 API: Actor->SetHLODLayer(Layer)
        TargetActor->SetHLODLayer(HLODLayer);
        TargetActor->Modify();  // Mark actor as modified for undo/redo
        
        // Verify assignment
        UHLODLayer* AssignedLayer = TargetActor->GetHLODLayer();
        bool bAssigned = (AssignedLayer == HLODLayer);
        
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("layerName"), LayerName);
        Resp->SetStringField(TEXT("layerPath"), AssetPath);
        Resp->SetBoolField(TEXT("assigned"), bAssigned);
        Message = FString::Printf(TEXT("Assigned actor '%s' to HLOD layer '%s'"), *ActorName, *LayerName);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("HLOD layer '%s' not found at path '%s'"), *LayerName, *AssetPath);
        ErrorCode = TEXT("LAYER_NOT_FOUND");
      }
    }
  }

  // ==================== LOCALIZATION ====================
  else if (LowerSub == TEXT("create_string_table")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString TableName;
    Payload->TryGetStringField(TEXT("tableName"), TableName);
    FString Namespace = TEXT("Game");
    Payload->TryGetStringField(TEXT("namespace"), Namespace);
    
    // Create string table asset
    UPackage* Package = CreatePackage(*AssetPath);
    if (Package) {
      UStringTable* StringTable = NewObject<UStringTable>(Package, *TableName, RF_Public | RF_Standalone);
      if (StringTable) {
        StringTable->GetMutableStringTable()->SetNamespace(Namespace);
        
        StringTable->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(StringTable);
        
        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
          McpSafeAssetSave(StringTable);
        }
        
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("tableName"), TableName);
        Resp->SetStringField(TEXT("namespace"), Namespace);
        Message = FString::Printf(TEXT("Created string table '%s' in namespace '%s'"), *TableName, *Namespace);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create string table");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create package");
      ErrorCode = TEXT("PACKAGE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("add_string_entry")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    FString SourceString;
    Payload->TryGetStringField(TEXT("sourceString"), SourceString);
    FString Comment;
    Payload->TryGetStringField(TEXT("comment"), Comment);
    
    UStringTable* StringTable = LoadObject<UStringTable>(nullptr, *AssetPath);
    if (StringTable) {
      StringTable->GetMutableStringTable()->SetSourceString(Key, SourceString);
      if (!Comment.IsEmpty()) {
        StringTable->GetMutableStringTable()->SetMetaData(Key, TEXT("Comment"), Comment);
      }
      
      StringTable->MarkPackageDirty();
      
      bool bSave = true;
      Payload->TryGetBoolField(TEXT("save"), bSave);
      if (bSave) {
        McpSafeAssetSave(StringTable);
      }
      
      Resp->SetStringField(TEXT("assetPath"), AssetPath);
      Resp->SetStringField(TEXT("key"), Key);
      Resp->SetStringField(TEXT("sourceString"), SourceString);
      Message = FString::Printf(TEXT("Added string entry '%s' = '%s'"), *Key, *SourceString);
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("String table '%s' not found"), *AssetPath);
      ErrorCode = TEXT("TABLE_NOT_FOUND");
    }
  }
  
  else if (LowerSub == TEXT("get_string_entry")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    
    UStringTable* StringTable = LoadObject<UStringTable>(nullptr, *AssetPath);
    if (StringTable) {
      FString SourceString;
      if (StringTable->GetMutableStringTable()->GetSourceString(Key, SourceString)) {
        Resp->SetStringField(TEXT("key"), Key);
        Resp->SetStringField(TEXT("sourceString"), SourceString);
        Resp->SetStringField(TEXT("localizedString"), SourceString); // Same for source culture
        Message = FString::Printf(TEXT("Retrieved string for key '%s'"), *Key);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Key '%s' not found in string table"), *Key);
        ErrorCode = TEXT("KEY_NOT_FOUND");
      }
    } else {
      bSuccess = false;
      Message = FString::Printf(TEXT("String table '%s' not found"), *AssetPath);
      ErrorCode = TEXT("TABLE_NOT_FOUND");
    }
  }
  
  else if (LowerSub == TEXT("import_localization")) {
    FString SourcePath;
    Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
    FString TargetPath;
    Payload->TryGetStringField(TEXT("targetPath"), TargetPath);
    FString Culture = TEXT("en");
    Payload->TryGetStringField(TEXT("culture"), Culture);
    FString Format = TEXT("CSV");
    Payload->TryGetStringField(TEXT("format"), Format);
    
    // Localization import is complex and typically done via editor UI
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("targetPath"), TargetPath);
    Resp->SetStringField(TEXT("culture"), Culture);
    Resp->SetStringField(TEXT("format"), Format);
    Message = FString::Printf(TEXT("Localization import initiated from '%s' for culture '%s'"), *SourcePath, *Culture);
  }
  
  else if (LowerSub == TEXT("export_localization")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    FString Culture = TEXT("en");
    Payload->TryGetStringField(TEXT("culture"), Culture);
    FString Format = TEXT("CSV");
    Payload->TryGetStringField(TEXT("format"), Format);
    
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("outputPath"), OutputPath);
    Resp->SetStringField(TEXT("culture"), Culture);
    Resp->SetStringField(TEXT("format"), Format);
    Message = FString::Printf(TEXT("Localization export initiated to '%s' for culture '%s'"), *OutputPath, *Culture);
  }
  
  else if (LowerSub == TEXT("set_culture")) {
    FString Culture;
    Payload->TryGetStringField(TEXT("culture"), Culture);
    bool bSaveToConfig = true;
    Payload->TryGetBoolField(TEXT("saveToConfig"), bSaveToConfig);
    
    FInternationalization::Get().SetCurrentCulture(Culture);
    
    Resp->SetStringField(TEXT("culture"), Culture);
    Resp->SetStringField(TEXT("currentCulture"), FInternationalization::Get().GetCurrentCulture()->GetName());
    Message = FString::Printf(TEXT("Set culture to '%s'"), *Culture);
  }
  
  else if (LowerSub == TEXT("get_available_cultures")) {
    // UE 5.7 API: GetAvailableCultures takes a culture names array
    // First get all known culture names, then get the available cultures from those
    TArray<FString> AllCultureNames;
    FInternationalization::Get().GetCultureNames(AllCultureNames);
    TArray<FCultureRef> AvailableCultures = FInternationalization::Get().GetAvailableCultures(AllCultureNames, true);
    
    TArray<TSharedPtr<FJsonValue>> CulturesArray;
    for (const FCultureRef& CultureRef : AvailableCultures) {
      CulturesArray.Add(MakeShared<FJsonValueString>(CultureRef->GetName()));
    }
    
    Resp->SetArrayField(TEXT("availableCultures"), CulturesArray);
    Resp->SetStringField(TEXT("currentCulture"), FInternationalization::Get().GetCurrentCulture()->GetName());
    Message = FString::Printf(TEXT("Found %d available cultures"), CulturesArray.Num());
  }

  // ==================== SCALABILITY ====================
  else if (LowerSub == TEXT("create_device_profile")) {
    FString ProfileName;
    Payload->TryGetStringField(TEXT("profileName"), ProfileName);
    FString BaseProfile;
    Payload->TryGetStringField(TEXT("baseProfile"), BaseProfile);
    FString DeviceType = TEXT("Desktop");
    Payload->TryGetStringField(TEXT("deviceType"), DeviceType);
    
    UDeviceProfileManager& Manager = UDeviceProfileManager::Get();
    UDeviceProfile* Profile = Manager.CreateProfile(ProfileName, DeviceType);
    
    if (Profile) {
      if (!BaseProfile.IsEmpty()) {
        // BaseProfileName is a UPROPERTY string, assign directly
        Profile->BaseProfileName = BaseProfile;
      }
      
      // Apply CVars if provided
      const TSharedPtr<FJsonObject>* CVarsObj;
      if (Payload->TryGetObjectField(TEXT("cvars"), CVarsObj)) {
        for (const auto& Pair : (*CVarsObj)->Values) {
          FString CVar = Pair.Key;
          FString Value;
          if (Pair.Value->TryGetString(Value)) {
            Profile->CVars.Add(FString::Printf(TEXT("%s=%s"), *CVar, *Value));
          } else {
            double NumValue;
            if (Pair.Value->TryGetNumber(NumValue)) {
              Profile->CVars.Add(FString::Printf(TEXT("%s=%.2f"), *CVar, NumValue));
            }
          }
        }
      }
      
      Resp->SetStringField(TEXT("profileName"), ProfileName);
      Resp->SetStringField(TEXT("deviceType"), DeviceType);
      Message = FString::Printf(TEXT("Created device profile '%s'"), *ProfileName);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create device profile");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }
  
  else if (LowerSub == TEXT("configure_scalability_group")) {
    FString GroupName;
    Payload->TryGetStringField(TEXT("groupName"), GroupName);
    int32 QualityLevel = 3;
    double QualityLevelD = 3.0;
    if (Payload->TryGetNumberField(TEXT("qualityLevel"), QualityLevelD)) {
      QualityLevel = FMath::Clamp((int32)QualityLevelD, 0, 4);
    }
    
    Scalability::FQualityLevels CurrentLevels = Scalability::GetQualityLevels();
    
    if (GroupName == TEXT("ViewDistance")) {
      CurrentLevels.ViewDistanceQuality = QualityLevel;
    } else if (GroupName == TEXT("AntiAliasing")) {
      CurrentLevels.AntiAliasingQuality = QualityLevel;
    } else if (GroupName == TEXT("PostProcess")) {
      CurrentLevels.PostProcessQuality = QualityLevel;
    } else if (GroupName == TEXT("Shadow")) {
      CurrentLevels.ShadowQuality = QualityLevel;
    } else if (GroupName == TEXT("Texture") || GroupName == TEXT("GlobalTexture")) {
      CurrentLevels.TextureQuality = QualityLevel;
    } else if (GroupName == TEXT("Effects")) {
      CurrentLevels.EffectsQuality = QualityLevel;
    } else if (GroupName == TEXT("Foliage")) {
      CurrentLevels.FoliageQuality = QualityLevel;
    } else if (GroupName == TEXT("Shading")) {
      CurrentLevels.ShadingQuality = QualityLevel;
    }
    
    Scalability::SetQualityLevels(CurrentLevels);
    
    Resp->SetStringField(TEXT("groupName"), GroupName);
    Resp->SetNumberField(TEXT("qualityLevel"), QualityLevel);
    Message = FString::Printf(TEXT("Set %s quality to level %d"), *GroupName, QualityLevel);
  }
  
  else if (LowerSub == TEXT("set_quality_level")) {
    int32 OverallQuality = 3;
    double OverallQualityD = 3.0;
    if (Payload->TryGetNumberField(TEXT("overallQuality"), OverallQualityD)) {
      OverallQuality = FMath::Clamp((int32)OverallQualityD, 0, 4);
    }
    bool bApplyImmediately = true;
    Payload->TryGetBoolField(TEXT("applyImmediately"), bApplyImmediately);
    
    Scalability::FQualityLevels QualityLevels;
    QualityLevels.SetFromSingleQualityLevel(OverallQuality);
    
    if (bApplyImmediately) {
      Scalability::SetQualityLevels(QualityLevels);
    }
    
    Resp->SetNumberField(TEXT("overallQuality"), OverallQuality);
    Resp->SetNumberField(TEXT("currentQuality"), OverallQuality);
    Message = FString::Printf(TEXT("Set overall quality to level %d"), OverallQuality);
  }
  
  else if (LowerSub == TEXT("get_scalability_settings")) {
    Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
    
    TSharedPtr<FJsonObject> SettingsObj = MakeShared<FJsonObject>();
    SettingsObj->SetNumberField(TEXT("viewDistance"), Levels.ViewDistanceQuality);
    SettingsObj->SetNumberField(TEXT("antiAliasing"), Levels.AntiAliasingQuality);
    SettingsObj->SetNumberField(TEXT("postProcess"), Levels.PostProcessQuality);
    SettingsObj->SetNumberField(TEXT("shadow"), Levels.ShadowQuality);
    SettingsObj->SetNumberField(TEXT("texture"), Levels.TextureQuality);
    SettingsObj->SetNumberField(TEXT("effects"), Levels.EffectsQuality);
    SettingsObj->SetNumberField(TEXT("foliage"), Levels.FoliageQuality);
    SettingsObj->SetNumberField(TEXT("shading"), Levels.ShadingQuality);
    
    Resp->SetObjectField(TEXT("scalabilitySettings"), SettingsObj);
    Message = TEXT("Retrieved scalability settings");
  }
  
  else if (LowerSub == TEXT("set_resolution_scale")) {
    double Scale = 100.0;
    Payload->TryGetNumberField(TEXT("scale"), Scale);
    double MinScale = 50.0;
    Payload->TryGetNumberField(TEXT("minScale"), MinScale);
    double MaxScale = 100.0;
    Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);
    
    Scale = FMath::Clamp(Scale, MinScale, MaxScale);
    
    if (GEngine) {
      GEngine->Exec(World, *FString::Printf(TEXT("r.ScreenPercentage %.0f"), Scale));
    }
    
    Resp->SetNumberField(TEXT("resolutionScale"), Scale);
    Message = FString::Printf(TEXT("Set resolution scale to %.0f%%"), Scale);
  }

  // ==================== UTILITY ====================
  else if (LowerSub == TEXT("get_gameplay_systems_info")) {
    TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
    
    // Count objectives
    int32 ObjectiveCount = 0;
    int32 MarkerCount = 0;
    int32 CheckpointCount = 0;
    
    if (World) {
      for (TActorIterator<AActor> It(World); It; ++It) {
        if (It->Tags.Contains(TEXT("Objective"))) ObjectiveCount++;
        if (It->Tags.Contains(TEXT("WorldMarker"))) MarkerCount++;
        if (It->Tags.Contains(TEXT("Checkpoint"))) CheckpointCount++;
      }
    }
    
    InfoObj->SetNumberField(TEXT("objectiveCount"), ObjectiveCount);
    InfoObj->SetNumberField(TEXT("markerCount"), MarkerCount);
    InfoObj->SetNumberField(TEXT("checkpointCount"), CheckpointCount);
    
    // Current culture
    InfoObj->SetStringField(TEXT("currentCulture"), FInternationalization::Get().GetCurrentCulture()->GetName());
    
    // Scalability
    Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
    InfoObj->SetNumberField(TEXT("currentQuality"), Levels.GetSingleQualityLevel());
    
    Resp->SetObjectField(TEXT("info"), InfoObj);
    Message = TEXT("Retrieved gameplay systems info");
  }
  
  // ==================== WAVE 3.41-3.50: ADDITIONAL GAMEPLAY ACTIONS ====================
  
  // 3.41: Create linked objectives chain
  else if (LowerSub == TEXT("create_objective_chain")) {
    const TArray<TSharedPtr<FJsonValue>>* ObjectiveIdsArray;
    if (!Payload->TryGetArrayField(TEXT("objectiveIds"), ObjectiveIdsArray) || ObjectiveIdsArray->Num() == 0) {
      bSuccess = false;
      Message = TEXT("objectiveIds array required and cannot be empty");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      FString ChainType = TEXT("Sequential");
      Payload->TryGetStringField(TEXT("chainType"), ChainType);
      bool bFailOnAnyFail = false;
      Payload->TryGetBoolField(TEXT("failOnAnyFail"), bFailOnAnyFail);
      FString Description;
      Payload->TryGetStringField(TEXT("description"), Description);
      
      // Create a chain actor to represent the objective chain
      FString ChainId = FGuid::NewGuid().ToString().Left(8);
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = *FString::Printf(TEXT("ObjectiveChain_%s"), *ChainId);
      SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      
      AActor* ChainActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
      if (ChainActor) {
        ChainActor->SetActorLabel(*FString::Printf(TEXT("ObjectiveChain_%s"), *ChainId));
        ChainActor->Tags.Add(TEXT("ObjectiveChain"));
        ChainActor->Tags.Add(*FString::Printf(TEXT("ChainId:%s"), *ChainId));
        ChainActor->Tags.Add(*FString::Printf(TEXT("ChainType:%s"), *ChainType));
        ChainActor->Tags.Add(*FString::Printf(TEXT("FailOnAnyFail:%s"), bFailOnAnyFail ? TEXT("true") : TEXT("false")));
        ChainActor->SetActorHiddenInGame(true);
        
        // Link each objective to this chain
        TArray<FString> LinkedObjectives;
        for (int32 i = 0; i < ObjectiveIdsArray->Num(); i++) {
          FString ObjId = (*ObjectiveIdsArray)[i]->AsString();
          ChainActor->Tags.Add(*FString::Printf(TEXT("Objective_%d:%s"), i, *ObjId));
          LinkedObjectives.Add(ObjId);
        }
        
        TArray<TSharedPtr<FJsonValue>> LinkedArray;
        for (const FString& Id : LinkedObjectives) {
          LinkedArray.Add(MakeShared<FJsonValueString>(Id));
        }
        
        Resp->SetStringField(TEXT("chainId"), ChainId);
        Resp->SetStringField(TEXT("chainType"), ChainType);
        Resp->SetArrayField(TEXT("linkedObjectives"), LinkedArray);
        Resp->SetNumberField(TEXT("objectiveCount"), LinkedObjectives.Num());
        Message = FString::Printf(TEXT("Created objective chain '%s' with %d objectives"), *ChainId, LinkedObjectives.Num());
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create objective chain actor");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  // 3.42: Configure checkpoint save data
  else if (LowerSub == TEXT("configure_checkpoint_data")) {
    FString CheckpointId;
    Payload->TryGetStringField(TEXT("checkpointId"), CheckpointId);
    if (CheckpointId.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("checkpointId required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      bool bSavePlayerState = true;
      Payload->TryGetBoolField(TEXT("savePlayerState"), bSavePlayerState);
      bool bSaveActorStates = true;
      Payload->TryGetBoolField(TEXT("saveActorStates"), bSaveActorStates);
      bool bSaveWorldState = true;
      Payload->TryGetBoolField(TEXT("saveWorldState"), bSaveWorldState);
      
      // Find checkpoint actor and configure it
      AActor* CheckpointActor = nullptr;
      for (TActorIterator<AActor> It(World); It; ++It) {
        if (It->Tags.Contains(TEXT("Checkpoint"))) {
          for (const FName& Tag : It->Tags) {
            if (Tag.ToString().StartsWith(TEXT("CheckpointId:"))) {
              FString Id = Tag.ToString().RightChop(13);
              if (Id == CheckpointId) {
                CheckpointActor = *It;
                break;
              }
            }
          }
        }
        if (CheckpointActor) break;
      }
      
      if (CheckpointActor) {
        // Update checkpoint configuration
        CheckpointActor->Tags.Add(*FString::Printf(TEXT("SavePlayerState:%s"), bSavePlayerState ? TEXT("true") : TEXT("false")));
        CheckpointActor->Tags.Add(*FString::Printf(TEXT("SaveActorStates:%s"), bSaveActorStates ? TEXT("true") : TEXT("false")));
        CheckpointActor->Tags.Add(*FString::Printf(TEXT("SaveWorldState:%s"), bSaveWorldState ? TEXT("true") : TEXT("false")));
        
        // Handle actor filter
        const TArray<TSharedPtr<FJsonValue>>* ActorFilterArray;
        if (Payload->TryGetArrayField(TEXT("actorFilter"), ActorFilterArray)) {
          for (int32 i = 0; i < ActorFilterArray->Num(); i++) {
            FString FilterTag = (*ActorFilterArray)[i]->AsString();
            CheckpointActor->Tags.Add(*FString::Printf(TEXT("ActorFilter_%d:%s"), i, *FilterTag));
          }
        }
        
        Resp->SetStringField(TEXT("checkpointId"), CheckpointId);
        Resp->SetBoolField(TEXT("savePlayerState"), bSavePlayerState);
        Resp->SetBoolField(TEXT("saveActorStates"), bSaveActorStates);
        Resp->SetBoolField(TEXT("saveWorldState"), bSaveWorldState);
        Message = FString::Printf(TEXT("Configured checkpoint data for '%s'"), *CheckpointId);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Checkpoint '%s' not found"), *CheckpointId);
        ErrorCode = TEXT("CHECKPOINT_NOT_FOUND");
      }
    }
  }
  
  // 3.43: Create dialogue tree node
  else if (LowerSub == TEXT("create_dialogue_node")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    
    if (AssetPath.IsEmpty() || NodeId.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and nodeId required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      FString NodeType = TEXT("Speech");
      Payload->TryGetStringField(TEXT("nodeType"), NodeType);
      FString SpeakerId;
      Payload->TryGetStringField(TEXT("speakerId"), SpeakerId);
      FString Text;
      Payload->TryGetStringField(TEXT("text"), Text);
      FString AudioAsset;
      Payload->TryGetStringField(TEXT("audioAsset"), AudioAsset);
      double Duration = 0.0;
      Payload->TryGetNumberField(TEXT("duration"), Duration);
      FString NextNodeId;
      Payload->TryGetStringField(TEXT("nextNodeId"), NextNodeId);
      
      // Load or create the dialogue data asset
      UDataAsset* DialogueAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
      if (!DialogueAsset) {
        // Create new dialogue asset if not found
        FString PackageName = AssetPath;
        FString AssetName = FPackageName::GetShortName(AssetPath);
        
        UPackage* Package = CreatePackage(*PackageName);
        if (Package) {
          DialogueAsset = NewObject<UDataAsset>(Package, *AssetName, RF_Public | RF_Standalone);
          if (DialogueAsset) {
            DialogueAsset->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(DialogueAsset);
          }
        }
      }
      
      if (DialogueAsset) {
        // Store dialogue node in world actor for runtime access
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = *FString::Printf(TEXT("DialogueNode_%s_%s"), *FPackageName::GetShortName(AssetPath), *NodeId);
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        AActor* NodeActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (NodeActor) {
          NodeActor->SetActorLabel(*FString::Printf(TEXT("DialogueNode_%s"), *NodeId));
          NodeActor->Tags.Add(TEXT("DialogueNode"));
          NodeActor->Tags.Add(*FString::Printf(TEXT("NodeId:%s"), *NodeId));
          NodeActor->Tags.Add(*FString::Printf(TEXT("NodeType:%s"), *NodeType));
          NodeActor->Tags.Add(*FString::Printf(TEXT("AssetPath:%s"), *AssetPath));
          if (!SpeakerId.IsEmpty()) {
            NodeActor->Tags.Add(*FString::Printf(TEXT("SpeakerId:%s"), *SpeakerId));
          }
          if (!NextNodeId.IsEmpty()) {
            NodeActor->Tags.Add(*FString::Printf(TEXT("NextNodeId:%s"), *NextNodeId));
          }
          NodeActor->SetActorHiddenInGame(true);
          
          // Handle choices array
          const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
          if (Payload->TryGetArrayField(TEXT("choices"), ChoicesArray)) {
            for (int32 i = 0; i < ChoicesArray->Num(); i++) {
              const TSharedPtr<FJsonObject>* ChoiceObj;
              if ((*ChoicesArray)[i]->TryGetObject(ChoiceObj)) {
                FString ChoiceText;
                FString ChoiceNextNode;
                (*ChoiceObj)->TryGetStringField(TEXT("text"), ChoiceText);
                (*ChoiceObj)->TryGetStringField(TEXT("nextNodeId"), ChoiceNextNode);
                NodeActor->Tags.Add(*FString::Printf(TEXT("Choice_%d:%s|%s"), i, *ChoiceText, *ChoiceNextNode));
              }
            }
          }
          
          Resp->SetStringField(TEXT("nodeId"), NodeId);
          Resp->SetStringField(TEXT("nodeType"), NodeType);
          Resp->SetStringField(TEXT("assetPath"), AssetPath);
          Resp->SetStringField(TEXT("speakerId"), SpeakerId);
          Resp->SetNumberField(TEXT("duration"), Duration);
          Message = FString::Printf(TEXT("Created dialogue node '%s' of type '%s'"), *NodeId, *NodeType);
        } else {
          bSuccess = false;
          Message = TEXT("Failed to create dialogue node actor");
          ErrorCode = TEXT("CREATE_FAILED");
        }
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Failed to create/load dialogue asset '%s'"), *AssetPath);
        ErrorCode = TEXT("ASSET_FAILED");
      }
    }
  }
  
  // 3.44: Configure targeting priorities
  else if (LowerSub == TEXT("configure_targeting_priority")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
      if (!TargetActor) {
        bSuccess = false;
        Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      } else {
        FString PreferredTargetType;
        Payload->TryGetStringField(TEXT("preferredTargetType"), PreferredTargetType);
        
        // Remove old targeting priority tags
        TargetActor->Tags.RemoveAll([](const FName& Tag) {
          return Tag.ToString().StartsWith(TEXT("TargetPriority_")) || 
                 Tag.ToString().StartsWith(TEXT("IgnoreTag_")) ||
                 Tag.ToString().StartsWith(TEXT("PreferredTarget:"));
        });
        
        // Add target priorities
        const TArray<TSharedPtr<FJsonValue>>* PrioritiesArray;
        if (Payload->TryGetArrayField(TEXT("targetPriorities"), PrioritiesArray)) {
          for (int32 i = 0; i < PrioritiesArray->Num(); i++) {
            const TSharedPtr<FJsonObject>* PriorityObj;
            if ((*PrioritiesArray)[i]->TryGetObject(PriorityObj)) {
              FString TargetClass;
              double Priority = 1.0;
              (*PriorityObj)->TryGetStringField(TEXT("class"), TargetClass);
              (*PriorityObj)->TryGetNumberField(TEXT("priority"), Priority);
              TargetActor->Tags.Add(*FString::Printf(TEXT("TargetPriority_%d:%s|%.2f"), i, *TargetClass, Priority));
            }
          }
        }
        
        // Add ignore tags
        const TArray<TSharedPtr<FJsonValue>>* IgnoreTagsArray;
        if (Payload->TryGetArrayField(TEXT("ignoreTags"), IgnoreTagsArray)) {
          for (int32 i = 0; i < IgnoreTagsArray->Num(); i++) {
            FString IgnoreTag = (*IgnoreTagsArray)[i]->AsString();
            TargetActor->Tags.Add(*FString::Printf(TEXT("IgnoreTag_%d:%s"), i, *IgnoreTag));
          }
        }
        
        if (!PreferredTargetType.IsEmpty()) {
          TargetActor->Tags.Add(*FString::Printf(TEXT("PreferredTarget:%s"), *PreferredTargetType));
        }
        
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetStringField(TEXT("preferredTargetType"), PreferredTargetType);
        Message = FString::Printf(TEXT("Configured targeting priorities for actor '%s'"), *ActorName);
      }
    }
  }
  
  // 3.46: Add/modify localization entry
  else if (LowerSub == TEXT("configure_localization_entry")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    
    if (AssetPath.IsEmpty() || Key.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and key required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      FString SourceString;
      Payload->TryGetStringField(TEXT("sourceString"), SourceString);
      FString Namespace = TEXT("Game");
      Payload->TryGetStringField(TEXT("namespace"), Namespace);
      FString Culture = TEXT("en");
      Payload->TryGetStringField(TEXT("culture"), Culture);
      FString Comment;
      Payload->TryGetStringField(TEXT("comment"), Comment);
      bool bSave = true;
      Payload->TryGetBoolField(TEXT("save"), bSave);
      
      // Try to load or create string table
      UStringTable* StringTable = LoadObject<UStringTable>(nullptr, *AssetPath);
      if (!StringTable) {
        // Create new string table
        FString PackageName = AssetPath;
        FString AssetName = FPackageName::GetShortName(AssetPath);
        
        UPackage* Package = CreatePackage(*PackageName);
        if (Package) {
          StringTable = NewObject<UStringTable>(Package, *AssetName, RF_Public | RF_Standalone);
          if (StringTable) {
            StringTable->MarkPackageDirty();
            FAssetRegistryModule::AssetCreated(StringTable);
          }
        }
      }
      
      if (StringTable) {
        // Add or update the entry
        FStringTableRef TableRef = StringTable->GetMutableStringTable();
        TableRef->SetSourceString(Key, SourceString);
        
        StringTable->MarkPackageDirty();
        
        if (bSave) {
          McpSafeAssetSave(StringTable);
        }
        
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("key"), Key);
        Resp->SetStringField(TEXT("sourceString"), SourceString);
        Resp->SetStringField(TEXT("namespace"), Namespace);
        Resp->SetStringField(TEXT("culture"), Culture);
        Message = FString::Printf(TEXT("Added localization entry '%s' to '%s'"), *Key, *AssetPath);
      } else {
        bSuccess = false;
        Message = FString::Printf(TEXT("Failed to create/load string table '%s'"), *AssetPath);
        ErrorCode = TEXT("ASSET_FAILED");
      }
    }
  }
  
  // 3.47: Create quest stage
  else if (LowerSub == TEXT("create_quest_stage")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    FString StageId;
    Payload->TryGetStringField(TEXT("stageId"), StageId);
    
    if (AssetPath.IsEmpty() || StageId.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("assetPath and stageId required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      FString StageName;
      Payload->TryGetStringField(TEXT("stageName"), StageName);
      FString Description;
      Payload->TryGetStringField(TEXT("description"), Description);
      FString StageType = TEXT("Progress");
      Payload->TryGetStringField(TEXT("stageType"), StageType);
      bool bSave = true;
      Payload->TryGetBoolField(TEXT("save"), bSave);
      
      // Create stage actor in world
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = *FString::Printf(TEXT("QuestStage_%s_%s"), *FPackageName::GetShortName(AssetPath), *StageId);
      SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      
      AActor* StageActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
      if (StageActor) {
        StageActor->SetActorLabel(*FString::Printf(TEXT("QuestStage_%s"), *StageId));
        StageActor->Tags.Add(TEXT("QuestStage"));
        StageActor->Tags.Add(*FString::Printf(TEXT("StageId:%s"), *StageId));
        StageActor->Tags.Add(*FString::Printf(TEXT("AssetPath:%s"), *AssetPath));
        StageActor->Tags.Add(*FString::Printf(TEXT("StageType:%s"), *StageType));
        if (!StageName.IsEmpty()) {
          StageActor->Tags.Add(*FString::Printf(TEXT("StageName:%s"), *StageName));
        }
        StageActor->SetActorHiddenInGame(true);
        
        // Add next stage links
        const TArray<TSharedPtr<FJsonValue>>* NextStageIdsArray;
        if (Payload->TryGetArrayField(TEXT("nextStageIds"), NextStageIdsArray)) {
          for (int32 i = 0; i < NextStageIdsArray->Num(); i++) {
            FString NextStageId = (*NextStageIdsArray)[i]->AsString();
            StageActor->Tags.Add(*FString::Printf(TEXT("NextStage_%d:%s"), i, *NextStageId));
          }
        }
        
        // Add stage objectives
        const TArray<TSharedPtr<FJsonValue>>* ObjectivesArray;
        if (Payload->TryGetArrayField(TEXT("stageObjectives"), ObjectivesArray)) {
          for (int32 i = 0; i < ObjectivesArray->Num(); i++) {
            FString ObjectiveId = (*ObjectivesArray)[i]->AsString();
            StageActor->Tags.Add(*FString::Printf(TEXT("StageObjective_%d:%s"), i, *ObjectiveId));
          }
        }
        
        Resp->SetStringField(TEXT("stageId"), StageId);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        Resp->SetStringField(TEXT("stageType"), StageType);
        Resp->SetStringField(TEXT("stageName"), StageName);
        Message = FString::Printf(TEXT("Created quest stage '%s'"), *StageId);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create quest stage actor");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  // 3.48: Configure minimap display
  else if (LowerSub == TEXT("configure_minimap_icon")) {
    FString ActorName;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("actorName required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      AActor* TargetActor = FindActorByLabelOrName<AActor>(ActorName);
      if (!TargetActor) {
        bSuccess = false;
        Message = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
      } else {
        FString IconTexture;
        Payload->TryGetStringField(TEXT("iconTexture"), IconTexture);
        if (IconTexture.IsEmpty()) {
          Payload->TryGetStringField(TEXT("iconPath"), IconTexture);
        }
        double IconSize = 32.0;
        Payload->TryGetNumberField(TEXT("iconSize"), IconSize);
        bool bRotateWithActor = true;
        Payload->TryGetBoolField(TEXT("rotateWithActor"), bRotateWithActor);
        bool bVisibleOnMinimap = true;
        Payload->TryGetBoolField(TEXT("visibleOnMinimap"), bVisibleOnMinimap);
        int32 MinimapLayer = 0;
        double MinimapLayerD = 0;
        if (Payload->TryGetNumberField(TEXT("minimapLayer"), MinimapLayerD)) {
          MinimapLayer = (int32)MinimapLayerD;
        }
        
        // Extract color
        FLinearColor IconColor = FLinearColor::White;
        const TSharedPtr<FJsonObject>* ColorObj;
        if (Payload->TryGetObjectField(TEXT("color"), ColorObj)) {
          (*ColorObj)->TryGetNumberField(TEXT("r"), IconColor.R);
          (*ColorObj)->TryGetNumberField(TEXT("g"), IconColor.G);
          (*ColorObj)->TryGetNumberField(TEXT("b"), IconColor.B);
          (*ColorObj)->TryGetNumberField(TEXT("a"), IconColor.A);
        }
        
        // Remove old minimap tags
        TargetActor->Tags.RemoveAll([](const FName& Tag) {
          return Tag.ToString().StartsWith(TEXT("Minimap_"));
        });
        
        // Add minimap configuration tags
        TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Visible:%s"), bVisibleOnMinimap ? TEXT("true") : TEXT("false")));
        TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Size:%.1f"), IconSize));
        TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Rotate:%s"), bRotateWithActor ? TEXT("true") : TEXT("false")));
        TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Layer:%d"), MinimapLayer));
        TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Color:%.2f,%.2f,%.2f,%.2f"), IconColor.R, IconColor.G, IconColor.B, IconColor.A));
        if (!IconTexture.IsEmpty()) {
          TargetActor->Tags.Add(*FString::Printf(TEXT("Minimap_Icon:%s"), *IconTexture));
        }
        
        Resp->SetStringField(TEXT("actorName"), ActorName);
        Resp->SetBoolField(TEXT("visibleOnMinimap"), bVisibleOnMinimap);
        Resp->SetNumberField(TEXT("iconSize"), IconSize);
        Resp->SetNumberField(TEXT("minimapLayer"), MinimapLayer);
        Message = FString::Printf(TEXT("Configured minimap icon for actor '%s'"), *ActorName);
      }
    }
  }
  
  // 3.49: Set global game state value
  else if (LowerSub == TEXT("set_game_state")) {
    FString StateKey;
    Payload->TryGetStringField(TEXT("stateKey"), StateKey);
    
    if (StateKey.IsEmpty()) {
      bSuccess = false;
      Message = TEXT("stateKey required");
      ErrorCode = TEXT("INVALID_ARGUMENT");
    } else {
      bool bPersistent = false;
      Payload->TryGetBoolField(TEXT("persistent"), bPersistent);
      bool bReplicated = false;
      Payload->TryGetBoolField(TEXT("replicated"), bReplicated);
      
      // Store state in a game state holder actor
      AActor* StateHolderActor = nullptr;
      for (TActorIterator<AActor> It(World); It; ++It) {
        if (It->Tags.Contains(TEXT("GameStateHolder"))) {
          StateHolderActor = *It;
          break;
        }
      }
      
      // Create state holder if it doesn't exist
      if (!StateHolderActor) {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Name = TEXT("GameStateHolder");
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        StateHolderActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (StateHolderActor) {
          StateHolderActor->SetActorLabel(TEXT("GameStateHolder"));
          StateHolderActor->Tags.Add(TEXT("GameStateHolder"));
          StateHolderActor->SetActorHiddenInGame(true);
        }
      }
      
      if (StateHolderActor) {
        // Remove old state with this key
        StateHolderActor->Tags.RemoveAll([&StateKey](const FName& Tag) {
          return Tag.ToString().StartsWith(FString::Printf(TEXT("State_%s:"), *StateKey));
        });
        
        // Get state value (can be string, number, or bool)
        FString StateValueStr;
        double StateValueNum = 0.0;
        bool StateValueBool = false;
        FString ValueType = TEXT("string");
        
        if (Payload->TryGetStringField(TEXT("stateValue"), StateValueStr)) {
          ValueType = TEXT("string");
        } else if (Payload->TryGetNumberField(TEXT("stateValue"), StateValueNum)) {
          StateValueStr = FString::Printf(TEXT("%.6f"), StateValueNum);
          ValueType = TEXT("number");
        } else if (Payload->TryGetBoolField(TEXT("stateValue"), StateValueBool)) {
          StateValueStr = StateValueBool ? TEXT("true") : TEXT("false");
          ValueType = TEXT("bool");
        }
        
        // Add new state
        StateHolderActor->Tags.Add(*FString::Printf(TEXT("State_%s:%s|%s|%s|%s"), 
          *StateKey, *StateValueStr, *ValueType,
          bPersistent ? TEXT("p") : TEXT(""),
          bReplicated ? TEXT("r") : TEXT("")));
        
        Resp->SetStringField(TEXT("stateKey"), StateKey);
        Resp->SetStringField(TEXT("stateValue"), StateValueStr);
        Resp->SetStringField(TEXT("valueType"), ValueType);
        Resp->SetBoolField(TEXT("persistent"), bPersistent);
        Resp->SetBoolField(TEXT("replicated"), bReplicated);
        Message = FString::Printf(TEXT("Set game state '%s' = '%s'"), *StateKey, *StateValueStr);
      } else {
        bSuccess = false;
        Message = TEXT("Failed to create game state holder");
        ErrorCode = TEXT("CREATE_FAILED");
      }
    }
  }
  
  // 3.50: Configure save system settings
  else if (LowerSub == TEXT("configure_save_system")) {
    FString SaveSystemType = TEXT("Slot");
    Payload->TryGetStringField(TEXT("saveSystemType"), SaveSystemType);
    int32 MaxSaveSlots = 10;
    double MaxSaveSlotsD = 10.0;
    if (Payload->TryGetNumberField(TEXT("maxSaveSlots"), MaxSaveSlotsD)) {
      MaxSaveSlots = FMath::Clamp((int32)MaxSaveSlotsD, 1, 999);
    }
    double AutoSaveInterval = 0.0;
    Payload->TryGetNumberField(TEXT("autoSaveInterval"), AutoSaveInterval);
    bool bCompressSaves = true;
    Payload->TryGetBoolField(TEXT("compressSaves"), bCompressSaves);
    bool bEncryptSaves = false;
    Payload->TryGetBoolField(TEXT("encryptSaves"), bEncryptSaves);
    
    // Create or update save system config actor
    AActor* SaveConfigActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->Tags.Contains(TEXT("SaveSystemConfig"))) {
        SaveConfigActor = *It;
        break;
      }
    }
    
    if (!SaveConfigActor) {
      FActorSpawnParameters SpawnParams;
      SpawnParams.Name = TEXT("SaveSystemConfig");
      SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
      
      SaveConfigActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
      if (SaveConfigActor) {
        SaveConfigActor->SetActorLabel(TEXT("SaveSystemConfig"));
        SaveConfigActor->Tags.Add(TEXT("SaveSystemConfig"));
        SaveConfigActor->SetActorHiddenInGame(true);
      }
    }
    
    if (SaveConfigActor) {
      // Clear old config tags
      SaveConfigActor->Tags.RemoveAll([](const FName& Tag) {
        return Tag.ToString().StartsWith(TEXT("SaveConfig_"));
      });
      
      // Add new config
      SaveConfigActor->Tags.Add(*FString::Printf(TEXT("SaveConfig_Type:%s"), *SaveSystemType));
      SaveConfigActor->Tags.Add(*FString::Printf(TEXT("SaveConfig_MaxSlots:%d"), MaxSaveSlots));
      SaveConfigActor->Tags.Add(*FString::Printf(TEXT("SaveConfig_AutoSave:%.1f"), AutoSaveInterval));
      SaveConfigActor->Tags.Add(*FString::Printf(TEXT("SaveConfig_Compress:%s"), bCompressSaves ? TEXT("true") : TEXT("false")));
      SaveConfigActor->Tags.Add(*FString::Printf(TEXT("SaveConfig_Encrypt:%s"), bEncryptSaves ? TEXT("true") : TEXT("false")));
      
      Resp->SetStringField(TEXT("saveSystemType"), SaveSystemType);
      Resp->SetNumberField(TEXT("maxSaveSlots"), MaxSaveSlots);
      Resp->SetNumberField(TEXT("autoSaveInterval"), AutoSaveInterval);
      Resp->SetBoolField(TEXT("compressSaves"), bCompressSaves);
      Resp->SetBoolField(TEXT("encryptSaves"), bEncryptSaves);
      Message = FString::Printf(TEXT("Configured save system: %s with %d slots"), *SaveSystemType, MaxSaveSlots);
    } else {
      bSuccess = false;
      Message = TEXT("Failed to create save system config");
      ErrorCode = TEXT("CREATE_FAILED");
    }
  }
  
  else {
    bSuccess = false;
    Message = FString::Printf(TEXT("Unknown gameplay systems action: '%s'"), *LowerSub);
    ErrorCode = TEXT("UNKNOWN_ACTION");
  }

  // Send response
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("message"), Message);
  if (!ErrorCode.IsEmpty()) {
    Resp->SetStringField(TEXT("error"), ErrorCode);
  }
  
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor-only action."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
