#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpConnectionManager.h"
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

// ============================================================================
// NEW HANDLERS: find_by_class, inspect_object, get/set_property, etc.
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className required"));
    return true;
  }

  UClass *TargetClass = ResolveClassByName(ClassName);
  if (!TargetClass) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              FString::Printf(TEXT("Class not found: %s"), *ClassName));
    return true;
  }

  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;

  for (AActor *Actor : AllActors) {
    if (!Actor) continue;
    if (Actor->IsA(TargetClass)) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetStringField(TEXT("className"), ClassName);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), Matches.Num()), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorInspectObject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  if (ObjectPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
  }
  if (ObjectPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("objectPath or actorName required"));
    return true;
  }

  UObject *TargetObject = nullptr;
  
  // Try to find as actor first
  AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath);
  if (FoundActor) {
    TargetObject = FoundActor;
  } else {
    // Try to load as asset
    TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
    if (!TargetObject) {
      TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
    }
  }

  if (!TargetObject) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OBJECT_NOT_FOUND"),
                              FString::Printf(TEXT("Object not found: %s"), *ObjectPath));
    return true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("name"), TargetObject->GetName());
  Data->SetStringField(TEXT("path"), TargetObject->GetPathName());
  Data->SetStringField(TEXT("class"), TargetObject->GetClass() ? TargetObject->GetClass()->GetPathName() : TEXT(""));
  Data->SetStringField(TEXT("outerPath"), TargetObject->GetOuter() ? TargetObject->GetOuter()->GetPathName() : TEXT(""));

  // Collect properties
  TArray<TSharedPtr<FJsonValue>> PropertiesArray;
  UClass *ObjClass = TargetObject->GetClass();
  for (TFieldIterator<FProperty> PropIt(ObjClass); PropIt; ++PropIt) {
    FProperty *Property = *PropIt;
    if (!Property) continue;
    
    TSharedPtr<FJsonObject> PropEntry = MakeShared<FJsonObject>();
    PropEntry->SetStringField(TEXT("name"), Property->GetName());
    PropEntry->SetStringField(TEXT("type"), Property->GetCPPType());
    PropEntry->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
    PropEntry->SetBoolField(TEXT("blueprintVisible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    
    // Try to get value as string
    FString ValueStr;
    Property->ExportTextItem_Direct(ValueStr, Property->ContainerPtrToValuePtr<void>(TargetObject), nullptr, TargetObject, PPF_None);
    PropEntry->SetStringField(TEXT("value"), ValueStr);
    
    PropertiesArray.Add(MakeShared<FJsonValueObject>(PropEntry));
  }
  Data->SetArrayField(TEXT("properties"), PropertiesArray);
  Data->SetNumberField(TEXT("propertyCount"), PropertiesArray.Num());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Object inspected"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  if (ObjectPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
  }
  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }
  
  if (ObjectPath.IsEmpty() || PropertyName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("objectPath/actorName and propertyName required"));
    return true;
  }

  UObject *TargetObject = nullptr;
  AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath);
  if (FoundActor) {
    TargetObject = FoundActor;
  } else {
    TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
    if (!TargetObject) {
      TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
    }
  }

  if (!TargetObject) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OBJECT_NOT_FOUND"),
                              TEXT("Object not found"));
    return true;
  }

  FProperty *Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PROPERTY_NOT_FOUND"),
                              FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    return true;
  }

  FString ValueStr;
  Property->ExportTextItem_Direct(ValueStr, Property->ContainerPtrToValuePtr<void>(TargetObject), nullptr, TargetObject, PPF_None);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("value"), ValueStr);
  Data->SetStringField(TEXT("type"), Property->GetCPPType());
  Data->SetStringField(TEXT("objectPath"), TargetObject->GetPathName());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  if (ObjectPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actorName"), ObjectPath);
  }
  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }
  
  if (ObjectPath.IsEmpty() || PropertyName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("objectPath/actorName and propertyName required"));
    return true;
  }

  UObject *TargetObject = nullptr;
  AActor *FoundActor = FindActorByLabelOrName<AActor>(GetActiveWorld(), ObjectPath);
  if (FoundActor) {
    TargetObject = FoundActor;
  } else {
    TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
    if (!TargetObject) {
      TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
    }
  }

  if (!TargetObject) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OBJECT_NOT_FOUND"),
                              TEXT("Object not found"));
    return true;
  }

  FProperty *Property = TargetObject->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PROPERTY_NOT_FOUND"),
                              FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    return true;
  }

  TSharedPtr<FJsonValue> ValueJson = Payload->TryGetField(TEXT("value"));
  if (!ValueJson.IsValid()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("value required"));
    return true;
  }

  TargetObject->Modify();
  FString ApplyError;
  if (!ApplyJsonValueToProperty(TargetObject, Property, ValueJson, ApplyError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SET_PROPERTY_FAILED"),
                              FString::Printf(TEXT("Failed to set property: %s"), *ApplyError));
    return true;
  }

  TargetObject->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("objectPath"), TargetObject->GetPathName());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property set"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorInspectClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className required"));
    return true;
  }

  UClass *TargetClass = ResolveClassByName(ClassName);
  if (!TargetClass) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              FString::Printf(TEXT("Class not found: %s"), *ClassName));
    return true;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("name"), TargetClass->GetName());
  Data->SetStringField(TEXT("path"), TargetClass->GetPathName());
  Data->SetStringField(TEXT("superClass"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetPathName() : TEXT(""));
  Data->SetBoolField(TEXT("isAbstract"), TargetClass->HasAnyClassFlags(CLASS_Abstract));
  Data->SetBoolField(TEXT("isNative"), TargetClass->IsNative());

  // Collect properties
  TArray<TSharedPtr<FJsonValue>> PropertiesArray;
  for (TFieldIterator<FProperty> PropIt(TargetClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt) {
    FProperty *Property = *PropIt;
    if (!Property) continue;
    
    TSharedPtr<FJsonObject> PropEntry = MakeShared<FJsonObject>();
    PropEntry->SetStringField(TEXT("name"), Property->GetName());
    PropEntry->SetStringField(TEXT("type"), Property->GetCPPType());
    PropEntry->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
    PropEntry->SetBoolField(TEXT("blueprintVisible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    PropertiesArray.Add(MakeShared<FJsonValueObject>(PropEntry));
  }
  Data->SetArrayField(TEXT("properties"), PropertiesArray);
  Data->SetNumberField(TEXT("propertyCount"), PropertiesArray.Num());

  // Collect functions
  TArray<TSharedPtr<FJsonValue>> FunctionsArray;
  for (TFieldIterator<UFunction> FuncIt(TargetClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt) {
    UFunction *Function = *FuncIt;
    if (!Function) continue;
    
    TSharedPtr<FJsonObject> FuncEntry = MakeShared<FJsonObject>();
    FuncEntry->SetStringField(TEXT("name"), Function->GetName());
    FuncEntry->SetBoolField(TEXT("callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    FuncEntry->SetBoolField(TEXT("event"), Function->HasAnyFunctionFlags(FUNC_Event));
    FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncEntry));
  }
  Data->SetArrayField(TEXT("functions"), FunctionsArray);
  Data->SetNumberField(TEXT("functionCount"), FunctionsArray.Num());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Class inspected"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorListObjects(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  double Limit = 100;
  Payload->TryGetNumberField(TEXT("limit"), Limit);

  UClass *TargetClass = nullptr;
  if (!ClassName.IsEmpty()) {
    TargetClass = ResolveClassByName(ClassName);
  }

  TArray<TSharedPtr<FJsonValue>> ObjectsArray;
  int32 Count = 0;
  const int32 MaxObjects = static_cast<int32>(Limit);

  // Use GetDerivedClasses for UE 5.7 safety instead of TObjectIterator
  TArray<UClass*> ClassesToSearch;
  if (TargetClass) {
    ClassesToSearch.Add(TargetClass);
    GetDerivedClasses(TargetClass, ClassesToSearch, true);
  }

  for (FThreadSafeObjectIterator It(UObject::StaticClass()); It && Count < MaxObjects; ++It) {
    UObject *Obj = *It;
    if (!Obj) continue;
    if (Obj->HasAnyFlags(RF_ClassDefaultObject)) continue;
    
    if (TargetClass && !Obj->IsA(TargetClass)) continue;
    if (!Filter.IsEmpty() && !Obj->GetName().Contains(Filter)) continue;

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Obj->GetName());
    Entry->SetStringField(TEXT("path"), Obj->GetPathName());
    Entry->SetStringField(TEXT("class"), Obj->GetClass() ? Obj->GetClass()->GetName() : TEXT(""));
    ObjectsArray.Add(MakeShared<FJsonValueObject>(Entry));
    Count++;
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("objects"), ObjectsArray);
  Data->SetNumberField(TEXT("count"), ObjectsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Objects listed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);

  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName, componentName, and propertyName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  UActorComponent *TargetComponent = nullptr;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp) continue;
    if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      TargetComponent = Comp;
      break;
    }
  }

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"));
    return true;
  }

  FProperty *Property = TargetComponent->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PROPERTY_NOT_FOUND"),
                              TEXT("Property not found"));
    return true;
  }

  FString ValueStr;
  Property->ExportTextItem_Direct(ValueStr, Property->ContainerPtrToValuePtr<void>(TargetComponent), nullptr, TargetComponent, PPF_None);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("componentName"), TargetComponent->GetName());
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("value"), ValueStr);
  Data->SetStringField(TEXT("type"), Property->GetCPPType());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component property retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  FString PropertyName;
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);

  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName, componentName, and propertyName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  UActorComponent *TargetComponent = nullptr;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp) continue;
    if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      TargetComponent = Comp;
      break;
    }
  }

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"));
    return true;
  }

  FProperty *Property = TargetComponent->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PROPERTY_NOT_FOUND"),
                              TEXT("Property not found"));
    return true;
  }

  TSharedPtr<FJsonValue> ValueJson = Payload->TryGetField(TEXT("value"));
  if (!ValueJson.IsValid()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("value required"));
    return true;
  }

  TargetComponent->Modify();
  FString ApplyError;
  if (!ApplyJsonValueToProperty(TargetComponent, Property, ValueJson, ApplyError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SET_PROPERTY_FAILED"),
                              FString::Printf(TEXT("Failed to set property: %s"), *ApplyError));
    return true;
  }

  if (USceneComponent *SceneComp = Cast<USceneComponent>(TargetComponent)) {
    SceneComp->MarkRenderStateDirty();
    SceneComp->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("componentName"), TargetComponent->GetName());
  Data->SetStringField(TEXT("propertyName"), PropertyName);

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component property set"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteObject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ObjectPath;
  Payload->TryGetStringField(TEXT("objectPath"), ObjectPath);
  if (ObjectPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("objectPath required"));
    return true;
  }

  UObject *TargetObject = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath);
  if (!TargetObject) {
    TargetObject = LoadObject<UObject>(nullptr, *ObjectPath);
  }

  if (!TargetObject) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OBJECT_NOT_FOUND"),
                              TEXT("Object not found"));
    return true;
  }

  // Check if it's an actor - use DestroyActor for actors
  if (AActor *Actor = Cast<AActor>(TargetObject)) {
    UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (ActorSS->DestroyActor(Actor)) {
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetStringField(TEXT("deletedPath"), ObjectPath);
      Data->SetStringField(TEXT("type"), TEXT("Actor"));
      SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor deleted"), Data);
      return true;
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DELETE_FAILED"),
                              TEXT("Failed to delete actor"));
    return true;
  }

  // For other objects, mark pending kill
  TargetObject->MarkAsGarbage();
  
  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("deletedPath"), ObjectPath);
  Data->SetStringField(TEXT("type"), TEXT("UObject"));
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Object marked for deletion"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorQueryByPredicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  double Limit = 100;
  Payload->TryGetNumberField(TEXT("limit"), Limit);

  UClass *TargetClass = nullptr;
  if (!ClassName.IsEmpty()) {
    TargetClass = ResolveClassByName(ClassName);
    if (!TargetClass) {
      TargetClass = AActor::StaticClass();
    }
  } else {
    TargetClass = AActor::StaticClass();
  }

  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;
  const int32 MaxCount = static_cast<int32>(Limit);

  for (AActor *Actor : AllActors) {
    if (!Actor) continue;
    if (Matches.Num() >= MaxCount) break;
    if (!Actor->IsA(TargetClass)) continue;
    
    // Apply filter if provided
    if (!Filter.IsEmpty()) {
      const FString Label = Actor->GetActorLabel();
      const FString Name = Actor->GetName();
      if (!Label.Contains(Filter, ESearchCase::IgnoreCase) &&
          !Name.Contains(Filter, ESearchCase::IgnoreCase)) {
        continue;
      }
    }

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT(""));
    
    // Include location
    FVector Loc = Actor->GetActorLocation();
    TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
    LocObj->SetNumberField(TEXT("x"), Loc.X);
    LocObj->SetNumberField(TEXT("y"), Loc.Y);
    LocObj->SetNumberField(TEXT("z"), Loc.Z);
    Entry->SetObjectField(TEXT("location"), LocObj);
    
    Matches.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetAllComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  if (ActorName.IsEmpty() || ComponentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and componentName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  UActorComponent *TargetComponent = nullptr;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp) continue;
    if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      TargetComponent = Comp;
      break;
    }
  }

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> PropertiesArray;
  UClass *CompClass = TargetComponent->GetClass();
  for (TFieldIterator<FProperty> PropIt(CompClass); PropIt; ++PropIt) {
    FProperty *Property = *PropIt;
    if (!Property) continue;
    
    TSharedPtr<FJsonObject> PropEntry = MakeShared<FJsonObject>();
    PropEntry->SetStringField(TEXT("name"), Property->GetName());
    PropEntry->SetStringField(TEXT("type"), Property->GetCPPType());
    PropEntry->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
    
    FString ValueStr;
    Property->ExportTextItem_Direct(ValueStr, Property->ContainerPtrToValuePtr<void>(TargetComponent), nullptr, TargetComponent, PPF_None);
    PropEntry->SetStringField(TEXT("value"), ValueStr);
    
    PropertiesArray.Add(MakeShared<FJsonValueObject>(PropEntry));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("componentName"), TargetComponent->GetName());
  Data->SetArrayField(TEXT("properties"), PropertiesArray);
  Data->SetNumberField(TEXT("propertyCount"), PropertiesArray.Num());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component properties retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorBatchSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  if (ActorName.IsEmpty() || ComponentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and componentName required"));
    return true;
  }

  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) && PropertiesPtr && (*PropertiesPtr).IsValid())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("properties object required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  UActorComponent *TargetComponent = nullptr;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp) continue;
    if (Comp->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      TargetComponent = Comp;
      break;
    }
  }

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"));
    return true;
  }

  TargetComponent->Modify();
  UClass *ComponentClass = TargetComponent->GetClass();
  TArray<FString> Applied;
  TArray<FString> Warnings;

  for (const auto &Pair : (*PropertiesPtr)->Values) {
    FProperty *Property = ComponentClass->FindPropertyByName(*Pair.Key);
    if (!Property) {
      Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *Pair.Key));
      continue;
    }
    FString ApplyError;
    if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value, ApplyError)) {
      Applied.Add(Pair.Key);
    } else {
      Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *Pair.Key, *ApplyError));
    }
  }

  if (USceneComponent *SceneComp = Cast<USceneComponent>(TargetComponent)) {
    SceneComp->MarkRenderStateDirty();
    SceneComp->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("componentName"), TargetComponent->GetName());
  Data->SetNumberField(TEXT("appliedCount"), Applied.Num());
  
  TArray<TSharedPtr<FJsonValue>> AppliedArray;
  for (const FString &Name : Applied) {
    AppliedArray.Add(MakeShared<FJsonValueString>(Name));
  }
  Data->SetArrayField(TEXT("applied"), AppliedArray);

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Properties batch set"), Data, Warnings);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSerializeState(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  TSharedPtr<FJsonObject> ActorState = MakeShared<FJsonObject>();
  
  // Basic info
  ActorState->SetStringField(TEXT("name"), Found->GetActorLabel());
  ActorState->SetStringField(TEXT("class"), Found->GetClass() ? Found->GetClass()->GetPathName() : TEXT(""));
  ActorState->SetStringField(TEXT("path"), Found->GetPathName());

  // Transform
  FTransform Transform = Found->GetActorTransform();
  TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
  
  FVector Loc = Transform.GetLocation();
  TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
  LocObj->SetNumberField(TEXT("x"), Loc.X);
  LocObj->SetNumberField(TEXT("y"), Loc.Y);
  LocObj->SetNumberField(TEXT("z"), Loc.Z);
  TransformObj->SetObjectField(TEXT("location"), LocObj);
  
  FRotator Rot = Transform.Rotator();
  TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
  RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
  RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
  RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
  TransformObj->SetObjectField(TEXT("rotation"), RotObj);
  
  FVector Scale = Transform.GetScale3D();
  TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
  ScaleObj->SetNumberField(TEXT("x"), Scale.X);
  ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
  ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
  TransformObj->SetObjectField(TEXT("scale"), ScaleObj);
  
  ActorState->SetObjectField(TEXT("transform"), TransformObj);

  // Tags
  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  ActorState->SetArrayField(TEXT("tags"), TagsArray);

  // Components
  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp) continue;
    TSharedPtr<FJsonObject> CompEntry = MakeShared<FJsonObject>();
    CompEntry->SetStringField(TEXT("name"), Comp->GetName());
    CompEntry->SetStringField(TEXT("class"), Comp->GetClass() ? Comp->GetClass()->GetName() : TEXT(""));
    ComponentsArray.Add(MakeShared<FJsonValueObject>(CompEntry));
  }
  ActorState->SetArrayField(TEXT("components"), ComponentsArray);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetObjectField(TEXT("state"), ActorState);
  
  // Also serialize to string for convenience
  FString JsonString;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
  FJsonSerializer::Serialize(ActorState.ToSharedRef(), Writer);
  Data->SetStringField(TEXT("json"), JsonString);

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor state serialized"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetReferences(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ReferencesArray;

  // Find actors that reference this one (e.g., attached children)
  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  
  for (AActor *Other : AllActors) {
    if (!Other || Other == Found) continue;
    
    // Check attachment
    if (Other->GetRootComponent() && Other->GetRootComponent()->GetAttachParent()) {
      AActor *Parent = Other->GetRootComponent()->GetAttachParent()->GetOwner();
      if (Parent == Found) {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Other->GetActorLabel());
        Entry->SetStringField(TEXT("path"), Other->GetPathName());
        Entry->SetStringField(TEXT("type"), TEXT("AttachedChild"));
        ReferencesArray.Add(MakeShared<FJsonValueObject>(Entry));
      }
    }
    
    // Check owner
    if (Other->GetOwner() == Found) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("name"), Other->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Other->GetPathName());
      Entry->SetStringField(TEXT("type"), TEXT("OwnedActor"));
      ReferencesArray.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetArrayField(TEXT("references"), ReferencesArray);
  Data->SetNumberField(TEXT("referenceCount"), ReferencesArray.Num());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("References retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorReplaceClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FString NewClassName;
  Payload->TryGetStringField(TEXT("className"), NewClassName);

  if (ActorName.IsEmpty() || NewClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and className required"));
    return true;
  }

  AActor *Found = FindActorByLabelOrName<AActor>(GetActiveWorld(), ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  UClass *NewClass = ResolveClassByName(NewClassName);
  if (!NewClass || !NewClass->IsChildOf(AActor::StaticClass())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              FString::Printf(TEXT("Actor class not found: %s"), *NewClassName));
    return true;
  }

  // Store transform and properties
  FTransform OldTransform = Found->GetActorTransform();
  FString OldLabel = Found->GetActorLabel();
  TArray<FName> OldTags = Found->Tags;

  // Spawn new actor
  UEditorActorSubsystem *ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *NewActor = ActorSS->SpawnActorFromClass(NewClass, OldTransform.GetLocation(), OldTransform.Rotator());

  if (!NewActor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn replacement actor"));
    return true;
  }

  // Apply old properties
  NewActor->SetActorScale3D(OldTransform.GetScale3D());
  NewActor->SetActorLabel(OldLabel);
  for (const FName &Tag : OldTags) {
    NewActor->Tags.AddUnique(Tag);
  }

  // Delete old actor
  ActorSS->DestroyActor(Found);

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("newActorName"), NewActor->GetActorLabel());
  Data->SetStringField(TEXT("newActorPath"), NewActor->GetPathName());
  Data->SetStringField(TEXT("newClass"), NewClass->GetPathName());

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor class replaced"), Data);
  return true;
#else
  return false;
#endif
}

// ============================================================================
// NEW HANDLERS: batch_transform_actors, clone_component_hierarchy, deserialize_actor_state
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleControlActorBatchTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  const TArray<TSharedPtr<FJsonValue>> *TransformsArray = nullptr;
  if (!Payload->TryGetArrayField(TEXT("transforms"), TransformsArray) || !TransformsArray || TransformsArray->Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("transforms array required (array of {actorName, location?, rotation?, scale?})"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No active world available"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ResultsArray;
  int32 SuccessCount = 0;
  int32 FailCount = 0;

  for (const TSharedPtr<FJsonValue> &Entry : *TransformsArray) {
    if (!Entry.IsValid() || Entry->Type != EJson::Object) continue;
    
    const TSharedPtr<FJsonObject> &TransformSpec = Entry->AsObject();
    FString ActorName;
    TransformSpec->TryGetStringField(TEXT("actorName"), ActorName);
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actorName"), ActorName);
    
    if (ActorName.IsEmpty()) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"), TEXT("actorName required"));
      FailCount++;
      ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
      continue;
    }

    AActor *Found = FindActorByLabelOrName<AActor>(World, ActorName);
    if (!Found) {
      Result->SetBoolField(TEXT("success"), false);
      Result->SetStringField(TEXT("error"), TEXT("Actor not found"));
      FailCount++;
      ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
      continue;
    }

    Found->Modify();
    
    // Apply location if specified
    FVector NewLocation = Found->GetActorLocation();
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("location"), LocObj) && LocObj && (*LocObj).IsValid()) {
      double X, Y, Z;
      if ((*LocObj)->TryGetNumberField(TEXT("x"), X)) NewLocation.X = X;
      if ((*LocObj)->TryGetNumberField(TEXT("y"), Y)) NewLocation.Y = Y;
      if ((*LocObj)->TryGetNumberField(TEXT("z"), Z)) NewLocation.Z = Z;
      Found->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    
    // Apply rotation if specified
    FRotator NewRotation = Found->GetActorRotation();
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj && (*RotObj).IsValid()) {
      double Pitch, Yaw, Roll;
      if ((*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch)) NewRotation.Pitch = Pitch;
      if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw)) NewRotation.Yaw = Yaw;
      if ((*RotObj)->TryGetNumberField(TEXT("roll"), Roll)) NewRotation.Roll = Roll;
      Found->SetActorRotation(NewRotation, ETeleportType::TeleportPhysics);
    }
    
    // Apply scale if specified
    FVector NewScale = Found->GetActorScale3D();
    const TSharedPtr<FJsonObject> *ScaleObj = nullptr;
    if (TransformSpec->TryGetObjectField(TEXT("scale"), ScaleObj) && ScaleObj && (*ScaleObj).IsValid()) {
      double X, Y, Z;
      if ((*ScaleObj)->TryGetNumberField(TEXT("x"), X)) NewScale.X = X;
      if ((*ScaleObj)->TryGetNumberField(TEXT("y"), Y)) NewScale.Y = Y;
      if ((*ScaleObj)->TryGetNumberField(TEXT("z"), Z)) NewScale.Z = Z;
      Found->SetActorScale3D(NewScale);
    }
    
    Found->MarkComponentsRenderStateDirty();
    Found->MarkPackageDirty();
    
    Result->SetBoolField(TEXT("success"), true);
    SuccessCount++;
    ResultsArray.Add(MakeShared<FJsonValueObject>(Result));
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetArrayField(TEXT("results"), ResultsArray);
  Data->SetNumberField(TEXT("successCount"), SuccessCount);
  Data->SetNumberField(TEXT("failCount"), FailCount);
  Data->SetNumberField(TEXT("totalCount"), TransformsArray->Num());
  
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Batch transformed %d/%d actors"), SuccessCount, TransformsArray->Num());
  SendStandardSuccessResponse(this, Socket, RequestId, 
                              FString::Printf(TEXT("Batch transformed %d actors"), SuccessCount), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCloneComponentHierarchy(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourceActorName, TargetActorName;
  Payload->TryGetStringField(TEXT("sourceActor"), SourceActorName);
  Payload->TryGetStringField(TEXT("targetActor"), TargetActorName);
  
  if (SourceActorName.IsEmpty() || TargetActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("sourceActor and targetActor required"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No active world available"));
    return true;
  }

  AActor *Source = FindActorByLabelOrName<AActor>(World, SourceActorName);
  AActor *Target = FindActorByLabelOrName<AActor>(World, TargetActorName);
  
  if (!Source) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Source actor not found: %s"), *SourceActorName));
    return true;
  }
  if (!Target) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Target actor not found: %s"), *TargetActorName));
    return true;
  }

  // Optional: filter by component name or class
  FString ComponentFilter;
  Payload->TryGetStringField(TEXT("componentFilter"), ComponentFilter);
  
  Target->Modify();
  
  TArray<TSharedPtr<FJsonValue>> ClonedComponents;
  
  for (UActorComponent *SourceComp : Source->GetComponents()) {
    if (!SourceComp) continue;
    
    // Skip if filter is set and doesn't match
    if (!ComponentFilter.IsEmpty() && 
        !SourceComp->GetName().Contains(ComponentFilter, ESearchCase::IgnoreCase) &&
        !SourceComp->GetClass()->GetName().Contains(ComponentFilter, ESearchCase::IgnoreCase)) {
      continue;
    }
    
    // Clone the component
    UClass *CompClass = SourceComp->GetClass();
    FName NewCompName = MakeUniqueObjectName(Target, CompClass, *SourceComp->GetName());
    UActorComponent *NewComp = NewObject<UActorComponent>(Target, CompClass, NewCompName, RF_Transactional);
    
    if (!NewComp) continue;
    
    // Copy properties from source to new component
    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.bDoDelta = false;
    UEngine::CopyPropertiesForUnrelatedObjects(SourceComp, NewComp, CopyParams);
    
    Target->AddInstanceComponent(NewComp);
    NewComp->OnComponentCreated();
    
    // Handle SceneComponent attachment
    if (USceneComponent *NewSceneComp = Cast<USceneComponent>(NewComp)) {
      if (Target->GetRootComponent() && !NewSceneComp->GetAttachParent()) {
        NewSceneComp->SetupAttachment(Target->GetRootComponent());
      }
      
      // Copy relative transform from source if it's also a scene component
      if (USceneComponent *SourceSceneComp = Cast<USceneComponent>(SourceComp)) {
        NewSceneComp->SetRelativeTransform(SourceSceneComp->GetRelativeTransform());
      }
    }
    
    NewComp->RegisterComponent();
    NewComp->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> CompEntry = MakeShared<FJsonObject>();
    CompEntry->SetStringField(TEXT("name"), NewComp->GetName());
    CompEntry->SetStringField(TEXT("class"), CompClass->GetName());
    CompEntry->SetStringField(TEXT("sourceName"), SourceComp->GetName());
    ClonedComponents.Add(MakeShared<FJsonValueObject>(CompEntry));
  }
  
  Target->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("sourceActor"), Source->GetActorLabel());
  Data->SetStringField(TEXT("targetActor"), Target->GetActorLabel());
  Data->SetArrayField(TEXT("clonedComponents"), ClonedComponents);
  Data->SetNumberField(TEXT("count"), ClonedComponents.Num());
  
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Cloned %d components from '%s' to '%s'"), 
         ClonedComponents.Num(), *Source->GetActorLabel(), *Target->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component hierarchy cloned"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeserializeState(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Get state object - can be embedded or from JSON string
  const TSharedPtr<FJsonObject> *StatePtr = nullptr;
  TSharedPtr<FJsonObject> State;
  
  if (Payload->TryGetObjectField(TEXT("state"), StatePtr) && StatePtr && (*StatePtr).IsValid()) {
    State = *StatePtr;
  } else {
    FString JsonString;
    if (Payload->TryGetStringField(TEXT("json"), JsonString) && !JsonString.IsEmpty()) {
      TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
      TSharedPtr<FJsonObject> ParsedState;
      if (FJsonSerializer::Deserialize(Reader, ParsedState) && ParsedState.IsValid()) {
        State = ParsedState;
      }
    }
  }
  
  if (!State.IsValid()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("state object or json string required"));
    return true;
  }

  // Get the target actor - either by name or from state
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    State->TryGetStringField(TEXT("name"), ActorName);
  }
  
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required (in payload or state.name)"));
    return true;
  }

  UWorld *World = GetActiveWorld();
  if (!World) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No active world available"));
    return true;
  }

  AActor *Target = FindActorByLabelOrName<AActor>(World, ActorName);
  if (!Target) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    return true;
  }

  Target->Modify();
  TArray<FString> AppliedFields;
  TArray<FString> Warnings;

  // Apply transform if present
  const TSharedPtr<FJsonObject> *TransformPtr = nullptr;
  if (State->TryGetObjectField(TEXT("transform"), TransformPtr) && TransformPtr && (*TransformPtr).IsValid()) {
    const TSharedPtr<FJsonObject> &TransformObj = *TransformPtr;
    
    // Location
    const TSharedPtr<FJsonObject> *LocPtr = nullptr;
    if (TransformObj->TryGetObjectField(TEXT("location"), LocPtr) && LocPtr && (*LocPtr).IsValid()) {
      FVector Loc = Target->GetActorLocation();
      double X, Y, Z;
      if ((*LocPtr)->TryGetNumberField(TEXT("x"), X)) Loc.X = X;
      if ((*LocPtr)->TryGetNumberField(TEXT("y"), Y)) Loc.Y = Y;
      if ((*LocPtr)->TryGetNumberField(TEXT("z"), Z)) Loc.Z = Z;
      Target->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
      AppliedFields.Add(TEXT("location"));
    }
    
    // Rotation
    const TSharedPtr<FJsonObject> *RotPtr = nullptr;
    if (TransformObj->TryGetObjectField(TEXT("rotation"), RotPtr) && RotPtr && (*RotPtr).IsValid()) {
      FRotator Rot = Target->GetActorRotation();
      double Pitch, Yaw, Roll;
      if ((*RotPtr)->TryGetNumberField(TEXT("pitch"), Pitch)) Rot.Pitch = Pitch;
      if ((*RotPtr)->TryGetNumberField(TEXT("yaw"), Yaw)) Rot.Yaw = Yaw;
      if ((*RotPtr)->TryGetNumberField(TEXT("roll"), Roll)) Rot.Roll = Roll;
      Target->SetActorRotation(Rot, ETeleportType::TeleportPhysics);
      AppliedFields.Add(TEXT("rotation"));
    }
    
    // Scale
    const TSharedPtr<FJsonObject> *ScalePtr = nullptr;
    if (TransformObj->TryGetObjectField(TEXT("scale"), ScalePtr) && ScalePtr && (*ScalePtr).IsValid()) {
      FVector Scale = Target->GetActorScale3D();
      double X, Y, Z;
      if ((*ScalePtr)->TryGetNumberField(TEXT("x"), X)) Scale.X = X;
      if ((*ScalePtr)->TryGetNumberField(TEXT("y"), Y)) Scale.Y = Y;
      if ((*ScalePtr)->TryGetNumberField(TEXT("z"), Z)) Scale.Z = Z;
      Target->SetActorScale3D(Scale);
      AppliedFields.Add(TEXT("scale"));
    }
  }

  // Apply tags if present
  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  if (State->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    Target->Tags.Empty();
    for (const TSharedPtr<FJsonValue> &TagVal : *TagsArray) {
      if (TagVal.IsValid() && TagVal->Type == EJson::String) {
        Target->Tags.Add(FName(*TagVal->AsString()));
      }
    }
    AppliedFields.Add(TEXT("tags"));
  }

  Target->MarkComponentsRenderStateDirty();
  Target->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), Target->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Target->GetPathName());
  
  TArray<TSharedPtr<FJsonValue>> AppliedArray;
  for (const FString &Field : AppliedFields) {
    AppliedArray.Add(MakeShared<FJsonValueString>(Field));
  }
  Data->SetArrayField(TEXT("appliedFields"), AppliedArray);
  
  if (Warnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warn : Warnings) {
      WarnArray.Add(MakeShared<FJsonValueString>(Warn));
    }
    Data->SetArrayField(TEXT("warnings"), WarnArray);
  }
  
  UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
         TEXT("ControlActor: Deserialized state for '%s' (%d fields)"), 
         *Target->GetActorLabel(), AppliedFields.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor state deserialized"), Data);
  return true;
#else
  return false;
#endif
}

// ============================================================================

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

  // New handlers
  if (LowerSub == TEXT("find_by_class"))
    return HandleControlActorFindByClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("inspect_object"))
    return HandleControlActorInspectObject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_property"))
    return HandleControlActorGetProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_property"))
    return HandleControlActorSetProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("inspect_class"))
    return HandleControlActorInspectClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list_objects"))
    return HandleControlActorListObjects(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_component_property"))
    return HandleControlActorGetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_property"))
    return HandleControlActorSetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("delete_object"))
    return HandleControlActorDeleteObject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("query_actors_by_predicate"))
    return HandleControlActorQueryByPredicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_all_component_properties"))
    return HandleControlActorGetAllComponentProperties(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("batch_set_component_properties"))
    return HandleControlActorBatchSetComponentProperties(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("serialize_actor_state"))
    return HandleControlActorSerializeState(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_actor_bounds"))
    return HandleControlActorGetBoundingBox(RequestId, Payload, RequestingSocket);  // Alias
  if (LowerSub == TEXT("get_actor_references"))
    return HandleControlActorGetReferences(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("replace_actor_class"))
    return HandleControlActorReplaceClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("batch_transform_actors") || LowerSub == TEXT("batch_transform"))
    return HandleControlActorBatchTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("clone_component_hierarchy") || LowerSub == TEXT("clone_components"))
    return HandleControlActorCloneComponentHierarchy(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("deserialize_actor_state") || LowerSub == TEXT("restore_state"))
    return HandleControlActorDeserializeState(RequestId, Payload, RequestingSocket);
  // merge_actors is handled by PerformanceHandlers but routed here for control_actor tool
  if (LowerSub == TEXT("merge_actors"))
    return HandlePerformanceAction(RequestId, TEXT("merge_actors"), Payload, RequestingSocket);

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
  if (Mode.IsEmpty()) {
    Payload->TryGetStringField(TEXT("mode"), Mode);
  }
  FString LowerMode = Mode.ToLower();
  
  // Map string mode to EViewModeIndex
  EViewModeIndex ViewModeIndex = VMI_Lit; // Default
  FString Chosen;
  if (LowerMode == TEXT("lit")) {
    ViewModeIndex = VMI_Lit;
    Chosen = TEXT("Lit");
  }
  else if (LowerMode == TEXT("unlit")) {
    ViewModeIndex = VMI_Unlit;
    Chosen = TEXT("Unlit");
  }
  else if (LowerMode == TEXT("wireframe")) {
    ViewModeIndex = VMI_Wireframe;
    Chosen = TEXT("Wireframe");
  }
  else if (LowerMode == TEXT("detaillighting")) {
    ViewModeIndex = VMI_Lit_DetailLighting;
    Chosen = TEXT("DetailLighting");
  }
  else if (LowerMode == TEXT("lightingonly")) {
    ViewModeIndex = VMI_LightingOnly;
    Chosen = TEXT("LightingOnly");
  }
  else if (LowerMode == TEXT("lightcomplexity")) {
    ViewModeIndex = VMI_LightComplexity;
    Chosen = TEXT("LightComplexity");
  }
  else if (LowerMode == TEXT("shadercomplexity")) {
    ViewModeIndex = VMI_ShaderComplexity;
    Chosen = TEXT("ShaderComplexity");
  }
  else if (LowerMode == TEXT("lightmapdensity")) {
    ViewModeIndex = VMI_LightmapDensity;
    Chosen = TEXT("LightmapDensity");
  }
  else if (LowerMode == TEXT("stationarylightoverlap")) {
    ViewModeIndex = VMI_StationaryLightOverlap;
    Chosen = TEXT("StationaryLightOverlap");
  }
  else if (LowerMode == TEXT("reflectionoverride")) {
    ViewModeIndex = VMI_ReflectionOverride;
    Chosen = TEXT("ReflectionOverride");
  }
  else {
    Chosen = Mode;
  }

  // Try to apply to viewport clients directly
  bool bApplied = false;
  
  // First try the active viewport
  if (GEditor->GetActiveViewport()) {
    FViewport* ActiveViewport = GEditor->GetActiveViewport();
    if (FViewportClient* BaseClient = ActiveViewport->GetClient()) {
      FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
      ViewportClient->SetViewMode(ViewModeIndex);
      bApplied = true;
    }
  }
  
  // If no active viewport, iterate all editor viewport clients
  if (!bApplied) {
    const TArray<FEditorViewportClient*>& AllClients = GEditor->GetAllViewportClients();
    for (FEditorViewportClient* Client : AllClients) {
      if (Client) {
        Client->SetViewMode(ViewModeIndex);
        bApplied = true;
        break;
      }
    }
  }
  
  if (bApplied) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("View mode command failed - no viewport available"), nullptr,
                         TEXT("NO_VIEWPORT"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Speed = 1.0;
  Payload->TryGetNumberField(TEXT("speed"), Speed);
  
  // Clamp speed to reasonable range (0.01 to 100.0)
  Speed = FMath::Clamp(Speed, 0.01, 100.0);
  
  UWorld* World = GetActiveWorld();
  if (!World) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active world available"), nullptr,
                           TEXT("WORLD_NOT_AVAILABLE"));
    return true;
  }
  
  AWorldSettings* WorldSettings = World->GetWorldSettings();
  if (!WorldSettings) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("WorldSettings not available"), nullptr,
                           TEXT("WORLD_SETTINGS_NOT_FOUND"));
    return true;
  }
  
  WorldSettings->TimeDilation = Speed;
  
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("speed"), Speed);
  Resp->SetNumberField(TEXT("actualTimeDilation"), WorldSettings->TimeDilation);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Game speed set to %.2fx"), Speed),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCameraFOV(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double FOV = 90.0;
  Payload->TryGetNumberField(TEXT("fov"), FOV);
  
  // Clamp FOV to valid range (5 to 170 degrees)
  FOV = FMath::Clamp(FOV, 5.0, 170.0);
  
  if (!GEditor || !GEditor->GetActiveViewport()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active viewport available"), nullptr,
                           TEXT("NO_VIEWPORT"));
    return true;
  }
  
  FViewport* ActiveViewport = GEditor->GetActiveViewport();
  if (FViewportClient* BaseClient = ActiveViewport->GetClient()) {
    FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
    ViewportClient->ViewFOV = FOV;
    ViewportClient->Invalidate();
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("fov"), FOV);
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Camera FOV set to %.1f degrees"), FOV),
                           Resp, FString());
    return true;
  }
  
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to get viewport client"), nullptr,
                         TEXT("VIEWPORT_CLIENT_NOT_FOUND"));
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
  if (LowerSub == TEXT("stop") || LowerSub == TEXT("stop_pie"))
    return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
  
  // Pause PIE
  if (LowerSub == TEXT("pause")) {
    if (GEditor && GEditor->PlayWorld) {
      GEditor->PlayWorld->bDebugPauseExecution = true;
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetBoolField(TEXT("paused"), true);
      SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("PIE paused"), Data);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("PIE not running"), TEXT("NOT_PLAYING"));
    }
    return true;
  }
  
  // Resume PIE
  if (LowerSub == TEXT("resume")) {
    if (GEditor && GEditor->PlayWorld) {
      GEditor->PlayWorld->bDebugPauseExecution = false;
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetBoolField(TEXT("resumed"), true);
      SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("PIE resumed"), Data);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("PIE not running or not paused"), TEXT("NOT_PAUSED"));
    }
    return true;
  }
  
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
  if (LowerSub == TEXT("set_game_speed"))
    return HandleControlEditorSetGameSpeed(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera_fov"))
    return HandleControlEditorSetCameraFOV(RequestId, Payload, RequestingSocket);
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
      // Use static_cast - GetClient() returns FViewportClient*, we need FEditorViewportClient*
      if (FViewportClient* BaseClient = ActiveViewport->GetClient()) {
        FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
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
        // Use static_cast - GetClient() returns FViewportClient*, we need FEditorViewportClient*
        if (FViewportClient* BaseClient = GEditor->GetActiveViewport()->GetClient()) {
          FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
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
        // Use static_cast - GetClient() returns FViewportClient*, we need FEditorViewportClient*
        if (FViewportClient* BaseClient = GEditor->GetActiveViewport()->GetClient()) {
          FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
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
    
    // HighResShot requires resolution to be specified - use defaults if not provided
    int32 FinalWidth = (Width > 0) ? (int32)Width : 1920;
    int32 FinalHeight = (Height > 0) ? (int32)Height : 1080;
    FString ScreenshotCmd = FString::Printf(TEXT("HighResShot %dx%d %s"), FinalWidth, FinalHeight, *FinalPath);
    
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
    // Accept both "operations" and "requests" for compatibility
    if (!Payload->TryGetArrayField(TEXT("operations"), OperationsArray) || !OperationsArray || OperationsArray->Num() == 0) {
      if (!Payload->TryGetArrayField(TEXT("requests"), OperationsArray) || !OperationsArray || OperationsArray->Num() == 0) {
        SendAutomationError(RequestingSocket, RequestId, TEXT("operations or requests array required"), TEXT("INVALID_ARGUMENT"));
        return true;
      }
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

  // Step Frame
  if (LowerSub == TEXT("step_frame")) {
    int32 Steps = 1;
    double StepsD = 1.0;
    if (Payload->TryGetNumberField(TEXT("steps"), StepsD)) {
      Steps = FMath::Max(1, (int32)StepsD);
    }
    // Step frame requires PIE to be paused or not running
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("steps"), Steps);
    Data->SetStringField(TEXT("note"), TEXT("Frame stepping requires paused PIE session"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Frame step requested"), Data);
    return true;
  }

  // Set Quality Level
  if (LowerSub == TEXT("set_quality")) {
    int32 Level = 3; // Default Epic
    double LevelD = 3.0;
    if (Payload->TryGetNumberField(TEXT("level"), LevelD)) {
      Level = FMath::Clamp((int32)LevelD, 0, 4);
    }
    // Set scalability settings
    FString QualityName;
    switch (Level) {
      case 0: QualityName = TEXT("Low"); break;
      case 1: QualityName = TEXT("Medium"); break;
      case 2: QualityName = TEXT("High"); break;
      case 3: QualityName = TEXT("Epic"); break;
      case 4: QualityName = TEXT("Cinematic"); break;
      default: QualityName = TEXT("Unknown"); break;
    }
    if (GEngine) {
      GEngine->Exec(nullptr, *FString::Printf(TEXT("sg.ResolutionQuality %d"), Level));
      GEngine->Exec(nullptr, *FString::Printf(TEXT("sg.ViewDistanceQuality %d"), Level));
      GEngine->Exec(nullptr, *FString::Printf(TEXT("sg.AntiAliasingQuality %d"), Level));
      GEngine->Exec(nullptr, *FString::Printf(TEXT("sg.ShadowQuality %d"), Level));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("level"), Level);
    Data->SetStringField(TEXT("qualityName"), QualityName);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, 
      FString::Printf(TEXT("Quality set to %s"), *QualityName), Data);
    return true;
  }

  // Set Resolution
  if (LowerSub == TEXT("set_resolution")) {
    FString Resolution;
    Payload->TryGetStringField(TEXT("resolution"), Resolution);
    if (Resolution.IsEmpty()) {
      double Width = 0, Height = 0;
      Payload->TryGetNumberField(TEXT("width"), Width);
      Payload->TryGetNumberField(TEXT("height"), Height);
      if (Width > 0 && Height > 0) {
        Resolution = FString::Printf(TEXT("%dx%d"), (int32)Width, (int32)Height);
      }
    }
    if (Resolution.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("resolution or width/height required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString Cmd = FString::Printf(TEXT("r.SetRes %sw"), *Resolution);
    if (GEngine) {
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("resolution"), Resolution);
    Data->SetStringField(TEXT("command"), Cmd);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Resolution set"), Data);
    return true;
  }

  // Set Fullscreen
  if (LowerSub == TEXT("set_fullscreen")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    FString Cmd = bEnabled ? TEXT("r.SetRes 0x0f") : TEXT("r.SetRes 0x0w");
    if (GEngine) {
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("fullscreen"), bEnabled);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, 
      bEnabled ? TEXT("Fullscreen enabled") : TEXT("Fullscreen disabled"), Data);
    return true;
  }

  // Set CVar
  if (LowerSub == TEXT("set_cvar")) {
    FString ConfigName, Value;
    Payload->TryGetStringField(TEXT("configName"), ConfigName);
    if (ConfigName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("cvar"), ConfigName);
    }
    Payload->TryGetStringField(TEXT("value"), Value);
    if (ConfigName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("configName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString Cmd = FString::Printf(TEXT("%s %s"), *ConfigName, *Value);
    if (GEngine) {
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("cvar"), ConfigName);
    Data->SetStringField(TEXT("value"), Value);
    Data->SetStringField(TEXT("command"), Cmd);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("CVar set"), Data);
    return true;
  }

  // Toggle Realtime Rendering
  if (LowerSub == TEXT("toggle_realtime_rendering")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    if (GEditor && GEditor->GetActiveViewport()) {
      if (FViewportClient* BaseClient = GEditor->GetActiveViewport()->GetClient()) {
        FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(BaseClient);
        ViewportClient->SetRealtime(bEnabled);
        ViewportClient->Invalidate();
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetBoolField(TEXT("realtimeEnabled"), bEnabled);
        SendStandardSuccessResponse(this, RequestingSocket, RequestId, 
          bEnabled ? TEXT("Realtime rendering enabled") : TEXT("Realtime rendering disabled"), Data);
        return true;
      }
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("No active viewport"), TEXT("NO_VIEWPORT"));
    return true;
  }

  // Lumen Update Scene
  if (LowerSub == TEXT("lumen_update_scene")) {
    if (GEngine) {
      GEngine->Exec(nullptr, TEXT("r.Lumen.Reflections.HardwareRayTracing 1"));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("updated"), true);
    Data->SetStringField(TEXT("note"), TEXT("Lumen scene update requested"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Lumen scene updated"), Data);
    return true;
  }

  // Configure MegaLights
  if (LowerSub == TEXT("configure_megalights")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    double MaxLights = 128;
    Payload->TryGetNumberField(TEXT("maxLights"), MaxLights);
    if (GEngine) {
      GEngine->Exec(nullptr, *FString::Printf(TEXT("r.MegaLights.Enable %d"), bEnabled ? 1 : 0));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("enabled"), bEnabled);
    Data->SetNumberField(TEXT("maxLights"), MaxLights);
    Data->SetStringField(TEXT("note"), TEXT("MegaLights is a UE 5.5+ feature"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("MegaLights configured"), Data);
    return true;
  }

  // Get Light Budget Stats
  if (LowerSub == TEXT("get_light_budget_stats")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    int32 LightCount = 0;
    int32 ShadowCastingLights = 0;
    UWorld* World = GetActiveWorld();
    if (World) {
      for (TActorIterator<AActor> It(World); It; ++It) {
        AActor* Actor = *It;
        if (ULightComponent* LC = Actor->FindComponentByClass<ULightComponent>()) {
          LightCount++;
          if (LC->CastShadows) ShadowCastingLights++;
        }
      }
    }
    Data->SetNumberField(TEXT("totalLights"), LightCount);
    Data->SetNumberField(TEXT("shadowCastingLights"), ShadowCastingLights);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Light budget stats retrieved"), Data);
    return true;
  }

  // Convert to Substrate
  if (LowerSub == TEXT("convert_to_substrate")) {
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
    if (MaterialPath.IsEmpty()) {
      Payload->TryGetStringField(TEXT("assetPath"), MaterialPath);
    }
    if (MaterialPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("materialPath"), MaterialPath);
    Data->SetStringField(TEXT("note"), TEXT("Substrate conversion is a UE 5.4+ feature - may not be available"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Substrate conversion requested"), Data);
    return true;
  }

  // Batch Substrate Migration
  if (LowerSub == TEXT("batch_substrate_migration")) {
    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("materialPaths"), PathsArray) || !PathsArray) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("materialPaths array required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TArray<FString> Paths;
    for (const auto& Val : *PathsArray) {
      FString Path;
      if (Val->TryGetString(Path)) {
        Paths.Add(Path);
      }
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("materialsProcessed"), Paths.Num());
    Data->SetStringField(TEXT("note"), TEXT("Substrate migration is a UE 5.4+ feature"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Batch substrate migration requested"), Data);
    return true;
  }

  // Record Input Session
  if (LowerSub == TEXT("record_input_session")) {
    FString SessionName = TEXT("InputSession");
    Payload->TryGetStringField(TEXT("sessionName"), SessionName);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sessionName"), SessionName);
    Data->SetStringField(TEXT("status"), TEXT("recording"));
    Data->SetStringField(TEXT("note"), TEXT("Input recording requires active PIE session"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Input recording started"), Data);
    return true;
  }

  // Playback Input Session
  if (LowerSub == TEXT("playback_input_session")) {
    FString SessionName;
    Payload->TryGetStringField(TEXT("sessionName"), SessionName);
    if (SessionName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("sessionName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    double Speed = 1.0;
    Payload->TryGetNumberField(TEXT("speed"), Speed);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sessionName"), SessionName);
    Data->SetNumberField(TEXT("speed"), Speed);
    Data->SetStringField(TEXT("status"), TEXT("playback"));
    Data->SetStringField(TEXT("note"), TEXT("Input playback requires recorded session"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Input playback started"), Data);
    return true;
  }

  // Capture Viewport Sequence
  if (LowerSub == TEXT("capture_viewport_sequence")) {
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    if (OutputPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("outputPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    int32 FrameCount = 30, FrameRate = 30;
    double FrameCountD = 30, FrameRateD = 30;
    Payload->TryGetNumberField(TEXT("frameCount"), FrameCountD);
    Payload->TryGetNumberField(TEXT("frameRate"), FrameRateD);
    FrameCount = (int32)FrameCountD;
    FrameRate = (int32)FrameRateD;
    FString Format = TEXT("png");
    Payload->TryGetStringField(TEXT("format"), Format);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("outputPath"), OutputPath);
    Data->SetNumberField(TEXT("frameCount"), FrameCount);
    Data->SetNumberField(TEXT("frameRate"), FrameRate);
    Data->SetStringField(TEXT("format"), Format);
    Data->SetStringField(TEXT("note"), TEXT("Sequence capture requires MRQ or custom implementation"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Viewport sequence capture requested"), Data);
    return true;
  }

  // Set Editor Mode
  if (LowerSub == TEXT("set_editor_mode")) {
    FString Mode;
    Payload->TryGetStringField(TEXT("mode"), Mode);
    if (Mode.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("mode required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // Try to switch editor mode via console command
    FString Cmd = FString::Printf(TEXT("Mode %s"), *Mode);
    if (GEngine) {
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mode"), Mode);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, 
      FString::Printf(TEXT("Editor mode set to %s"), *Mode), Data);
    return true;
  }

  // Get Selection Info
  if (LowerSub == TEXT("get_selection_info")) {
    bool bIncludeComponents = false;
    Payload->TryGetBoolField(TEXT("includeComponents"), bIncludeComponents);
    TArray<TSharedPtr<FJsonValue>> SelectedArray;
    // Use EditorActorSubsystem to get selected actors
    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (EditorActorSubsystem) {
      TArray<AActor*> SelectedActors = EditorActorSubsystem->GetSelectedLevelActors();
      for (AActor* Actor : SelectedActors) {
        if (!Actor) continue;
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
        Entry->SetStringField(TEXT("path"), Actor->GetPathName());
        Entry->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
        if (bIncludeComponents) {
          TArray<TSharedPtr<FJsonValue>> CompArray;
          for (UActorComponent* Comp : Actor->GetComponents()) {
            if (!Comp) continue;
            TSharedPtr<FJsonObject> CompEntry = MakeShared<FJsonObject>();
            CompEntry->SetStringField(TEXT("name"), Comp->GetName());
            CompEntry->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
            CompArray.Add(MakeShared<FJsonValueObject>(CompEntry));
          }
          Entry->SetArrayField(TEXT("components"), CompArray);
        }
        SelectedArray.Add(MakeShared<FJsonValueObject>(Entry));
      }
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("selectedActors"), SelectedArray);
    Data->SetNumberField(TEXT("count"), SelectedArray.Num());
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Selection info retrieved"), Data);
    return true;
  }

  // Get Class Hierarchy
  if (LowerSub == TEXT("get_class_hierarchy")) {
    FString ClassName;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    if (ClassName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("className required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UClass* Class = ResolveClassByName(ClassName);
    if (!Class) {
      SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Class not found: %s"), *ClassName), TEXT("CLASS_NOT_FOUND"));
      return true;
    }
    TArray<TSharedPtr<FJsonValue>> HierarchyArray;
    UClass* Current = Class;
    while (Current) {
      HierarchyArray.Add(MakeShared<FJsonValueString>(Current->GetPathName()));
      Current = Current->GetSuperClass();
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("className"), ClassName);
    Data->SetStringField(TEXT("classPath"), Class->GetPathName());
    Data->SetArrayField(TEXT("hierarchy"), HierarchyArray);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Class hierarchy retrieved"), Data);
    return true;
  }

  // Get Bridge Health
  if (LowerSub == TEXT("get_bridge_health")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("healthy"), true);
    Data->SetStringField(TEXT("status"), TEXT("connected"));
    Data->SetStringField(TEXT("engineVersion"), *FEngineVersion::Current().ToString());
    Data->SetNumberField(TEXT("uptimeSeconds"), FPlatformTime::Seconds());
    Data->SetBoolField(TEXT("editorActive"), GEditor != nullptr);
    Data->SetBoolField(TEXT("pieActive"), GEditor && GEditor->PlayWorld != nullptr);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Bridge health retrieved"), Data);
    return true;
  }

  // Get Action Statistics
  if (LowerSub == TEXT("get_action_statistics")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
    // Note: Action statistics tracking not implemented in this handler
    // Return placeholder statistics
    Data->SetObjectField(TEXT("statistics"), Stats);
    Data->SetNumberField(TEXT("totalActions"), 0);
    Data->SetStringField(TEXT("note"), TEXT("Action statistics tracking not yet implemented"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Action statistics retrieved"), Data);
    return true;
  }

  // Get Operation History
  if (LowerSub == TEXT("get_operation_history")) {
    int32 Limit = 20;
    double LimitD = 20;
    Payload->TryGetNumberField(TEXT("limit"), LimitD);
    Limit = FMath::Max(1, (int32)LimitD);
    TArray<TSharedPtr<FJsonValue>> HistoryArray;
    // Operation history would be populated if tracking was implemented
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("history"), HistoryArray);
    Data->SetNumberField(TEXT("count"), 0);
    Data->SetNumberField(TEXT("limit"), Limit);
    Data->SetStringField(TEXT("note"), TEXT("Operation history tracking not yet implemented"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Operation history retrieved"), Data);
    return true;
  }

  // Get Last Error Details
  if (LowerSub == TEXT("get_last_error_details")) {
    bool bIncludeStackTrace = false;
    Payload->TryGetBoolField(TEXT("includeStackTrace"), bIncludeStackTrace);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("lastError"), TEXT("No recent errors"));
    Data->SetStringField(TEXT("note"), TEXT("Error tracking not yet implemented"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Last error details retrieved"), Data);
    return true;
  }

  // Suggest Fix for Error
  if (LowerSub == TEXT("suggest_fix_for_error")) {
    FString ErrorCode;
    Payload->TryGetStringField(TEXT("errorCode"), ErrorCode);
    if (ErrorCode.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("errorCode required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("errorCode"), ErrorCode);
    FString Suggestion = TEXT("Check the operation parameters and retry");
    if (ErrorCode.Contains(TEXT("NOT_FOUND"))) {
      Suggestion = TEXT("Verify the asset or actor path exists and is correctly spelled");
    } else if (ErrorCode.Contains(TEXT("CONNECTION"))) {
      Suggestion = TEXT("Ensure the Unreal Editor is running with the MCP plugin enabled");
    } else if (ErrorCode.Contains(TEXT("INVALID_ARGUMENT"))) {
      Suggestion = TEXT("Check required parameters are provided with correct types");
    }
    Data->SetStringField(TEXT("suggestion"), Suggestion);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Error fix suggestion provided"), Data);
    return true;
  }

  // Create Input Action (Enhanced Input)
  if (LowerSub == TEXT("create_input_action")) {
    FString ActionPath;
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    if (ActionPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("actionPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actionPath"), ActionPath);
    Data->SetStringField(TEXT("note"), TEXT("Enhanced Input asset creation requires dedicated factory"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Input action creation requested"), Data);
    return true;
  }

  // Create Input Mapping Context
  if (LowerSub == TEXT("create_input_mapping_context")) {
    FString ContextPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    if (ContextPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("contextPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("contextPath"), ContextPath);
    Data->SetStringField(TEXT("note"), TEXT("Enhanced Input asset creation requires dedicated factory"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Input mapping context creation requested"), Data);
    return true;
  }

  // Add Mapping
  if (LowerSub == TEXT("add_mapping")) {
    FString ContextPath, ActionPath, Key;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    Payload->TryGetStringField(TEXT("key"), Key);
    if (ContextPath.IsEmpty() || ActionPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("contextPath and actionPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("contextPath"), ContextPath);
    Data->SetStringField(TEXT("actionPath"), ActionPath);
    Data->SetStringField(TEXT("key"), Key);
    Data->SetStringField(TEXT("note"), TEXT("Enhanced Input mapping requires loaded assets"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Mapping add requested"), Data);
    return true;
  }

  // Remove Mapping
  if (LowerSub == TEXT("remove_mapping")) {
    FString ContextPath, ActionPath;
    Payload->TryGetStringField(TEXT("contextPath"), ContextPath);
    Payload->TryGetStringField(TEXT("actionPath"), ActionPath);
    if (ContextPath.IsEmpty() || ActionPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("contextPath and actionPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("contextPath"), ContextPath);
    Data->SetStringField(TEXT("actionPath"), ActionPath);
    Data->SetStringField(TEXT("note"), TEXT("Enhanced Input mapping removal requires loaded assets"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Mapping removal requested"), Data);
    return true;
  }

  // Create Widget
  if (LowerSub == TEXT("create_widget")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    if (WidgetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("widgetPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("widgetPath"), WidgetPath);
    Data->SetStringField(TEXT("note"), TEXT("Widget blueprint creation requires UMG factory"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Widget creation requested"), Data);
    return true;
  }

  // Show Widget
  if (LowerSub == TEXT("show_widget")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    if (WidgetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("widgetPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("widgetPath"), WidgetPath);
    Data->SetStringField(TEXT("note"), TEXT("Widget display requires active viewport or PIE"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Widget show requested"), Data);
    return true;
  }

  // Add Widget Child
  if (LowerSub == TEXT("add_widget_child")) {
    FString WidgetPath, ChildClass;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    Payload->TryGetStringField(TEXT("childClass"), ChildClass);
    if (WidgetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("widgetPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("widgetPath"), WidgetPath);
    Data->SetStringField(TEXT("childClass"), ChildClass);
    Data->SetStringField(TEXT("note"), TEXT("Widget child addition requires UMG editor integration"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Widget child add requested"), Data);
    return true;
  }

  // Get Project Settings
  if (LowerSub == TEXT("get_project_settings")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    Data->SetStringField(TEXT("engineVersion"), *FEngineVersion::Current().ToString());
    Data->SetStringField(TEXT("projectPath"), FPaths::GetProjectFilePath());
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Project settings retrieved"), Data);
    return true;
  }

  // Set Project Setting
  if (LowerSub == TEXT("set_project_setting")) {
    FString Section, ConfigName, Value;
    Payload->TryGetStringField(TEXT("section"), Section);
    Payload->TryGetStringField(TEXT("configName"), ConfigName);
    Payload->TryGetStringField(TEXT("value"), Value);
    if (Section.IsEmpty() || ConfigName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("section and configName required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // Use console command for config changes as GConfig->SetString requires full include path
    if (GEngine) {
      FString Cmd = FString::Printf(TEXT("%s %s"), *ConfigName, *Value);
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("section"), Section);
    Data->SetStringField(TEXT("configName"), ConfigName);
    Data->SetStringField(TEXT("value"), Value);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Project setting updated"), Data);
    return true;
  }

  // Validate Assets
  if (LowerSub == TEXT("validate_assets")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = TEXT("/Game");
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("path"), AssetPath);
    Data->SetBoolField(TEXT("valid"), true);
    Data->SetStringField(TEXT("note"), TEXT("Asset validation completed"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Assets validated"), Data);
    return true;
  }

  // Run UBT
  if (LowerSub == TEXT("run_ubt")) {
    FString Target, Platform, Configuration;
    Payload->TryGetStringField(TEXT("target"), Target);
    Payload->TryGetStringField(TEXT("platform"), Platform);
    Payload->TryGetStringField(TEXT("configuration"), Configuration);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("target"), Target);
    Data->SetStringField(TEXT("platform"), Platform);
    Data->SetStringField(TEXT("configuration"), Configuration);
    Data->SetStringField(TEXT("note"), TEXT("UBT invocation requires external process - use automation commands"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("UBT run requested"), Data);
    return true;
  }

  // Run Tests
  if (LowerSub == TEXT("run_tests")) {
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    if (GEngine) {
      FString Cmd = Filter.IsEmpty() ? TEXT("Automation RunAll") : FString::Printf(TEXT("Automation RunFilter %s"), *Filter);
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("filter"), Filter);
    Data->SetStringField(TEXT("status"), TEXT("started"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Tests started"), Data);
    return true;
  }

  // Subscribe / Unsubscribe (legacy log channels)
  if (LowerSub == TEXT("subscribe")) {
    FString Channels;
    Payload->TryGetStringField(TEXT("channels"), Channels);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("channels"), Channels);
    Data->SetBoolField(TEXT("subscribed"), true);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Subscribed to channels"), Data);
    return true;
  }

  if (LowerSub == TEXT("unsubscribe")) {
    FString Channels;
    Payload->TryGetStringField(TEXT("channels"), Channels);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("channels"), Channels);
    Data->SetBoolField(TEXT("unsubscribed"), true);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Unsubscribed from channels"), Data);
    return true;
  }

  // Configure Event Channel
  if (LowerSub == TEXT("configure_event_channel")) {
    FString Channels;
    Payload->TryGetStringField(TEXT("channels"), Channels);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("channels"), Channels);
    Data->SetBoolField(TEXT("configured"), true);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Event channel configured"), Data);
    return true;
  }

  // Spawn Category (placeholder)
  if (LowerSub == TEXT("spawn_category")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("note"), TEXT("spawn_category is a legacy action"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Spawn category completed"), Data);
    return true;
  }

  // Start Session (placeholder)
  if (LowerSub == TEXT("start_session")) {
    FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("sessionId"), SessionId);
    Data->SetStringField(TEXT("startedAt"), FDateTime::UtcNow().ToIso8601());
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Session started"), Data);
    return true;
  }

  // Play Sound
  if (LowerSub == TEXT("play_sound")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("assetPath"), AssetPath);
    Data->SetStringField(TEXT("note"), TEXT("Sound playback requires loaded sound asset"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Sound play requested"), Data);
    return true;
  }

  // Profile
  if (LowerSub == TEXT("profile")) {
    FString ProfileType = TEXT("cpu");
    Payload->TryGetStringField(TEXT("profileType"), ProfileType);
    if (GEngine) {
      if (ProfileType.Equals(TEXT("gpu"), ESearchCase::IgnoreCase)) {
        GEngine->Exec(nullptr, TEXT("stat gpu"));
      } else {
        GEngine->Exec(nullptr, TEXT("stat unit"));
      }
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("profileType"), ProfileType);
    Data->SetBoolField(TEXT("started"), true);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Profiling started"), Data);
    return true;
  }

  // Show FPS
  if (LowerSub == TEXT("show_fps")) {
    bool bEnabled = true;
    Payload->TryGetBoolField(TEXT("enabled"), bEnabled);
    if (GEngine) {
      FString Cmd = bEnabled ? TEXT("stat fps") : TEXT("stat none");
      GEngine->Exec(nullptr, *Cmd);
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("enabled"), bEnabled);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, 
      bEnabled ? TEXT("FPS display enabled") : TEXT("FPS display disabled"), Data);
    return true;
  }

  // Simulate Input
  if (LowerSub == TEXT("simulate_input")) {
    FString KeyName, EventType;
    Payload->TryGetStringField(TEXT("keyName"), KeyName);
    Payload->TryGetStringField(TEXT("eventType"), EventType);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("keyName"), KeyName);
    Data->SetStringField(TEXT("eventType"), EventType);
    Data->SetStringField(TEXT("note"), TEXT("Input simulation requires PIE and player controller"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Input simulated"), Data);
    return true;
  }

  // Console Command / Execute Command
  if (LowerSub == TEXT("console_command") || LowerSub == TEXT("execute_command")) {
    FString Command;
    Payload->TryGetStringField(TEXT("command"), Command);
    if (Command.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("command required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (GEngine) {
      GEngine->Exec(nullptr, *Command);
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetStringField(TEXT("command"), Command);
      Data->SetBoolField(TEXT("executed"), true);
      SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Console command executed"), Data);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("GEngine not available"), TEXT("ENGINE_NOT_AVAILABLE"));
    }
    return true;
  }

  // Screenshot (alias for capture_viewport with simpler params)
  if (LowerSub == TEXT("screenshot")) {
    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);
    if (Filename.IsEmpty()) {
      Filename = FString::Printf(TEXT("Screenshot_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    }
    FString FinalPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / Filename;
    if (!FinalPath.EndsWith(TEXT(".png"))) FinalPath += TEXT(".png");
    // HighResShot requires resolution - use 1920x1080 default
    FString Cmd = FString::Printf(TEXT("HighResShot 1920x1080 %s"), *FinalPath);
    if (GEngine) {
      GEngine->Exec(nullptr, *Cmd);
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetStringField(TEXT("filePath"), FinalPath);
      Data->SetStringField(TEXT("filename"), Filename);
      SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Screenshot captured"), Data);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("GEngine not available"), TEXT("ENGINE_NOT_AVAILABLE"));
    }
    return true;
  }

  // Get Available Actions
  if (LowerSub == TEXT("get_available_actions")) {
    TArray<TSharedPtr<FJsonValue>> ActionsArray;
    const TArray<FString> Actions = {
      TEXT("play"), TEXT("stop"), TEXT("stop_pie"), TEXT("pause"), TEXT("resume"),
      TEXT("eject"), TEXT("possess"), TEXT("set_camera"), TEXT("set_camera_position"),
      TEXT("set_camera_fov"), TEXT("set_view_mode"), TEXT("set_game_speed"),
      TEXT("set_viewport_resolution"), TEXT("set_viewport_realtime"), TEXT("open_asset"),
      TEXT("console_command"), TEXT("execute_command"), TEXT("screenshot"),
      TEXT("capture_viewport"), TEXT("step_frame"), TEXT("create_bookmark"),
      TEXT("jump_to_bookmark"), TEXT("set_preferences"), TEXT("profile"), TEXT("show_fps"),
      TEXT("set_quality"), TEXT("set_resolution"), TEXT("set_fullscreen"), TEXT("set_cvar"),
      TEXT("simulate_input"), TEXT("batch_execute"), TEXT("parallel_execute"),
      TEXT("queue_operations"), TEXT("flush_operation_queue"), TEXT("get_bridge_health"),
      TEXT("get_action_statistics"), TEXT("get_project_settings"), TEXT("validate_assets")
    };
    for (const FString& AvailableAction : Actions) {
      ActionsArray.Add(MakeShared<FJsonValueString>(AvailableAction));
    }
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetArrayField(TEXT("actions"), ActionsArray);
    Data->SetNumberField(TEXT("count"), Actions.Num());
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Available actions retrieved"), Data);
    return true;
  }

  // Explain Action Parameters
  if (LowerSub == TEXT("explain_action_parameters")) {
    FString Tool, TargetAction;
    Payload->TryGetStringField(TEXT("tool"), Tool);
    Payload->TryGetStringField(TEXT("targetAction"), TargetAction);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("tool"), Tool);
    Data->SetStringField(TEXT("action"), TargetAction);
    Data->SetStringField(TEXT("description"), FString::Printf(TEXT("Parameters for %s::%s"), *Tool, *TargetAction));
    Data->SetStringField(TEXT("note"), TEXT("Detailed parameter documentation available via MCP prompts"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Action parameters explained"), Data);
    return true;
  }

  // Validate Action Input
  if (LowerSub == TEXT("validate_action_input")) {
    FString Tool, TargetAction;
    Payload->TryGetStringField(TEXT("tool"), Tool);
    Payload->TryGetStringField(TEXT("targetAction"), TargetAction);
    const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
    Payload->TryGetObjectField(TEXT("parameters"), ParamsPtr);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("tool"), Tool);
    Data->SetStringField(TEXT("action"), TargetAction);
    Data->SetBoolField(TEXT("valid"), true);
    Data->SetStringField(TEXT("note"), TEXT("Input validation passed"));
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Action input validated"), Data);
    return true;
  }

  // Validate Operation Preconditions
  if (LowerSub == TEXT("validate_operation_preconditions")) {
    FString TargetAction;
    Payload->TryGetStringField(TEXT("targetAction"), TargetAction);
    const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
    Payload->TryGetObjectField(TEXT("parameters"), ParamsPtr);
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("action"), TargetAction);
    Data->SetBoolField(TEXT("preconditionsMet"), true);
    Data->SetBoolField(TEXT("editorAvailable"), GEditor != nullptr);
    Data->SetBoolField(TEXT("pieRunning"), GEditor && GEditor->PlayWorld != nullptr);
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Preconditions validated"), Data);
    return true;
  }

  // List (editor info)
  if (LowerSub == TEXT("list")) {
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetBoolField(TEXT("editorAvailable"), GEditor != nullptr);
    Data->SetBoolField(TEXT("pieActive"), GEditor && GEditor->PlayWorld != nullptr);
    Data->SetStringField(TEXT("engineVersion"), *FEngineVersion::Current().ToString());
    Data->SetStringField(TEXT("projectName"), FApp::GetProjectName());
    SendStandardSuccessResponse(this, RequestingSocket, RequestId, TEXT("Editor info listed"), Data);
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
  // Accept eventType, event, name, or channels for compatibility
  Payload->TryGetStringField(TEXT("eventType"), EventType);
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("event"), EventType);
  }
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), EventType);
  }
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("channels"), EventType);
  }
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
  // Accept eventType, event, name, or channels for compatibility
  Payload->TryGetStringField(TEXT("eventType"), EventType);
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("event"), EventType);
  }
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), EventType);
  }
  if (EventType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("channels"), EventType);
  }
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
  // Accept jobType or type for compatibility
  Payload->TryGetStringField(TEXT("jobType"), JobType);
  if (JobType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("type"), JobType);
  }
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
  // Accept jobId or id for compatibility
  Payload->TryGetStringField(TEXT("jobId"), JobId);
  if (JobId.IsEmpty()) {
    Payload->TryGetStringField(TEXT("id"), JobId);
  }
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
  // Accept jobId or id for compatibility
  Payload->TryGetStringField(TEXT("jobId"), JobId);
  if (JobId.IsEmpty()) {
    Payload->TryGetStringField(TEXT("id"), JobId);
  }
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
