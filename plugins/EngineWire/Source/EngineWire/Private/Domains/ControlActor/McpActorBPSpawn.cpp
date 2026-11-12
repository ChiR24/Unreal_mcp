#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

bool UEngineWireSubsystem::HandleControlActorSpawnBlueprint(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
  if (UBlueprint *BlueprintAsset = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError)) {
    ResolvedClass = BlueprintAsset->GeneratedClass;
  }
  if (!ResolvedClass) {
    // A class path or name. A path is read in its canonical form first (the /Content alias, a missing leading slash).
    const FString SafeBlueprintPath =
        BlueprintPath.Contains(TEXT("/")) ? SanitizeProjectRelativePath(BlueprintPath) : FString();
    ResolvedClass = ResolveClassByName(SafeBlueprintPath.IsEmpty() ? BlueprintPath : SafeBlueprintPath);
  }

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

  // One undo step for what this spawn makes, opened before the level is touched.
  const TUniquePtr<FMcpScopedEditorTransaction> Transaction = McpBeginSpawnTransaction(RequestId);

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }
  bool bSpawnStarted = false;
  Spawned = McpSpawnActorNoting(TargetWorld, ResolvedClass, Location, Rotation, SpawnParams, bSpawnStarted);
  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, McpSpawnFailureCode(bSpawnStarted),
                              McpSpawnFailureMessage(bSpawnStarted, TEXT("Failed to spawn blueprint")), nullptr);
    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);
  McpApplySpawnOrganisation(Spawned, Payload);

  // actorPath, actorName and actorClass come from AddVerification below; an actor { id, name, path }
  // object used to repeat them, with the path twice.
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  Resp->SetArrayField(TEXT("scale"), McpHandlerUtils::VectorToJsonArray(Spawned->GetActorScale3D()));
  if (Transaction) {
    Transaction->DescribeInto(Resp);
  }

	McpHandlerUtils::AddVerification(Resp, Spawned);
	McpDescribeActorPlacement(RequestId, Spawned, Resp);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
}
