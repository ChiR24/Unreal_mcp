#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

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
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);

  UClass *ResolvedClass = nullptr;
  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
  UStaticMesh *ResolvedStaticMesh = nullptr;
  USkeletalMesh *ResolvedSkeletalMesh = nullptr;

  // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to
  // avoid LogEditorAssetSubsystem errors
  if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) &&
      !ClassPath.StartsWith(TEXT("/Script/"))) {
    const FString SafeClassPath = SanitizeProjectRelativePath(ClassPath);
    if (!SafeClassPath.IsEmpty()) {
      if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(SafeClassPath)) {
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
  }
  if (!ResolvedClass && !ResolvedStaticMesh && !ResolvedSkeletalMesh)
    ResolvedClass = ResolveClassByName(ClassPath);

  // If explicit mesh path provided for a general spawn request
  if (!ResolvedStaticMesh && !ResolvedSkeletalMesh && !MeshPath.IsEmpty()) {
    const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (!SafeMeshPath.IsEmpty()) {
      if (UObject *MeshObj = UEditorAssetLibrary::LoadAsset(SafeMeshPath)) {
        ResolvedStaticMesh = Cast<UStaticMesh>(MeshObj);
        if (!ResolvedStaticMesh)
          ResolvedSkeletalMesh = Cast<USkeletalMesh>(MeshObj);
      }
    }
  }

  // If the caller explicitly asked for a mesh but it could not be resolved to a
  // static/skeletal mesh, fail BEFORE spawning anything. Otherwise we'd create an
  // actor we can't configure as requested and report it as a (misleading) success.
  if (!MeshPath.IsEmpty() && !ResolvedStaticMesh && !ResolvedSkeletalMesh) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("MESH_NOT_FOUND"),
        FString::Printf(TEXT("Requested mesh could not be loaded as a static or "
                             "skeletal mesh: %s"),
                        *MeshPath));
    return true;
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

  AActor *Spawned = nullptr;

  // Use direct world spawning for both PIE and editor worlds. EditorActorSubsystem
  // routes through viewport placement and hit-proxy rendering, which crashes
  // under NullRHI even when automation provides an explicit transform.
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                              TEXT("Editor world not available"));
    return true;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }

  UClass *ClassToSpawn =
      ResolvedClass
          ? ResolvedClass
          : (bSpawnStaticMeshActor ? AStaticMeshActor::StaticClass()
                                   : (bSpawnSkeletalMeshActor
                                          ? ASkeletalMeshActor::StaticClass()
                                          : AActor::StaticClass()));
  Spawned = TargetWorld->SpawnActor(ClassToSpawn, &Location, &Rotation,
                                    SpawnParams);

  if (!IsValid(Spawned)) {
    // SpawnActor returned null / an invalid object — nothing was added to the
    // world, so there is nothing to roll back.
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn actor"));
    return true;
  }

  // Transactional rollback: if a required post-spawn step fails, destroy the actor
  // we just created so a failed request leaves the world unchanged rather than
  // leaking a half-configured actor into the level (mirrors the spline handlers,
  // which Destroy() the new actor when component setup fails).
  auto RollbackSpawn = [&Spawned]() {
    if (IsValid(Spawned)) {
      Spawned->Destroy();
    }
    Spawned = nullptr;
  };

  Spawned->Modify();
  Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                       ETeleportType::TeleportPhysics);
  if (bSpawnStaticMeshActor) {
    AStaticMeshActor *StaticMeshActor = Cast<AStaticMeshActor>(Spawned);
    UStaticMeshComponent *MeshComponent =
        StaticMeshActor ? StaticMeshActor->GetStaticMeshComponent() : nullptr;
    if (ResolvedStaticMesh && !MeshComponent) {
      // A mesh was explicitly resolved but the spawned actor can't hold it.
      RollbackSpawn();
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("MESH_APPLY_FAILED"),
          TEXT("Could not apply the requested static mesh: the spawned actor has "
               "no static mesh component."));
      return true;
    }
    if (MeshComponent) {
      if (ResolvedStaticMesh) {
        MeshComponent->SetStaticMesh(ResolvedStaticMesh);
      }
      MeshComponent->SetMobility(EComponentMobility::Movable);
      MeshComponent->MarkRenderStateDirty();
    }
  } else if (bSpawnSkeletalMeshActor) {
    ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned);
    USkeletalMeshComponent *SkelComp =
        SkelActor ? SkelActor->GetSkeletalMeshComponent() : nullptr;
    if (ResolvedSkeletalMesh && !SkelComp) {
      RollbackSpawn();
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("MESH_APPLY_FAILED"),
          TEXT("Could not apply the requested skeletal mesh: the spawned actor "
               "has no skeletal mesh component."));
      return true;
    }
    if (SkelComp) {
      if (ResolvedSkeletalMesh) {
        SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
      }
      SkelComp->SetMobility(EComponentMobility::Movable);
      SkelComp->MarkRenderStateDirty();
    }
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
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

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath?, meshPath? }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Data->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Data->SetStringField(TEXT("actorPath"), Spawned->GetPathName());

  // Provide the resolved class path useful for referencing
  if (ResolvedClass)
    Data->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  else
    Data->SetStringField(TEXT("classPath"), ClassPath);

  if (ResolvedStaticMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
  else if (ResolvedSkeletalMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedSkeletalMesh->GetPathName());

  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Data->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Spawned);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Data);
  return true;

#else
  return false;
#endif
}

/**
 * Spawn an actor from a Blueprint class and apply the requested transform fields.
 *
 * Blueprint spawning mirrors the regular actor spawn path by accepting optional
 * `location`, `rotation`, and `scale`, then returning the applied scale in the
 * response payload for client-side verification.
 */
