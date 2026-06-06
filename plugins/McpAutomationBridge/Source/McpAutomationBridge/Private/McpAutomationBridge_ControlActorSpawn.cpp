#include "McpAutomationBridge_ControlActorSupport.h"

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

  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
    if (bSpawnStaticMeshActor) {
      if (AStaticMeshActor *StaticMeshActor = Cast<AStaticMeshActor>(Spawned)) {
        if (UStaticMeshComponent *MeshComponent =
                StaticMeshActor->GetStaticMeshComponent()) {
          if (ResolvedStaticMesh) {
            MeshComponent->SetStaticMesh(ResolvedStaticMesh);
          }
          MeshComponent->SetMobility(EComponentMobility::Movable);
          MeshComponent->MarkRenderStateDirty();
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
          SkelComp->MarkRenderStateDirty();
        }
      }
    }
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn actor"));

    return true;
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
