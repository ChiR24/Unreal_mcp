#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_NiagaraHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleSpawnNiagaraActor(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("spawn_niagara_actor"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("spawn_niagara_actor payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SystemPath;
  if (!Payload->TryGetStringField(TEXT("systemPath"), SystemPath) ||
      SystemPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("systemPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  double X = 0.0;
  double Y = 0.0;
  double Z = 0.0;
  const TSharedPtr<FJsonObject> *LocationObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("location"), LocationObj) &&
      LocationObj) {
    (*LocationObj)->TryGetNumberField(TEXT("x"), X);
    (*LocationObj)->TryGetNumberField(TEXT("y"), Y);
    (*LocationObj)->TryGetNumberField(TEXT("z"), Z);
  }

  FString ActorName;
  Payload->TryGetStringField(TEXT("name"), ActorName);

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UWorld *World = GEditor->GetEditorWorldContext().World();
  if (!UEditorAssetLibrary::DoesAssetExist(SystemPath)) {
    SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Niagara system asset not found: %s"),
                        *SystemPath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UNiagaraSystem *NiagaraSystem =
      LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
  if (!NiagaraSystem) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to load Niagara system"),
                        TEXT("LOAD_FAILED"));
    return true;
  }

  ANiagaraActor *NiagaraActor = World->SpawnActor<ANiagaraActor>(
      ANiagaraActor::StaticClass(), FVector(X, Y, Z), FRotator::ZeroRotator);
  if (!NiagaraActor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn Niagara actor"),
                        TEXT("SPAWN_FAILED"));
    return true;
  }

  if (NiagaraActor->GetNiagaraComponent()) {
    NiagaraActor->GetNiagaraComponent()->SetAsset(NiagaraSystem);
  }

  if (!ActorName.IsEmpty()) {
    NiagaraActor->SetActorLabel(ActorName);
  } else {
    NiagaraActor->SetActorLabel(
        FString::Printf(TEXT("NiagaraActor_%s"),
                        *FGuid::NewGuid().ToString(EGuidFormats::Short)));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorPath"), NiagaraActor->GetPathName());
  Resp->SetStringField(TEXT("actorName"), NiagaraActor->GetActorLabel());
  Resp->SetStringField(TEXT("systemPath"), SystemPath);
  McpHandlerUtils::AddVerification(Resp, NiagaraActor);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Niagara actor spawned successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("spawn_niagara_actor requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
