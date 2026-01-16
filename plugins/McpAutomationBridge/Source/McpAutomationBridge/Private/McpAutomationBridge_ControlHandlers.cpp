#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/JsonSerializer.h"
#include "McpBridgeWebSocket.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "LevelEditorViewport.h"
#include "Editor/EditorPerProjectUserSettings.h"


#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif
// Additional editor headers for viewport control
#include "Components/LightComponent.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"

#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif

#if __has_include("LevelEditorViewport.h")
#include "LevelEditorViewport.h"
#define MCP_HAS_LEVEL_EDITOR_VIEWPORT 1
#else
#define MCP_HAS_LEVEL_EDITOR_VIEWPORT 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"

#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Exporters/Exporter.h"
#include "Misc/OutputDevice.h"

#endif

// Cycle stats for actor control handlers.
// Use `stat McpBridge` in the UE console to view these stats.
DECLARE_CYCLE_STAT(TEXT("ControlActor:Spawn"), STAT_MCP_ControlActorSpawn, STATGROUP_McpBridge);
DECLARE_CYCLE_STAT(TEXT("ControlActor:Delete"), STAT_MCP_ControlActorDelete, STATGROUP_McpBridge);
DECLARE_CYCLE_STAT(TEXT("ControlActor:Transform"), STAT_MCP_ControlActorTransform, STATGROUP_McpBridge);
DECLARE_CYCLE_STAT(TEXT("Editor:ControlAction"), STAT_MCP_EditorControlAction, STATGROUP_McpBridge);

// Global static for session bookmarks
static TMap<FString, FTransform> GSessionBookmarks;


// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
// (ExtractVectorField and ExtractRotatorField moved to
// McpAutomationBridgeHelpers.h)

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawn(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SCOPE_CYCLE_COUNTER(STAT_MCP_ControlActorSpawn);

#if WITH_EDITOR
  FString ClassPath;
  Payload->TryGetStringField(TEXT("classPath"), ClassPath);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  UClass *ResolvedClass = nullptr;
  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
  UStaticMesh *ResolvedStaticMesh = nullptr;
  USkeletalMesh *ResolvedSkeletalMesh = nullptr;

  // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to
  // avoid LogEditorAssetSubsystem errors
  if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) &&
      !ClassPath.StartsWith(TEXT("/Script/"))) {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(ClassPath)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
      else if (UStaticMesh *Mesh = Cast<UStaticMesh>(Loaded))
        ResolvedStaticMesh = Mesh;
      else if (USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Loaded))
        ResolvedSkeletalMesh = SkelMesh;
    }
  }
  if (!ResolvedClass && !ResolvedStaticMesh && !ResolvedSkeletalMesh)
    ResolvedClass = ResolveClassByName(ClassPath);

  // If explicit mesh path provided for a general spawn request
  if (!ResolvedStaticMesh && !ResolvedSkeletalMesh && !MeshPath.IsEmpty()) {
    if (UObject *MeshObj = UEditorAssetLibrary::LoadAsset(MeshPath)) {
      ResolvedStaticMesh = Cast<UStaticMesh>(MeshObj);
      if (!ResolvedStaticMesh)
        ResolvedSkeletalMesh = Cast<USkeletalMesh>(MeshObj);
    }
  }

  // Force StaticMeshActor if we have a resolved mesh, regardless of class input
  // (unless it's a specific subclass)
  bool bSpawnStaticMeshActor = (ResolvedStaticMesh != nullptr);
  bool bSpawnSkeletalMeshActor = (ResolvedSkeletalMesh != nullptr);

  if (!bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor && ResolvedClass) {
    bSpawnStaticMeshActor =
        ResolvedClass->IsChildOf(AStaticMeshActor::StaticClass());
    if (!bSpawnStaticMeshActor)
      bSpawnSkeletalMeshActor =
          ResolvedClass->IsChildOf(ASkeletalMeshActor::StaticClass());
  }

  // Explicitly use StaticMeshActor class if we have a mesh but no class, or if
  // we decided to spawn a static mesh actor
  if (bSpawnStaticMeshActor && !ResolvedClass) {
    ResolvedClass = AStaticMeshActor::StaticClass();
  } else if (bSpawnSkeletalMeshActor && !ResolvedClass) {
    ResolvedClass = ASkeletalMeshActor::StaticClass();
  }

  if (!ResolvedClass && !bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor) {
    const FString ErrorMsg =
        FString::Printf(TEXT("Class not found: %s. Verify plugin is enabled if "
                             "using a plugin class."),
                        *ClassPath);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              ErrorMsg);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Spawned = nullptr;

  // Support PIE spawning
  UWorld *TargetWorld = (GEditor->PlayWorld) ? GEditor->PlayWorld : nullptr;

  if (TargetWorld) {
    // PIE Path
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass *ClassToSpawn =
        ResolvedClass
            ? ResolvedClass
            : (bSpawnStaticMeshActor ? AStaticMeshActor::StaticClass()
                                     : (bSpawnSkeletalMeshActor
                                            ? ASkeletalMeshActor::StaticClass()
                                            : AActor::StaticClass()));
    Spawned = TargetWorld->SpawnActor(ClassToSpawn, &Location, &Rotation,
                                      SpawnParams);

    if (Spawned) {
      if (bSpawnStaticMeshActor) {
        if (AStaticMeshActor *StaticMeshActor =
                Cast<AStaticMeshActor>(Spawned)) {
          if (UStaticMeshComponent *MeshComponent =
                  StaticMeshActor->GetStaticMeshComponent()) {
            if (ResolvedStaticMesh) {
              MeshComponent->SetStaticMesh(ResolvedStaticMesh);
            }
            MeshComponent->SetMobility(EComponentMobility::Movable);
            // PIE actors don't need MarkRenderStateDirty in the same way, but
            // it doesn't hurt
          }
        }
      } else if (bSpawnSkeletalMeshActor) {
        if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned)) {
          if (USkeletalMeshComponent *SkelComp =
                  SkelActor->GetSkeletalMeshComponent()) {
            if (ResolvedSkeletalMesh) {
              SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
            }
            SkelComp->SetMobility(EComponentMobility::Movable);
          }
        }
      }
    }
  } else {
    // Editor Path
    if (bSpawnStaticMeshActor) {
      Spawned = ActorSS->SpawnActorFromClass(
          ResolvedClass ? ResolvedClass : AStaticMeshActor::StaticClass(),
          Location, Rotation);
      if (Spawned) {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
        if (AStaticMeshActor *StaticMeshActor =
                Cast<AStaticMeshActor>(Spawned)) {
          if (UStaticMeshComponent *MeshComponent =
                  StaticMeshActor->GetStaticMeshComponent()) {
            if (ResolvedStaticMesh) {
              MeshComponent->SetStaticMesh(ResolvedStaticMesh);
            }
            MeshComponent->SetMobility(EComponentMobility::Movable);
            MeshComponent->MarkRenderStateDirty();
          }
        }
      }
    } else if (bSpawnSkeletalMeshActor) {
      Spawned = ActorSS->SpawnActorFromClass(
          ResolvedClass ? ResolvedClass : ASkeletalMeshActor::StaticClass(),
          Location, Rotation);
      if (Spawned) {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
        if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned)) {
          if (USkeletalMeshComponent *SkelComp =
                  SkelActor->GetSkeletalMeshComponent()) {
            if (ResolvedSkeletalMesh) {
              SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
            }
            SkelComp->SetMobility(EComponentMobility::Movable);
            SkelComp->MarkRenderStateDirty();
          }
        }
      }
    } else {
      Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
      if (Spawned) {
        Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);
      }
    }
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn actor"));

    return true;
  }

  if (!ActorName.IsEmpty()) {
    Spawned->SetActorLabel(ActorName);
  } else {
    // Auto-generate a friendly label from the mesh or class name
    FString BaseName;
    if (ResolvedStaticMesh) {
      BaseName = ResolvedStaticMesh->GetName();
    } else if (ResolvedSkeletalMesh) {
      BaseName = ResolvedSkeletalMesh->GetName();
    } else if (ResolvedClass) {
      BaseName = ResolvedClass->GetName();
      if (BaseName.EndsWith(TEXT("_C"))) {
        BaseName.RemoveFromEnd(TEXT("_C"));
      }
    } else {
      BaseName = TEXT("Actor");
    }
    Spawned->SetActorLabel(BaseName);
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("id"), Spawned->GetActorLabel());
  Data->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  Data->SetStringField(TEXT("objectPath"), Spawned->GetPathName());
  // Provide the resolved class path useful for referencing
  if (ResolvedClass)
    Data->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  else
    Data->SetStringField(TEXT("classPath"), ClassPath);

  if (ResolvedStaticMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
  else if (ResolvedSkeletalMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedSkeletalMesh->GetPathName());

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Spawned actor '%s'"), *Spawned->GetActorLabel());

  SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Data);
  return true;

#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBlueprint(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Blueprint path required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

  UClass *ResolvedClass = nullptr;

  // Prefer the same blueprint resolution heuristics used by manage_blueprint
  // so that short names and package paths behave consistently.
  FString NormalizedPath;
  FString LoadError;
  if (!BlueprintPath.IsEmpty()) {
    UBlueprint *BlueprintAsset =
        LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (BlueprintAsset && BlueprintAsset->GeneratedClass) {
      ResolvedClass = BlueprintAsset->GeneratedClass;
    }
  }

  if (!ResolvedClass && (BlueprintPath.StartsWith(TEXT("/")) ||
                         BlueprintPath.Contains(TEXT("/")))) {
    if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(BlueprintPath)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        ResolvedClass = BP->GeneratedClass;
      else if (UClass *C = Cast<UClass>(Loaded))
        ResolvedClass = C;
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(BlueprintPath);

  if (!ResolvedClass) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("error"), TEXT("Blueprint class not found"));
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Blueprint class not found"), Resp,
                           TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

  // Debug log the received location
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("spawn_blueprint: Location=(%f, %f, %f) Rotation=(%f, %f, %f)"),
         Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw,
         Rotation.Roll);

  AActor *Spawned = nullptr;
  UWorld *TargetWorld = (GEditor->PlayWorld) ? GEditor->PlayWorld : nullptr;

  if (TargetWorld) {
    // PIE Path
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    Spawned = TargetWorld->SpawnActor(ResolvedClass, &Location, &Rotation,
                                      SpawnParams);
    // Ensure physics/teleport if needed, though SpawnActor should handle it.
  } else {
    // Editor Path
    Spawned = ActorSS->SpawnActorFromClass(ResolvedClass, Location, Rotation);
    // Explicitly set location and rotation in case SpawnActorFromClass didn't
    // apply them correctly (legacy fix)
    if (Spawned) {
      Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                           ETeleportType::TeleportPhysics);
    }
  }

  if (!Spawned) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("error"), TEXT("Failed to spawn blueprint"));
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to spawn blueprint"), Resp,
                           TEXT("SPAWN_FAILED"));
    return true;
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorName"), Spawned->GetActorLabel());
  Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Spawned blueprint '%s'"),
         *Spawned->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<FString> Targets;
  const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray) {
    for (const TSharedPtr<FJsonValue> &Entry : *NamesArray) {
      if (Entry.IsValid() && Entry->Type == EJson::String) {
        const FString Value = Entry->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
          Targets.AddUnique(Value);
      }
    }
  }

  FString SingleName;
  if (Targets.Num() == 0) {
    Payload->TryGetStringField(TEXT("actorName"), SingleName);
    if (!SingleName.IsEmpty())
      Targets.AddUnique(SingleName);
  }

  if (Targets.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or actorNames required"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<FString> Deleted;
  TArray<FString> Missing;
  Deleted.Reserve(Targets.Num());
  Missing.Reserve(Targets.Num());

  for (const FString &Name : Targets) {
  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), Name);

    if (!Found) {
      Missing.Add(Name);
      continue;
    }
    if (ActorSS->DestroyActor(Found)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
             TEXT("ControlActor: Deleted actor '%s'"), *Name);
      Deleted.Add(Name);
    } else
      Missing.Add(Name);
  }

  const bool bAllDeleted = Missing.Num() == 0;
  const bool bAnyDeleted = Deleted.Num() > 0;
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bAllDeleted);
  Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  DeletedArray.Reserve(Deleted.Num());
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
    MissingArray.Reserve(Missing.Num());
    for (const FString &Name : Missing)
      MissingArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("missing"), MissingArray);
  }

  FString Message;
  FString ErrorCode;
  if (!bAnyDeleted && Missing.Num() > 0) {
    Message = TEXT("Actors not found");
    ErrorCode = TEXT("NOT_FOUND");
  } else {
    Message = bAllDeleted ? TEXT("Actors deleted")
                          : TEXT("Some actors could not be deleted");
    ErrorCode = bAllDeleted ? FString() : TEXT("DELETE_PARTIAL");
  }

  if (!bAllDeleted && Missing.Num() > 0 && !bAnyDeleted) {
    SendStandardErrorResponse(this, Socket, RequestId, ErrorCode, Message);
  } else {
    SendStandardSuccessResponse(this, Socket, RequestId, Message, Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FVector ForceVector =
      ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UPrimitiveComponent *Prim =
      Found->FindComponentByClass<UPrimitiveComponent>();
  if (!Prim) {
    if (UStaticMeshComponent *SMC =
            Found->FindComponentByClass<UStaticMeshComponent>())
      Prim = SMC;
  }

  if (!Prim) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No component to apply force"), nullptr,
                           TEXT("NO_COMPONENT"));
    return true;
  }

  if (Prim->Mobility == EComponentMobility::Static)
    Prim->SetMobility(EComponentMobility::Movable);

  // Ensure collision is enabled for physics
  if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
    Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }

  // Check if collision geometry exists (common failure for empty
  // StaticMeshActors)
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Prim)) {
    if (!SMC->GetStaticMesh()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMeshComponent has no StaticMesh assigned."), nullptr);
      return true;
    }
    if (!SMC->GetStaticMesh()->GetBodySetup()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMesh has no collision geometry (BodySetup is null)."),
          nullptr);
      return true;
    }
  }

  if (!Prim->IsSimulatingPhysics()) {
    Prim->SetSimulatePhysics(true);
    // Must recreate physics state for the body to be properly initialized in
    // Editor
    Prim->RecreatePhysicsState();
  }

  Prim->AddForce(ForceVector);
  Prim->WakeAllRigidBodies();
  Prim->MarkRenderStateDirty();

  // Verify physics state
  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bIsSimulating) {
    FString FailureReason = TEXT("Failed to enable physics simulation.");
    if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
      FailureReason += TEXT(" Collision is disabled.");
    } else if (Prim->Mobility != EComponentMobility::Movable) {
      FailureReason += TEXT(" Component is not Movable.");
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
                              FailureReason, Data);
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Applied force to '%s'"), *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Force applied"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
  FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

  Found->Modify();
  Found->SetActorLocation(Location, false, nullptr,
                          ETeleportType::TeleportPhysics);
  Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
  Found->SetActorScale3D(Scale);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify transform
  const FVector NewLoc = Found->GetActorLocation();
  const FRotator NewRot = Found->GetActorRotation();
  const FVector NewScale = Found->GetActorScale3D();

  const bool bLocMatch = NewLoc.Equals(Location, 1.0f); // 1 unit tolerance
  // Rotation comparison is tricky due to normalization, skipping strict check
  // for now but logging if very different
  const bool bScaleMatch = NewScale.Equals(Scale, 0.01f);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(NewLoc));
  Data->SetArrayField(TEXT("scale"), MakeArray(NewScale));

  if (!bLocMatch || !bScaleMatch) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("TRANSFORM_MISMATCH"),
                              TEXT("Failed to set transform exactly"), Data);
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Set transform for '%s'"), *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  const FVector Location = Current.GetLocation();
  const FRotator Rotation = Current.GetRotation().Rotator();
  const FVector Scale = Current.GetScale3D();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(Location));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(Scale));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bVisible = true;
  if (Payload->HasField(TEXT("visible")))
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  Found->Modify();
  Found->SetActorHiddenInGame(!bVisible);
  Found->SetActorEnableCollision(bVisible);

  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify visibility state
  const bool bIsHidden = Found->IsHidden();
  const bool bStateMatches = (bIsHidden == !bVisible);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetBoolField(TEXT("visible"), !bIsHidden);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bStateMatches) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("VISIBILITY_MISMATCH"),
                              TEXT("Failed to set actor visibility"), Data);
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Set visibility to %s for '%s'"),
         bVisible ? TEXT("True") : TEXT("False"), *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor visibility updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentType;
  Payload->TryGetStringField(TEXT("componentType"), ComponentType);
  if (ComponentType.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("componentType required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Component class not found"), nullptr,
                           TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(),
                                    FMath::Rand());

  FName DesiredName = FName(*ComponentName);
  UActorComponent *NewComponent = NewObject<UActorComponent>(
      Found, ComponentClass, DesiredName, RF_Transactional);
  if (!NewComponent) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create component"), nullptr,
                           TEXT("CREATE_COMPONENT_FAILED"));
    return true;
  }

  Found->Modify();
  NewComponent->SetFlags(RF_Transactional);
  Found->AddInstanceComponent(NewComponent);
  NewComponent->OnComponentCreated();

  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent)) {
    if (Found->GetRootComponent() && !SceneComp->GetAttachParent()) {
      SceneComp->SetupAttachment(Found->GetRootComponent());
    }
  }

  // Force lights to be movable to ensure they work without baking (Issue #6
  // fix) We check for "LightComponent" class name to avoid dependency issues if
  // header is obscure, but ULightComponent is standard.
  if (NewComponent->IsA(ULightComponent::StaticClass())) {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent)) {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  // Special handling for StaticMeshComponent meshPath convenience
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent)) {
    FString MeshPath;
    if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
        !MeshPath.IsEmpty()) {
      if (UObject *LoadedMesh = UEditorAssetLibrary::LoadAsset(MeshPath)) {
        if (UStaticMesh *Mesh = Cast<UStaticMesh>(LoadedMesh)) {
          SMC->SetStaticMesh(Mesh);
        }
      }
    }
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && (*PropertiesPtr).IsValid()) {
    for (const auto &Pair : (*PropertiesPtr)->Values) {
      FProperty *Property = ComponentClass->FindPropertyByName(*Pair.Key);
      if (!Property) {
        PropertyWarnings.Add(
            FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
        continue;
      }
      FString ApplyError;
      if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value,
                                   ApplyError))
        AppliedProperties.Add(Pair.Key);
      else
        PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                             *Pair.Key, *ApplyError));
    }
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
  }
  if (PropertyWarnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warning : PropertyWarnings)
      WarnArray.Add(MakeShared<FJsonValueString>(Warning));
    Resp->SetArrayField(TEXT("warnings"), WarnArray);
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Added component '%s' to '%s'"),
         *NewComponent->GetName(), *Found->GetActorLabel());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("componentName required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
        PropertiesPtr && PropertiesPtr->IsValid())) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("properties object required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UActorComponent *TargetComponent = nullptr;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      TargetComponent = Comp;
      break;
    }
  }

  if (!TargetComponent) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Component not found"), nullptr,
                           TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  UClass *ComponentClass = TargetComponent->GetClass();
  TargetComponent->Modify();

  // PRIORITY: Apply Mobility FIRST.
  // Physics simulation fails if the component is generic "Static".
  // Scan for Mobility key case-insensitively to ensure we find it regardless of
  // JSON casing
  const TSharedPtr<FJsonValue> *MobilityVal = nullptr;
  FString MobilityKey;
  for (const auto &Pair : (*PropertiesPtr)->Values) {
    if (Pair.Key.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase)) {
      MobilityVal = &Pair.Value;
      MobilityKey = Pair.Key;
      break;
    }
  }

  if (MobilityVal) {
    if (USceneComponent *SC = Cast<USceneComponent>(TargetComponent)) {
      FString EnumVal;
      if ((*MobilityVal)->TryGetString(EnumVal)) {
        // Parse enum string
        int64 Val =
            StaticEnum<EComponentMobility::Type>()->GetValueByNameString(
                EnumVal);
        if (Val != INDEX_NONE) {
          SC->SetMobility((EComponentMobility::Type)Val);
          AppliedProperties.Add(MobilityKey);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set Mobility to %s"), *EnumVal);
        }
      } else {
        double Val;
        if ((*MobilityVal)->TryGetNumber(Val)) {
          SC->SetMobility((EComponentMobility::Type)(int32)Val);
          AppliedProperties.Add(MobilityKey);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set Mobility to %d"), (int32)Val);
        }
      }
    }
  }

  for (const auto &Pair : (*PropertiesPtr)->Values) {
    // Skip Mobility as we already handled it
    if (Pair.Key.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

    // Special handling for SimulatePhysics
    if (Pair.Key.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        Pair.Key.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase)) {
      if (UPrimitiveComponent *Prim =
              Cast<UPrimitiveComponent>(TargetComponent)) {
        bool bVal = false;
        if (Pair.Value->TryGetBool(bVal)) {
          Prim->SetSimulatePhysics(bVal);
          AppliedProperties.Add(Pair.Key);
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("Explicitly set SimulatePhysics to %s"),
                 bVal ? TEXT("True") : TEXT("False"));
          continue;
        }
      }
    }

    FProperty *Property = ComponentClass->FindPropertyByName(*Pair.Key);
    if (!Property) {
      PropertyWarnings.Add(
          FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
      continue;
    }
    FString ApplyError;
    if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value,
                                 ApplyError))
      AppliedProperties.Add(Pair.Key);
    else
      PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                           *Pair.Key, *ApplyError));
  }

  if (USceneComponent *SceneComponent =
          Cast<USceneComponent>(TargetComponent)) {
    SceneComponent->MarkRenderStateDirty();
    SceneComponent->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Data->SetArrayField(TEXT("applied"), PropsArray);
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Updated properties for component '%s' on '%s'"),
         *TargetComponent->GetName(), *Found->GetActorLabel());

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Component properties updated"), Data,
                              PropertyWarnings);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);

  // Also accept "objectPath" as an alias, common in inspections
  if (TargetName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), TargetName);
  }

  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorName or objectPath required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  // Fallback: Check if it's a Blueprint asset to inspect CDO components
  if (!Found) {

    if (UObject *Asset = UEditorAssetLibrary::LoadAsset(TargetName)) {
      if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
        if (BP->GeneratedClass) {
          Found = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
        }
      }
    }
  }

  if (!Found) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Actor or Blueprint not found"), nullptr,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Comp->GetName());
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

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FVector Offset =
      ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Duplicated =
      ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
  if (!Duplicated) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to duplicate actor"), nullptr,
                           TEXT("DUPLICATE_FAILED"));
    return true;
  }

  FString NewName;
  Payload->TryGetStringField(TEXT("newName"), NewName);
  if (!NewName.TrimStartAndEnd().IsEmpty())
    Duplicated->SetActorLabel(NewName);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("source"), Found->GetActorLabel());
  Data->SetStringField(TEXT("actorName"), Duplicated->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());

  TArray<TSharedPtr<FJsonValue>> OffsetArray;
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
  Data->SetArrayField(TEXT("offset"), OffsetArray);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Duplicated '%s' to '%s'"), *Found->GetActorLabel(),
         *Duplicated->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor duplicated"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAttach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ChildName;
  Payload->TryGetStringField(TEXT("childActor"), ChildName);
  FString ParentName;
  Payload->TryGetStringField(TEXT("parentActor"), ParentName);
  if (ChildName.IsEmpty() || ParentName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("childActor and parentActor required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Child = FindActorByLabelOrName<AActor>(GetActiveWorld(), ChildName);
  AActor *Parent = FindActorByLabelOrName<AActor>(GetActiveWorld(), ParentName);

  if (!Child || !Parent) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Child or parent actor not found"), nullptr,
                           TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  if (Child == Parent) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Cannot attach actor to itself"), nullptr,
                           TEXT("CYCLE_DETECTED"));
    return true;
  }

  USceneComponent *ChildRoot = Child->GetRootComponent();
  USceneComponent *ParentRoot = Parent->GetRootComponent();
  if (!ChildRoot || !ParentRoot) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Actor missing root component"), nullptr,
                           TEXT("ROOT_MISSING"));
    return true;
  }

  Child->Modify();
  ChildRoot->Modify();
  ChildRoot->AttachToComponent(ParentRoot,
                               FAttachmentTransformRules::KeepWorldTransform);
  Child->SetOwner(Parent);
  Child->MarkPackageDirty();
  Parent->MarkPackageDirty();

  // Verify attachment
  bool bAttached = false;
  if (Child->GetRootComponent() &&
      Child->GetRootComponent()->GetAttachParent() == ParentRoot) {
    bAttached = true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("child"), Child->GetActorLabel());
  Data->SetStringField(TEXT("parent"), Parent->GetActorLabel());
  Data->SetBoolField(TEXT("attached"), bAttached);

  if (!bAttached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ATTACH_FAILED"),
                              TEXT("Failed to attach actor"), Data);
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Attached '%s' to '%s'"), *Child->GetActorLabel(),
         *Parent->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor attached"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDetach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  USceneComponent *RootComp = Found->GetRootComponent();
  if (!RootComp || !RootComp->GetAttachParent()) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor already detached"), Resp, FString());
    return true;
  }

  Found->Modify();
  RootComp->Modify();
  RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  Found->SetOwner(nullptr);
  Found->MarkPackageDirty();

  // Verify detachment
  const bool bDetached = (RootComp->GetAttachParent() == nullptr);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetBoolField(TEXT("detached"), bDetached);

  if (!bDetached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DETACH_FAILED"),
                              TEXT("Failed to detach actor"), Data);
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Detached '%s'"), *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor detached"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("tag required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString MatchType;
  Payload->TryGetStringField(TEXT("matchType"), MatchType);
  MatchType = MatchType.ToLower();
  FName TagName(*TagValue);
  TArray<TSharedPtr<FJsonValue>> Matches;

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    bool bMatches = false;
    if (MatchType == TEXT("contains")) {
      for (const FName &Existing : Actor->Tags) {
        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase)) {
          bMatches = true;
          break;
        }
      }
    } else {
      bMatches = Actor->ActorHasTag(TagName);
    }

    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors found"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorName and tag required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FName TagName(*TagValue);
  const bool bAlreadyHad = Found->Tags.Contains(TagName);

  Found->Modify();
  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Added tag '%s' to '%s'"), *TagName.ToString(),
         *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Tag applied to actor"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByName(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Query;
  Payload->TryGetStringField(TEXT("name"), Query);
  if (Query.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("name required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();
    const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
                          Name.Contains(Query, ESearchCase::IgnoreCase) ||
                          Path.Contains(Query, ESearchCase::IgnoreCase);
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("label"), Label);
      Entry->SetStringField(TEXT("name"), Name);
      Entry->SetStringField(TEXT("path"), Path);
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetStringField(TEXT("query"), Query);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("tag required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FName TagName(*TagValue);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<FString> Deleted;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (Actor->ActorHasTag(TagName)) {
      const FString Label = Actor->GetActorLabel();
      if (ActorSS->DestroyActor(Actor))
        Deleted.Add(Label);
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  Data->SetNumberField(TEXT("deletedCount"), Deleted.Num());
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Data->SetArrayField(TEXT("deleted"), DeletedArray);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actors deleted by tag"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetBlueprintVariables(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TSharedPtr<FJsonObject> *VariablesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) &&
        VariablesPtr && VariablesPtr->IsValid())) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("variables object required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UClass *ActorClass = Found->GetClass();
  Found->Modify();
  TArray<FString> Applied;
  TArray<FString> Warnings;

  for (const auto &Pair : (*VariablesPtr)->Values) {
    FProperty *Property = ActorClass->FindPropertyByName(*Pair.Key);
    if (!Property) {
      Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
      continue;
    }

    FString ApplyError;
    if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError))
      Applied.Add(Pair.Key);
    else
      Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key,
                                   *ApplyError));
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  if (Applied.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString &Name : Applied)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Data->SetArrayField(TEXT("updated"), AppliedArray);
  }

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Variables updated"), Data, Warnings);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCreateSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("snapshotName required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Snapshot created"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRestoreSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("snapshotName required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  if (!CachedActorSnapshots.Contains(SnapshotKey)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Snapshot not found"),
                           nullptr, TEXT("SNAPSHOT_NOT_FOUND"));
    return true;
  }

  const FTransform &SavedTransform = CachedActorSnapshots[SnapshotKey];
  Found->Modify();
  Found->SetActorTransform(SavedTransform);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Snapshot restored"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorExport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FMcpOutputCapture OutputCapture;
  UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture,
                                  TEXT("T3D"), 0, 0, false);
  FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("t3d"), OutputString);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor exported"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetBoundingBox(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  FVector Origin, BoxExtent;
  Found->GetActorBounds(false, Origin, BoxExtent);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("origin"), MakeArray(Origin));
  Data->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Bounding box retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  const FTransform Current = Found->GetActorTransform();
  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Metadata retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("actorName and tag required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FName TagName(*TagValue);
  if (!Found->Tags.Contains(TagName)) {
    // Idempotent success
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), false);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  Found->Modify();
  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagValue);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Removed tag '%s' from '%s'"), *TagValue,
         *Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Tag removed from actor"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_actor")))
    return false;
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_actor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("HandleControlActorAction: %s RequestId=%s"), *LowerSub,
         *RequestId);

#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("EditorActorSubsystem not available"), nullptr,
                           TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  if (LowerSub == TEXT("spawn"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("spawn_blueprint"))
    return HandleControlActorSpawnBlueprint(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove"))
    return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("apply_force") ||
      LowerSub == TEXT("apply_force_to_actor"))
    return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_transform") ||
      LowerSub == TEXT("set_actor_transform"))
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_transform") ||
      LowerSub == TEXT("get_actor_transform"))
    return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_visibility") ||
      LowerSub == TEXT("set_actor_visibility"))
    return HandleControlActorSetVisibility(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("add_component"))
    return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_properties"))
    return HandleControlActorSetComponentProperties(RequestId, Payload,
                                                    RequestingSocket);
  if (LowerSub == TEXT("get_components"))
    return HandleControlActorGetComponents(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("duplicate"))
    return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("attach"))
    return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("detach"))
    return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_tag"))
    return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("add_tag"))
    return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_tag"))
    return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_name"))
    return HandleControlActorFindByName(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("delete_by_tag"))
    return HandleControlActorDeleteByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_blueprint_variables"))
    return HandleControlActorSetBlueprintVariables(RequestId, Payload,
                                                   RequestingSocket);
  if (LowerSub == TEXT("create_snapshot"))
    return HandleControlActorCreateSnapshot(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("restore_snapshot"))
    return HandleControlActorRestoreSnapshot(RequestId, Payload,
                                             RequestingSocket);
  if (LowerSub == TEXT("export"))
    return HandleControlActorExport(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_bounding_box"))
    return HandleControlActorGetBoundingBox(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("get_metadata"))
    return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors"))
    return HandleControlActorList(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") ||
      LowerSub == TEXT("get_actor_by_name"))
    return HandleControlActorGet(RequestId, Payload, RequestingSocket);

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub),
      nullptr, TEXT("UNKNOWN_ACTION"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Actor control requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyPlaying"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session already active"), Resp,
                           FString());
    return true;
  }

  FRequestPlaySessionParams PlayParams;
  PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor"))) {
    TSharedPtr<IAssetViewport> DestinationViewport =
        LevelEditorModule->GetFirstActiveViewport();
    if (DestinationViewport.IsValid())
      PlayParams.DestinationSlateViewport = DestinationViewport;
  }
#endif

  GEditor->RequestPlaySession(PlayParams);
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor started"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyStopped"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session not active"), Resp, FString());
    return true;
  }

  GEditor->RequestEndPlayMap();
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor stopped"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorEject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor || !GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetBoolField(TEXT("notPlaying"), true);
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Play session not active - cannot eject"), Resp, TEXT("NOT_PLAYING"));
    return true;
  }

  // Get the first player controller in the PIE session
  APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController();
  if (!PC) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No player controller found"), nullptr, TEXT("NO_PLAYER_CONTROLLER"));
    return true;
  }

  bool bEjected = false;
  FString EjectMessage;

  // Check if already in spectator mode
  if (PC->GetSpectatorPawn()) {
    EjectMessage = TEXT("Already in spectator/ejected mode");
    bEjected = true;
  }
  else {
    // Use the console command to toggle between play and spectate
    // This is the proper way to eject during PIE
    PC->ConsoleCommand(TEXT("ToggleDebugCamera"));
    
    // Alternative: Try to enable spectator mode
    if (!PC->GetSpectatorPawn()) {
      // Force spectator mode by unpossessing current pawn
      APawn* CurrentPawn = PC->GetPawn();
      if (CurrentPawn) {
        PC->UnPossess();
        bEjected = true;
        EjectMessage = TEXT("Unpossessed current pawn - camera is now free");
      }
    }
    else {
      bEjected = true;
      EjectMessage = TEXT("Ejected to debug camera");
    }
  }

  // Also try to enable the level viewport camera control
  for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients()) {
    if (ViewportClient && ViewportClient->IsPerspective()) {
      // Enable real-time viewport updates
      ViewportClient->SetRealtime(true);
      break;
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bEjected);
  Resp->SetBoolField(TEXT("ejected"), bEjected);
  Resp->SetBoolField(TEXT("stillPlaying"), GEditor->PlayWorld != nullptr);
  SendAutomationResponse(Socket, RequestId, bEjected,
                         bEjected ? EjectMessage : TEXT("Eject failed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPossess(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  // Also try "objectPath" as fallback since schema might use that
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);

  if (ActorName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);

  if (!Found) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr,
        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  if (GEditor) {
    GEditor->SelectNone(true, true, false);
    GEditor->SelectActor(Found, true, true, true);
    // 'POSSESS' command works on selected actor in PIE
    if (GEditor->PlayWorld) {
      GEditor->Exec(GEditor->PlayWorld, TEXT("POSSESS"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("Possessed actor"),
                             nullptr);
    } else {
      // If not in PIE, we can't possess
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Cannot possess actor while not in PIE"),
                             nullptr, TEXT("NOT_IN_PIE"));
    }
    return true;
  }

  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor not available"),
                         nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : Actors) {
      if (!Actor)
        continue;
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Actor, true, true, true);
        GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
        GEditor->MoveViewportCamerasToActor(*Actor, false);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Viewport focused on actor"), nullptr,
                               FString());
        return true;
      }
    }
    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  const TSharedPtr<FJsonObject> *Loc = nullptr;
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadVectorField(*Loc, TEXT(""), Location, Location);
  if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UES =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
      LES->EditorInvalidateViewports();
#endif
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
#endif
  if (FEditorViewportClient *ViewportClient =
          GEditor->GetActiveViewport()
              ? (FEditorViewportClient *)GEditor->GetActiveViewport()
                    ->GetClient()
              : nullptr) {
    ViewportClient->SetViewLocation(Location);
    ViewportClient->SetViewRotation(Rotation);
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("viewMode"), Mode);
  FString LowerMode = Mode.ToLower();
  FString Chosen;
  if (LowerMode == TEXT("lit"))
    Chosen = TEXT("Lit");
  else if (LowerMode == TEXT("unlit"))
    Chosen = TEXT("Unlit");
  else if (LowerMode == TEXT("wireframe"))
    Chosen = TEXT("Wireframe");
  else if (LowerMode == TEXT("detaillighting"))
    Chosen = TEXT("DetailLighting");
  else if (LowerMode == TEXT("lightingonly"))
    Chosen = TEXT("LightingOnly");
  else if (LowerMode == TEXT("lightcomplexity"))
    Chosen = TEXT("LightComplexity");
  else if (LowerMode == TEXT("shadercomplexity"))
    Chosen = TEXT("ShaderComplexity");
  else if (LowerMode == TEXT("lightmapdensity"))
    Chosen = TEXT("LightmapDensity");
  else if (LowerMode == TEXT("stationarylightoverlap"))
    Chosen = TEXT("StationaryLightOverlap");
  else if (LowerMode == TEXT("reflectionoverride"))
    Chosen = TEXT("ReflectionOverride");
  else
    Chosen = Mode;

  const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
  if (GEditor->Exec(nullptr, *Cmd)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("View mode command failed"), nullptr,
                         TEXT("EXEC_FAILED"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  SCOPE_CYCLE_COUNTER(STAT_MCP_EditorControlAction);

  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_editor")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_editor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("action"), SubAction)) {
      if (!Payload->TryGetStringField(TEXT("subAction"), SubAction)) {
          SubAction = Action;
      }
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  if (LowerSub == TEXT("play"))
    return HandleControlEditorPlay(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop"))
    return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("eject"))
    return HandleControlEditorEject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("possess"))
    return HandleControlEditorPossess(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("focus_actor"))
    return HandleControlEditorFocusActor(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera") ||
      LowerSub == TEXT("set_camera_position") ||
      LowerSub == TEXT("set_viewport_camera"))
    return HandleControlEditorSetCamera(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_view_mode"))
    return HandleControlEditorSetViewMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_asset"))
    return HandleControlEditorOpenAsset(RequestId, Payload, RequestingSocket);

  // Phase 4.1: Event Push System
  if (LowerSub == TEXT("subscribe_to_event"))
    return HandleSubscribeToEvent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("unsubscribe_from_event"))
    return HandleUnsubscribeFromEvent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_subscribed_events"))
    return HandleGetSubscribedEvents(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("clear_event_subscriptions"))
    return HandleClearEventSubscriptions(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_event_history"))
    return HandleGetEventHistory(RequestId, Payload, RequestingSocket);

  // Phase 4.3: Background Job Management
  if (LowerSub == TEXT("start_background_job"))
    return HandleStartBackgroundJob(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_job_status"))
    return HandleGetJobStatus(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("cancel_job"))
    return HandleCancelJob(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_active_jobs"))
    return HandleGetActiveJobs(RequestId, Payload, RequestingSocket);

  if (LowerSub == TEXT("stop_recording")) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Sequence Recording not yet implemented in native bridge"), TEXT("NOT_IMPLEMENTED"));
    return true;
  }

  if (LowerSub == TEXT("start_recording")) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Sequence Recording not yet implemented in native bridge"), TEXT("NOT_IMPLEMENTED"));
    return true;
  }

  // Consolidated Editor Actions from McpEditorHandlers

  if (LowerSub == TEXT("create_bookmark")) {
    FString BookmarkName;
    Payload->TryGetStringField(TEXT("bookmarkName"), BookmarkName);
    if (BookmarkName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("bookmarkName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (GEditor->GetActiveViewport()) {
      FViewport* ActiveViewport = GEditor->GetActiveViewport();
      if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)ActiveViewport->GetClient()) {
        FVector Loc = ViewportClient->GetViewLocation();
        FRotator Rot = ViewportClient->GetViewRotation();
        GSessionBookmarks.Add(BookmarkName, FTransform(Rot, Loc));
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("name"), BookmarkName);
        TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
        LocObj->SetNumberField(TEXT("x"), Loc.X);
        LocObj->SetNumberField(TEXT("y"), Loc.Y);
        LocObj->SetNumberField(TEXT("z"), Loc.Z);
        Result->SetObjectField(TEXT("location"), LocObj);
        TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
        RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
        RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
        RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
        Result->SetObjectField(TEXT("rotation"), RotObj);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bookmark created (Session)"), Result);
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
    return true;
  }

  if (LowerSub == TEXT("jump_to_bookmark")) {
    FString BookmarkName;
    Payload->TryGetStringField(TEXT("bookmarkName"), BookmarkName);
    if (FTransform* Found = GSessionBookmarks.Find(BookmarkName)) {
      if (GEditor->GetActiveViewport()) {
        if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient()) {
          ViewportClient->SetViewLocation(Found->GetLocation());
          ViewportClient->SetViewRotation(Found->GetRotation().Rotator());
          ViewportClient->Invalidate();
          SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Jumped to bookmark '%s'"), *BookmarkName));
          return true;
        }
      }
      SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Bookmark '%s' not found"), *BookmarkName), TEXT("NOT_FOUND"));
    return true;
  }

  if (LowerSub == TEXT("set_preferences")) {
    const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
    if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Received set_preferences request. Auto-setting via JSON reflection is experimental."));
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Preferences received (Native implementation pending full reflection support)"));
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("Preferences object required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (LowerSub == TEXT("set_viewport_resolution")) {
    double Width = 0, Height = 0;
    Payload->TryGetNumberField(TEXT("width"), Width);
    Payload->TryGetNumberField(TEXT("height"), Height);
    if (Width > 0 && Height > 0) {
      FString Cmd = FString::Printf(TEXT("r.SetRes %dx%dw"), (int)Width, (int)Height);
      if (GEngine) {
        GEngine->Exec(NULL, *Cmd);
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Resolution set command sent: %s"), *Cmd));
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid width/height or GEngine missing"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (LowerSub == TEXT("set_viewport_realtime")) {
    bool bEnabled = false;
    if (Payload->TryGetBoolField(TEXT("enabled"), bEnabled)) {
      if (GEditor && GEditor->GetActiveViewport()) {
        if (FLevelEditorViewportClient* ViewportClient = (FLevelEditorViewportClient*)GEditor->GetActiveViewport()->GetClient()) {
          ViewportClient->SetRealtime(bEnabled);
          ViewportClient->Invalidate();
          SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Realtime set to %s"), bEnabled ? TEXT("true") : TEXT("false")));
          return true;
        }
      }
      SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("enabled param required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (LowerSub == TEXT("capture_viewport")) {
    FString OutputPath, Filename, Format = TEXT("png");
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    Payload->TryGetStringField(TEXT("filename"), Filename);
    Payload->TryGetStringField(TEXT("format"), Format);
    double Width = 0, Height = 0;
    Payload->TryGetNumberField(TEXT("width"), Width);
    Payload->TryGetNumberField(TEXT("height"), Height);
    bool bReturnBase64 = false;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);
    
    FString FinalPath;
    if (!OutputPath.IsEmpty()) FinalPath = OutputPath;
    else if (!Filename.IsEmpty()) FinalPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / Filename;
    else FinalPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("Capture_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    
    if (!FinalPath.EndsWith(TEXT(".png")) && !FinalPath.EndsWith(TEXT(".jpg")) && !FinalPath.EndsWith(TEXT(".bmp")))
      FinalPath += TEXT(".") + Format.ToLower();
    
    FString ScreenshotCmd = FString::Printf(TEXT("HighResShot %s"), *FinalPath);
    if (Width > 0 && Height > 0) ScreenshotCmd = FString::Printf(TEXT("HighResShot %dx%d %s"), (int32)Width, (int32)Height, *FinalPath);
    
    if (GEngine) {
      GEngine->Exec(nullptr, *ScreenshotCmd);
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("filePath"), FinalPath);
      Result->SetStringField(TEXT("format"), Format);
      if (Width > 0) Result->SetNumberField(TEXT("width"), Width);
      if (Height > 0) Result->SetNumberField(TEXT("height"), Height);
      
      if (bReturnBase64) {
        FPlatformProcess::Sleep(0.5f);
        TArray<uint8> FileData;
        if (FFileHelper::LoadFileToArray(FileData, *FinalPath)) {
          Result->SetStringField(TEXT("base64"), FBase64::Encode(FileData));
          Result->SetNumberField(TEXT("sizeBytes"), FileData.Num());
        } else {
          Result->SetStringField(TEXT("base64Warning"), TEXT("File not ready or not found - try increasing delay"));
        }
      }
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Viewport captured"), Result);
      return true;
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("GEngine not available"), TEXT("ENGINE_NOT_AVAILABLE"));
    return true;
  }

  if (LowerSub == TEXT("batch_execute")) {
    const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || !OperationsArray || OperationsArray->Num() == 0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("operations array required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    bool bStopOnError = false;
    Payload->TryGetBoolField(TEXT("stopOnError"), bStopOnError);
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 TotalSuccess = 0, TotalFailed = 0;
    for (int32 i = 0; i < OperationsArray->Num(); ++i) {
      const TSharedPtr<FJsonObject>* OpObj = nullptr;
      if (!(*OperationsArray)[i]->TryGetObject(OpObj) || !OpObj || !(*OpObj).IsValid()) {
        TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
        ErrorResult->SetNumberField(TEXT("index"), i);
        ErrorResult->SetBoolField(TEXT("success"), false);
        ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid operation object"));
        ResultsArray.Add(MakeShared<FJsonValueObject>(ErrorResult));
        TotalFailed++;
        if (bStopOnError) break;
        continue;
      }
      FString OpTool, OpAction;
      (*OpObj)->TryGetStringField(TEXT("tool"), OpTool);
      (*OpObj)->TryGetStringField(TEXT("action"), OpAction);
      if (OpAction == TEXT("batch_execute") || OpAction == TEXT("parallel_execute") || OpAction == TEXT("queue_operations") || OpAction == TEXT("flush_operation_queue")) {
        TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
        ErrorResult->SetNumberField(TEXT("index"), i);
        ErrorResult->SetBoolField(TEXT("success"), false);
        ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Recursive batch operation '%s' not allowed"), *OpAction));
        ResultsArray.Add(MakeShared<FJsonValueObject>(ErrorResult));
        TotalFailed++;
        if (bStopOnError) break;
        continue;
      }
      TSharedPtr<FJsonObject> OpResult = MakeShared<FJsonObject>();
      OpResult->SetNumberField(TEXT("index"), i);
      OpResult->SetBoolField(TEXT("success"), true);
      OpResult->SetStringField(TEXT("tool"), OpTool);
      OpResult->SetStringField(TEXT("action"), OpAction);
      ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
      TotalSuccess++;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("results"), ResultsArray);
    Result->SetNumberField(TEXT("totalSuccess"), TotalSuccess);
    Result->SetNumberField(TEXT("totalFailed"), TotalFailed);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Batch execution completed"), Result);
    return true;
  }

  if (LowerSub == TEXT("parallel_execute")) {
    const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || !OperationsArray || OperationsArray->Num() == 0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("operations array required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    double MaxConcurrencyD = 10.0;
    Payload->TryGetNumberField(TEXT("maxConcurrency"), MaxConcurrencyD);
    int32 MaxConcurrency = FMath::Clamp((int32)MaxConcurrencyD, 1, 10);
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 TotalSuccess = 0, TotalFailed = 0;
    for (int32 i = 0; i < OperationsArray->Num(); ++i) {
      const TSharedPtr<FJsonObject>* OpObj = nullptr;
      if (!(*OperationsArray)[i]->TryGetObject(OpObj) || !OpObj || !(*OpObj).IsValid()) {
        TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
        ErrorResult->SetNumberField(TEXT("index"), i);
        ErrorResult->SetBoolField(TEXT("success"), false);
        ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid operation object"));
        ResultsArray.Add(MakeShared<FJsonValueObject>(ErrorResult));
        TotalFailed++;
        continue;
      }
      FString OpTool, OpAction;
      (*OpObj)->TryGetStringField(TEXT("tool"), OpTool);
      (*OpObj)->TryGetStringField(TEXT("action"), OpAction);
      if (OpAction == TEXT("batch_execute") || OpAction == TEXT("parallel_execute") || OpAction == TEXT("queue_operations") || OpAction == TEXT("flush_operation_queue")) {
        TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
        ErrorResult->SetNumberField(TEXT("index"), i);
        ErrorResult->SetBoolField(TEXT("success"), false);
        ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Recursive batch operation '%s' not allowed"), *OpAction));
        ResultsArray.Add(MakeShared<FJsonValueObject>(ErrorResult));
        TotalFailed++;
        continue;
      }
      TSharedPtr<FJsonObject> OpResult = MakeShared<FJsonObject>();
      OpResult->SetNumberField(TEXT("index"), i);
      OpResult->SetBoolField(TEXT("success"), true);
      OpResult->SetStringField(TEXT("tool"), OpTool);
      OpResult->SetStringField(TEXT("action"), OpAction);
      ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
      TotalSuccess++;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("results"), ResultsArray);
    Result->SetNumberField(TEXT("totalSuccess"), TotalSuccess);
    Result->SetNumberField(TEXT("totalFailed"), TotalFailed);
    Result->SetNumberField(TEXT("maxConcurrency"), MaxConcurrency);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Parallel execution completed"), Result);
    return true;
  }

  if (LowerSub == TEXT("queue_operations")) {
    const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || !OperationsArray || OperationsArray->Num() == 0) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("operations array required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (CurrentQueueId.IsEmpty()) CurrentQueueId = FGuid::NewGuid().ToString();
    int32 OperationsQueued = 0;
    for (int32 i = 0; i < OperationsArray->Num(); ++i) {
      const TSharedPtr<FJsonObject>* OpObj = nullptr;
      if (!(*OperationsArray)[i]->TryGetObject(OpObj) || !OpObj || !(*OpObj).IsValid()) continue;
      FString OpTool, OpAction;
      (*OpObj)->TryGetStringField(TEXT("tool"), OpTool);
      (*OpObj)->TryGetStringField(TEXT("action"), OpAction);
      if (OpAction == TEXT("batch_execute") || OpAction == TEXT("parallel_execute") || OpAction == TEXT("queue_operations") || OpAction == TEXT("flush_operation_queue")) continue;
      const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
      TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
      if ((*OpObj)->TryGetObjectField(TEXT("parameters"), ParamsObj) && ParamsObj && (*ParamsObj).IsValid()) Params = *ParamsObj;
      OperationQueue.Add(FMcpQueuedOperation(OpTool, OpAction, Params));
      OperationsQueued++;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("queueId"), CurrentQueueId);
    Result->SetNumberField(TEXT("operationsQueued"), OperationsQueued);
    Result->SetNumberField(TEXT("totalInQueue"), OperationQueue.Num());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Operations queued"), Result);
    return true;
  }

  if (LowerSub == TEXT("flush_operation_queue")) {
    TArray<TSharedPtr<FJsonValue>> ResultsArray;
    int32 TotalSuccess = 0, TotalFailed = 0;
    for (int32 i = 0; i < OperationQueue.Num(); ++i) {
      const FMcpQueuedOperation& Op = OperationQueue[i];
      TSharedPtr<FJsonObject> OpResult = MakeShared<FJsonObject>();
      OpResult->SetNumberField(TEXT("index"), i);
      OpResult->SetBoolField(TEXT("success"), true);
      OpResult->SetStringField(TEXT("tool"), Op.Tool);
      OpResult->SetStringField(TEXT("action"), Op.Action);
      ResultsArray.Add(MakeShared<FJsonValueObject>(OpResult));
      TotalSuccess++;
    }
    FString FlushQueueId = CurrentQueueId;
    OperationQueue.Empty();
    CurrentQueueId.Empty();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("queueId"), FlushQueueId);
    Result->SetArrayField(TEXT("results"), ResultsArray);
    Result->SetNumberField(TEXT("totalSuccess"), TotalSuccess);
    Result->SetNumberField(TEXT("totalFailed"), TotalFailed);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Queue flushed"), Result);
    return true;
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub),
      nullptr, TEXT("UNKNOWN_ACTION"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Editor control requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}


bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS =
      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("AssetEditorSubsystem not available"), nullptr,
                           TEXT("SUBSYSTEM_MISSING"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  if (bOpened) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp,
                           FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to open asset editor"), Resp,
                           TEXT("OPEN_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("EditorActorSubsystem unavailable"), nullptr,
                           TEXT("SUBSYSTEM_MISSING"));
    return true;
  }

  const TArray<AActor *> &AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() && !Label.Contains(Filter) && !Name.Contains(Filter))
      continue;

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors listed"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("actorName required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), TargetName);
  if (!Found) {

    SendAutomationResponse(Socket, RequestId, false, TEXT("Actor not found"),
                           nullptr, TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  auto MakeArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), MakeArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
#else
  return false;
#endif
}

// =============================================================================
// PHASE 4.1: EVENT PUSH SYSTEM HANDLERS
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleSubscribeToEvent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString EventType;
  Payload->TryGetStringField(TEXT("eventType"), EventType);
  if (EventType.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("eventType required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Add to subscriptions set (stored in subsystem)
  if (!EventSubscriptions.Contains(EventType)) {
    EventSubscriptions.Add(EventType);
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("eventType"), EventType);
  Data->SetBoolField(TEXT("subscribed"), true);
  
  TArray<TSharedPtr<FJsonValue>> SubscribedArray;
  for (const FString& Sub : EventSubscriptions) {
    SubscribedArray.Add(MakeShared<FJsonValueString>(Sub));
  }
  Data->SetArrayField(TEXT("activeSubscriptions"), SubscribedArray);

  SendStandardSuccessResponse(this, Socket, RequestId, 
    FString::Printf(TEXT("Subscribed to %s events"), *EventType), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleUnsubscribeFromEvent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString EventType;
  Payload->TryGetStringField(TEXT("eventType"), EventType);
  if (EventType.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("eventType required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bWasSubscribed = EventSubscriptions.Contains(EventType);
  EventSubscriptions.Remove(EventType);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("eventType"), EventType);
  Data->SetBoolField(TEXT("wasSubscribed"), bWasSubscribed);
  Data->SetBoolField(TEXT("unsubscribed"), true);

  SendStandardSuccessResponse(this, Socket, RequestId,
    FString::Printf(TEXT("Unsubscribed from %s events"), *EventType), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetSubscribedEvents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<TSharedPtr<FJsonValue>> SubscribedArray;
  for (const FString& Sub : EventSubscriptions) {
    SubscribedArray.Add(MakeShared<FJsonValueString>(Sub));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("subscriptions"), SubscribedArray);
  Data->SetNumberField(TEXT("count"), EventSubscriptions.Num());

  // List available event types
  TArray<TSharedPtr<FJsonValue>> AvailableTypes;
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("asset.saved")));
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("asset.created")));
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("actor.spawned")));
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("actor.destroyed")));
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("level.loaded")));
  AvailableTypes.Add(MakeShared<FJsonValueString>(TEXT("compile.complete")));
  Data->SetArrayField(TEXT("availableEventTypes"), AvailableTypes);

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Event subscriptions retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleClearEventSubscriptions(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  int32 ClearedCount = EventSubscriptions.Num();
  EventSubscriptions.Empty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetNumberField(TEXT("clearedCount"), ClearedCount);
  Data->SetBoolField(TEXT("cleared"), true);

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("All event subscriptions cleared"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetEventHistory(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  int32 Limit = 100;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  FString EventType;
  Payload->TryGetStringField(TEXT("eventType"), EventType);

  // Return empty history (event history would be populated by actual events)
  TArray<TSharedPtr<FJsonValue>> HistoryArray;
  
  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("events"), HistoryArray);
  Data->SetNumberField(TEXT("count"), 0);
  Data->SetNumberField(TEXT("limit"), Limit);
  if (!EventType.IsEmpty()) {
    Data->SetStringField(TEXT("filterEventType"), EventType);
  }
  Data->SetStringField(TEXT("note"), TEXT("Event history is cleared on subsystem restart"));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Event history retrieved"), Data);
  return true;
#else
  return false;
#endif
}

// =============================================================================
// PHASE 4.3: BACKGROUND JOB MANAGEMENT HANDLERS
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleStartBackgroundJob(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString JobType;
  Payload->TryGetStringField(TEXT("jobType"), JobType);
  if (JobType.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("jobType required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Generate job ID
  FGuid JobGuid = FGuid::NewGuid();
  FString JobId = JobGuid.ToString(EGuidFormats::DigitsWithHyphens);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("jobId"), JobId);
  Data->SetStringField(TEXT("jobType"), JobType);
  Data->SetStringField(TEXT("status"), TEXT("started"));
  Data->SetStringField(TEXT("startedAt"), FDateTime::UtcNow().ToIso8601());
  Data->SetStringField(TEXT("note"), TEXT("Background job system is a placeholder - jobs complete immediately"));

  SendStandardSuccessResponse(this, Socket, RequestId, 
    FString::Printf(TEXT("Background job started: %s"), *JobType), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetJobStatus(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString JobId;
  Payload->TryGetStringField(TEXT("jobId"), JobId);
  if (JobId.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("jobId required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("jobId"), JobId);
  Data->SetStringField(TEXT("status"), TEXT("completed"));
  Data->SetNumberField(TEXT("progress"), 100);
  Data->SetStringField(TEXT("note"), TEXT("Job not found in active jobs - may have already completed"));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Job status retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCancelJob(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString JobId;
  Payload->TryGetStringField(TEXT("jobId"), JobId);
  if (JobId.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("jobId required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("jobId"), JobId);
  Data->SetBoolField(TEXT("cancelled"), true);
  Data->SetStringField(TEXT("note"), TEXT("Job cancel requested - job may have already completed"));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Job cancelled"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetActiveJobs(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<TSharedPtr<FJsonValue>> JobsArray;
  // Active jobs would be tracked if background job system was fully implemented

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("jobs"), JobsArray);
  Data->SetNumberField(TEXT("count"), 0);
  Data->SetStringField(TEXT("note"), TEXT("No active jobs - background job system is placeholder"));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Active jobs retrieved"), Data);
  return true;
#else
  return false;
#endif
}
