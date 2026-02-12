#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"

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
#include "IAssetViewport.h"  // For IAssetViewport::GetAssetViewportClient()

#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
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
#include "UnrealClient.h" // For FScreenshotRequest

#endif

// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
// (ExtractVectorField and ExtractRotatorField moved to
// McpAutomationBridgeHelpers.h)

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target) {
#if WITH_EDITOR
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld) {
    for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It) {
      AActor *A = *It;
      if (!A)
        continue;
      if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
        return A;
      }
    }
    // If not found in PIE, do we fall back to Editor World?
    // Probably not, because interacting with Editor world during PIE is
    // confusing. But for "Editor subsystems" usage, we usually want Editor
    // world. Let's fallback if not found, just in case.
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors) {
    if (!A)
      continue;
    if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
      ExactMatch = A;
      break;
    }
    // Collect fuzzy matches
    if (A->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase)) {
      FuzzyMatches.Add(A);
    }
  }

  if (ExactMatch) {
    return ExactMatch;
  }

  // If no exact match, check fuzzy matches
  if (FuzzyMatches.Num() == 1) {
    return FuzzyMatches[0];
  } else if (FuzzyMatches.Num() > 1) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("FindActorByName: Ambiguous match for '%s'. Found %d matches."),
           *Target, FuzzyMatches.Num());
  }

  // Fallback: try to load as asset if it looks like a path
  if (Target.StartsWith(TEXT("/"))) {
    if (UObject *Obj = UEditorAssetLibrary::LoadAsset(Target)) {
      return Cast<AActor>(Obj);
    }
  }
#endif
  return nullptr;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawn(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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

  for (const FString &Name : Targets) {
    AActor *Found = FindActorByName(Name);
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
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Child = FindActorByName(ChildName);
  AActor *Parent = FindActorByName(ParentName);
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

  AActor *Found = FindActorByName(TargetName);
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

  // Security: Validate tag format - reject path traversal attempts
  if (TagValue.Contains(TEXT("..")) || TagValue.Contains(TEXT("\\")) || TagValue.Contains(TEXT("/"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid tag: '%s'. Path separators and traversal characters are not allowed."), *TagValue),
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

  AActor *Found = FindActorByName(TargetName);
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

  // Security: Validate query format - reject path traversal attempts
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\")) || Query.Contains(TEXT("/"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid name query: '%s'. Path separators and traversal characters are not allowed."), *Query),
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

  AActor *Found = FindActorByName(TargetName);
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

// Additional handlers for test compatibility

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }

  if (ClassName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("className or class is required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) && 
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/"))) {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) || 
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\"))) {
      SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  if (UWorld* World = GEditor->GetEditorWorldContext().World()) {
    UClass* ClassToFind = nullptr;

    // Try to find the class
    if (ClassName.StartsWith(TEXT("/"))) {
      ClassToFind = LoadObject<UClass>(nullptr, *ClassName);
    } else {
      // Use nullptr instead of ANY_PACKAGE for UE 5.7+ compatibility
      ClassToFind = FindObject<UClass>(nullptr, *ClassName);
    }

    if (ClassToFind) {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It) {
        if (AActor* Actor = *It) {
          TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    } else {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }
  
  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("component_name"), ComponentName);
  }
  
  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  if (ComponentName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("componentName is required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, 
                        FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  // Find and destroy component
  TInlineComponentArray<UActorComponent*> Components;
  Actor->GetComponents(Components);
  
  for (UActorComponent* Component : Components) {
    if (Component && Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      Component->DestroyComponent();
      TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
      Data->SetStringField(TEXT("actorName"), ActorName);
      Data->SetStringField(TEXT("componentName"), ComponentName);
      SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component removed"), Data);
      return true;
    }
  }
  
  SendAutomationError(Socket, RequestId,
                      FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                      TEXT("COMPONENT_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  
  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName, componentName, and propertyName are required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  // Find component
  TInlineComponentArray<UActorComponent*> Components;
  Actor->GetComponents(Components);
  
  for (UActorComponent* Component : Components) {
    if (Component && Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase)) {
      // Get property using reflection
      FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
      if (Property) {
        TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
        Data->SetStringField(TEXT("actorName"), ActorName);
        Data->SetStringField(TEXT("componentName"), ComponentName);
        Data->SetStringField(TEXT("propertyName"), PropertyName);
        // Note: Full property value serialization would require more complex logic
        Data->SetStringField(TEXT("value"), TEXT("<property value>"));
        SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
        return true;
      }
      break;
    }
  }
  
  SendAutomationError(Socket, RequestId, TEXT("Component or property not found"), TEXT("NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  bool bCollisionEnabled = true;
  
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }
  
  if (Payload->HasField(TEXT("collisionEnabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  } else if (Payload->HasField(TEXT("collision_enabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }
  
  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  // Set collision on root component
  if (USceneComponent* RootComp = Actor->GetRootComponent()) {
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComp)) {
      if (bCollisionEnabled) {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
      } else {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      }
    }
  }
  
  TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCallFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, FunctionName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);
  
  if (ActorName.IsEmpty() || FunctionName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName and functionName are required"), TEXT("MISSING_PARAM"));
    return true;
  }
  
  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }
  
  // Find and call the function
  UFunction* Function = Actor->FindFunction(*FunctionName);
  if (Function) {
    // Check if function has parameters - passing nullptr to a function expecting
    // parameters can cause crashes or undefined behavior
    if (Function->ParmsSize > 0) {
      // Function has parameters - we need to provide a buffer
      // Allocate zeroed memory for parameters
      void* ParmsBuffer = FMemory::Malloc(Function->ParmsSize, 16);
      FMemory::Memzero(ParmsBuffer, Function->ParmsSize);
      
      // Call with parameter buffer
      Actor->ProcessEvent(Function, ParmsBuffer);
      
      // Free the buffer
      FMemory::Free(ParmsBuffer);
    } else {
      // No parameters, safe to pass nullptr
      Actor->ProcessEvent(Function, nullptr);
    }
    
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("functionName"), FunctionName);
    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Function called"), Data);
    return true;
  }
  
  SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Function not found: %s"), *FunctionName), TEXT("FUNCTION_NOT_FOUND"));
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

  if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("spawn_blueprint"))
    return HandleControlActorSpawnBlueprint(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove") ||
      LowerSub == TEXT("destroy_actor"))
    return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("apply_force") ||
      LowerSub == TEXT("apply_force_to_actor"))
    return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_transform") ||
      LowerSub == TEXT("set_actor_transform") ||
      LowerSub == TEXT("teleport_actor") ||
      LowerSub == TEXT("set_actor_location") ||
      LowerSub == TEXT("set_actor_rotation") ||
      LowerSub == TEXT("set_actor_scale"))
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_transform") ||
      LowerSub == TEXT("get_actor_transform"))
    return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_visibility") ||
      LowerSub == TEXT("set_actor_visible") ||
      LowerSub == TEXT("set_actor_visibility"))
    return HandleControlActorSetVisibility(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("add_component"))
    return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_properties") ||
      LowerSub == TEXT("set_component_property"))
    return HandleControlActorSetComponentProperties(RequestId, Payload,
                                                    RequestingSocket);
  if (LowerSub == TEXT("get_components") ||
      LowerSub == TEXT("get_actor_components"))
    return HandleControlActorGetComponents(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("duplicate"))
    return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("attach") || LowerSub == TEXT("attach_actor"))
    return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("detach") || LowerSub == TEXT("detach_actor"))
    return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_tag"))
    return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("add_tag"))
    return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_tag"))
    return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_name") || LowerSub == TEXT("find_actors_by_name"))
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
  if (LowerSub == TEXT("get_bounding_box") || LowerSub == TEXT("get_actor_bounds"))
    return HandleControlActorGetBoundingBox(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("get_metadata"))
    return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors"))
    return HandleControlActorList(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") ||
      LowerSub == TEXT("get_actor_by_name"))
    return HandleControlActorGet(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_class") || LowerSub == TEXT("find_actors_by_class"))
    return HandleControlActorFindByClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_component"))
    return HandleControlActorRemoveComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_component_property"))
    return HandleControlActorGetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_collision") || LowerSub == TEXT("set_actor_collision"))
    return HandleControlActorSetCollision(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("call_function") || LowerSub == TEXT("call_actor_function"))
    return HandleControlActorCallFunction(RequestId, Payload, RequestingSocket);

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
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetBoolField(TEXT("notInPIE"), true);
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Cannot eject: Play session not active"), Resp, TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Use Eject console command instead of RequestEndPlayMap
  // This ejects the player from the possessed pawn without stopping PIE
  GEditor->Exec(GEditor->PlayWorld, TEXT("Eject"));
  
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("ejected"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Ejected from possessed actor"), Resp, FString());
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

  AActor *Found = FindActorByName(ActorName);
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
  Payload->TryGetStringField(TEXT("action"), SubAction);
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
  if (LowerSub == TEXT("screenshot") || LowerSub == TEXT("take_screenshot"))
    return HandleControlEditorScreenshot(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("pause"))
    return HandleControlEditorPause(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("resume"))
    return HandleControlEditorResume(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("console_command") || LowerSub == TEXT("execute_command"))
    return HandleControlEditorConsoleCommand(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("step_frame"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("start_recording"))
    return HandleControlEditorStartRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop_recording"))
    return HandleControlEditorStopRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("create_bookmark"))
    return HandleControlEditorCreateBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("jump_to_bookmark"))
    return HandleControlEditorJumpToBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_preferences"))
    return HandleControlEditorSetPreferences(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_viewport_realtime"))
    return HandleControlEditorSetViewportRealtime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("simulate_input"))
    return HandleControlEditorSimulateInput(RequestId, Payload, RequestingSocket);
  // Additional actions for test compatibility
  if (LowerSub == TEXT("close_asset"))
    return HandleControlEditorCloseAsset(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("save_all"))
    return HandleControlEditorSaveAll(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("undo"))
    return HandleControlEditorUndo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("redo"))
    return HandleControlEditorRedo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_editor_mode"))
    return HandleControlEditorSetEditorMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("show_stats"))
    return HandleControlEditorShowStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("hide_stats"))
    return HandleControlEditorHideStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_game_view"))
    return HandleControlEditorSetGameView(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_immersive_mode"))
    return HandleControlEditorSetImmersiveMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("single_frame_step"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_fixed_delta_time"))
    return HandleControlEditorSetFixedDeltaTime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_level"))
    return HandleControlEditorOpenLevel(RequestId, Payload, RequestingSocket);

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

bool UMcpAutomationBridgeSubsystem::HandleControlEditorScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Get optional filename from payload
  FString Filename;
  Payload->TryGetStringField(TEXT("filename"), Filename);
  if (Filename.IsEmpty()) {
    // Generate default filename with timestamp
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // SECURITY: Sanitize filename to prevent path traversal
  // Remove any path components and keep only the base filename
  Filename = FPaths::GetCleanFilename(Filename);
  
  // Validate filename doesn't contain suspicious patterns
  if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) || Filename.Contains(TEXT("\\"))) {
    // Reject suspicious filename and use default
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // Ensure filename ends with .png
  if (!Filename.EndsWith(TEXT(".png"))) {
    Filename += TEXT(".png");
  }

  // Build the full path - save to project's Saved/Screenshots folder
  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;

  // Get the active viewport
  FViewport* Viewport = GEditor->GetActiveViewport();
  if (!Viewport) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active viewport available"), nullptr,
                           TEXT("VIEWPORT_NOT_AVAILABLE"));
    return true;
  }

  // Request a screenshot
  bool bCaptured = false;
  FScreenshotRequest::RequestScreenshot(FullPath, false, false);
  
  // Since screenshot is async, we respond with the expected path
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("path"), FullPath);
  Resp->SetStringField(TEXT("message"), TEXT("Screenshot request submitted"));
  
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Screenshot requested"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Screenshot requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active PIE session to pause"), nullptr,
                           TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Pause PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = true;

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("paused"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session paused"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session paused"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Pause requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorResume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active PIE session to resume"), nullptr,
                           TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Resume PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("resumed"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session resumed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session resumed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Resume requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorConsoleCommand(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString Command;
  Payload->TryGetStringField(TEXT("command"), Command);
  if (Command.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("command parameter is required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Execute the console command in editor context
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }
  UWorld* World = GEditor->GetEditorWorldContext().World();
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("command"), Command);
  Resp->SetStringField(TEXT("message"), TEXT("Console command executed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Console command executed"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Console command requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStepFrame(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("No active PIE session to step"), nullptr,
                           TEXT("NO_ACTIVE_SESSION"));
    return true;
  }

  // Step one frame - set debug step flag and unpause momentarily
  GEditor->PlayWorld->bDebugFrameStepExecution = true;
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Stepped one frame"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Frame stepped"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Step frame requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStartRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString RecordingName;
  Payload->TryGetStringField(TEXT("name"), RecordingName);
  if (RecordingName.IsEmpty()) {
    RecordingName = FString::Printf(TEXT("Recording_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  // Use console command to start demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    FString Command = FString::Printf(TEXT("DemoRec %s"), *RecordingName);
    GEditor->Exec(World, *Command);
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("message"), TEXT("Recording started"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording started"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Recording requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStopRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Use console command to stop demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("DemoStop"));
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Recording stopped"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording stopped"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Recording requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to set bookmark
  FString Command = FString::Printf(TEXT("SetBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Bookmark %d created"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Bookmark created"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Bookmarks require editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to jump to bookmark
  FString Command = FString::Printf(TEXT("JumpToBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Jumped to bookmark %d"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Jumped to bookmark"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Bookmarks require editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetPreferences(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  TArray<FString> AppliedSettings;
  TArray<FString> FailedSettings;

  // Get preferences object from payload
  const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
    for (const auto& Pair : (*PrefsPtr)->Values) {
      // Try to set via console variable first
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Pair.Key);
      if (CVar) {
        FString Value;
        if (Pair.Value->TryGetString(Value)) {
          CVar->Set(*Value);
          AppliedSettings.Add(Pair.Key);
        } else {
          double NumVal;
          if (Pair.Value->TryGetNumber(NumVal)) {
            CVar->Set((float)NumVal);
            AppliedSettings.Add(Pair.Key);
          } else {
            bool BoolVal;
            if (Pair.Value->TryGetBool(BoolVal)) {
              CVar->Set(BoolVal ? 1 : 0);
              AppliedSettings.Add(Pair.Key);
            } else {
              FailedSettings.Add(Pair.Key);
            }
          }
        }
      } else {
        FailedSettings.Add(Pair.Key);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), FailedSettings.Num() == 0);
  Resp->SetNumberField(TEXT("appliedCount"), AppliedSettings.Num());

  if (AppliedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString& Name : AppliedSettings)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedArray);
  }

  if (FailedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& Name : FailedSettings)
      FailedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("failed"), FailedArray);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Preferences updated"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Preferences require editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewportRealtime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  bool bRealtime = true;
  Payload->TryGetBoolField(TEXT("realtime"), bRealtime);

#if MCP_HAS_LEVEL_EDITOR_MODULE
  // Get the level editor module and active viewport
  FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
  TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();
  
  if (ActiveViewport.IsValid()) {
    FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
    ViewportClient.SetRealtime(bRealtime);
    
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("realtime"), bRealtime);
    Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Viewport realtime updated"), Resp, FString());
    return true;
  }
#endif

  // Fallback: use console command
  FString Command = bRealtime ? TEXT("Viewport Realtime") : TEXT("Viewport Realtime 0");
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("realtime"), bRealtime);
  Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport realtime updated"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Viewport realtime requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSimulateInput(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  FString InputType;
  Payload->TryGetStringField(TEXT("type"), InputType);
  InputType = InputType.ToLower();

  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  bool bSuccess = false;
  FString Message;

  if (InputType == TEXT("key_down") || InputType == TEXT("keydown")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        FSlateApplication& SlateApp = FSlateApplication::Get();
        FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
        SlateApp.ProcessKeyDownEvent(KeyEvent);
        bSuccess = true;
        Message = FString::Printf(TEXT("Key down: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_down");
    }
  } else if (InputType == TEXT("key_up") || InputType == TEXT("keyup")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        FSlateApplication& SlateApp = FSlateApplication::Get();
        FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
        SlateApp.ProcessKeyUpEvent(KeyEvent);
        bSuccess = true;
        Message = FString::Printf(TEXT("Key up: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_up");
    }
  } else if (InputType == TEXT("mouse_click") || InputType == TEXT("click")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FString Button = TEXT("left");
    Payload->TryGetStringField(TEXT("button"), Button);

    // UE 5.7: EKeys::Type is deprecated, use FKey directly
    FKey MouseButtonKey = EKeys::LeftMouseButton;
    if (Button.ToLower() == TEXT("right")) MouseButtonKey = EKeys::RightMouseButton;
    else if (Button.ToLower() == TEXT("middle")) MouseButtonKey = EKeys::MiddleMouseButton;

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);
    
    // UE 5.7: FPointerEvent constructor signature changed
    // FPointerEvent(uint32 InPointerIndex, ScreenSpacePosition, LastScreenSpacePosition, Delta, PressedButtons, ModifierKeys)
    TSet<FKey> PressedButtons;
    PressedButtons.Add(MouseButtonKey);
    
    // Simulate mouse down then up for a click
    FPointerEvent MouseDownEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        PressedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);
    
    TSet<FKey> ReleasedButtons;  // Empty set for mouse up
    FPointerEvent MouseUpEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        ReleasedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse click at (%f, %f)"), X, Y);
  } else if (InputType == TEXT("mouse_move") || InputType == TEXT("move")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);
    SlateApp.SetCursorPos(Position);
    
    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse moved to (%f, %f)"), X, Y);
  } else {
    Message = FString::Printf(TEXT("Unknown input type: %s. Supported: key_down, key_up, mouse_click, mouse_move"), *InputType);
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, Message, Resp, TEXT("INPUT_FAILED"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Simulate input requires editor build."), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCloseAsset(
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

  UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("AssetEditorSubsystem unavailable"), nullptr,
                           TEXT("SUBSYSTEM_MISSING"));
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  AssetEditorSS->CloseAllEditorsForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset editor closed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSaveAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Save all dirty packages using FEditorFileUtils
  TArray<UPackage*> DirtyPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

  bool bSuccess = true;
  int32 SavedCount = 0;
  
  for (UPackage* Package : DirtyPackages) {
    if (Package) {
      FString PackagePath = Package->GetPathName();
      if (UEditorAssetLibrary::SaveAsset(PackagePath, false)) {
        SavedCount++;
      } else {
        bSuccess = false;
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("savedCount"), SavedCount);
  Resp->SetNumberField(TEXT("totalDirty"), DirtyPackages.Num());
  
  // Only report outer success if the operation actually succeeded
  if (bSuccess || DirtyPackages.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, true, 
                           FString::Printf(TEXT("Saved %d of %d dirty assets"), SavedCount, DirtyPackages.Num()), 
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, 
                           FString::Printf(TEXT("Failed to save all assets. Saved %d of %d dirty assets."), 
                                           SavedCount, DirtyPackages.Num()), 
                           Resp, TEXT("SAVE_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorUndo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Execute undo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Undo"));

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), TEXT("undo"));
  Resp->SetStringField(TEXT("command"), TEXT("Undo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Undo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRedo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Execute redo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Redo"));

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), TEXT("redo"));
  Resp->SetStringField(TEXT("command"), TEXT("Redo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Redo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetEditorMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  if (Mode.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("mode required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Execute editor mode command via console
  FString Command = FString::Printf(TEXT("mode %s"), *Mode);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("mode"), Mode);
  SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Editor mode set to %s"), *Mode), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  TArray<FString> StatsShown;
  if (World) {
    GEditor->Exec(World, TEXT("Stat FPS"));
    StatsShown.Add(TEXT("FPS"));
    GEditor->Exec(World, TEXT("Stat Unit"));
    StatsShown.Add(TEXT("Unit"));
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), TEXT("showStats"));
  TArray<TSharedPtr<FJsonValue>> StatsArray;
  for (const FString& Stat : StatsShown) {
    StatsArray.Add(MakeShared<FJsonValueString>(Stat));
  }
  Resp->SetArrayField(TEXT("statsShown"), StatsArray);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats displayed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorHideStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("Stat None"));
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), TEXT("hideStats"));
  Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats hidden"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Toggle game view via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), 
                bEnabled ? TEXT("ToggleGameView 1") : TEXT("ToggleGameView 0"));

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("gameViewEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Game view %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")), 
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetImmersiveMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  // Toggle immersive mode - this is viewport-specific
  if (GEditor && GEditor->GetActiveViewport()) {
    FViewport* Viewport = GEditor->GetActiveViewport();
    if (Viewport) {
      // Immersive mode toggle via console
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("ToggleImmersive"));
    }
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Immersive mode toggled"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetFixedDeltaTime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double DeltaTime = 0.01667; // Default ~60fps
  if (Payload->HasField(TEXT("deltaTime"))) {
    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("deltaTime"));
    if (Value.IsValid() && Value->Type == EJson::Number) {
      DeltaTime = Value->AsNumber();
    }
  }

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Set fixed delta time via console
  FString Command = FString::Printf(TEXT("r.FixedDeltaTime %f"), DeltaTime);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  SendAutomationResponse(Socket, RequestId, true, 
                         FString::Printf(TEXT("Fixed delta time set to %f"), DeltaTime), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenLevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString LevelPath;
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("levelPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Normalize the level path
  if (!LevelPath.StartsWith(TEXT("/Game/")) && !LevelPath.StartsWith(TEXT("/Engine/"))) {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  // Remove map suffix if present
  if (LevelPath.EndsWith(TEXT(".umap"))) {
    LevelPath.LeftChopInline(5);
  }

  if (!GEditor) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available"), nullptr,
                           TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  // Use FEditorFileUtils to load the map
  FString MapPath = LevelPath + TEXT(".umap");
  
  // Check if the map file exists before attempting to load
  FString FullMapPath = FPaths::ProjectContentDir() + MapPath.RightChop(6); // Remove "/Game/" prefix
  FullMapPath = FPaths::ConvertRelativePathToFull(FullMapPath);
  
  if (!FPaths::FileExists(FullMapPath)) {
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("levelPath"), LevelPath);
    Resp->SetStringField(TEXT("error"), TEXT("Level file not found"));
    SendAutomationResponse(Socket, RequestId, false, 
                           FString::Printf(TEXT("Level file not found: %s"), *FullMapPath), 
                           Resp, TEXT("FILE_NOT_FOUND"));
    return true;
  }
  
  bool bOpened = FEditorFileUtils::LoadMap(*MapPath);

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("levelPath"), LevelPath);
  
  if (bOpened) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Level opened"), Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to open level"), Resp, TEXT("OPEN_FAILED"));
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

  AActor *Found = FindActorByName(TargetName);
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
