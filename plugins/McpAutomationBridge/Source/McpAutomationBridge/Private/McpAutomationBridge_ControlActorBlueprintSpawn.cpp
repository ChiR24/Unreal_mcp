#include "McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBlueprint(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Blueprint path required"), nullptr);
    return true;
  }

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
    const FString SafeBlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
    if (!SafeBlueprintPath.IsEmpty()) {
      if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(SafeBlueprintPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Loaded))
          ResolvedClass = C;
      }
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(BlueprintPath);

  if (!ResolvedClass) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Blueprint class not found"), nullptr);
    return true;
  }

  AActor *Spawned = nullptr;
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                              TEXT("Editor world not available"), nullptr);
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
  Spawned = TargetWorld->SpawnActor(ResolvedClass, &Location, &Rotation,
                                    SpawnParams);
  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn blueprint"), nullptr);
    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Resp->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Resp->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));

  // Add verification data
	McpHandlerUtils::AddVerification(Resp, Spawned);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}
