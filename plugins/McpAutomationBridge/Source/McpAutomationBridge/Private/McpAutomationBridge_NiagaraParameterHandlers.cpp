#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_NiagaraHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleModifyNiagaraParameter(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("modify_niagara_parameter"),
                    ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("modify_niagara_parameter payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString ActorName;
  if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
      ActorName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ParameterName;
  if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) ||
      ParameterName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("parameterName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ParameterType;
  if (!Payload->TryGetStringField(TEXT("parameterType"), ParameterType)) {
    Payload->TryGetStringField(TEXT("type"), ParameterType);
  }
  if (ParameterType.IsEmpty()) {
    ParameterType = TEXT("Float");
  }

  if (!GEditor || !GEditor->GetEditorWorldContext().World()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Editor world not available"),
                        TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("EditorActorSubsystem not available"),
                        TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
    return true;
  }

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  ANiagaraActor *NiagaraActor = nullptr;
  for (AActor *Actor : AllActors) {
    if (Actor &&
        Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
      NiagaraActor = Cast<ANiagaraActor>(Actor);
      if (NiagaraActor) {
        break;
      }
    }
  }

  if (!NiagaraActor || !NiagaraActor->GetNiagaraComponent()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Niagara actor not found"),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  UNiagaraComponent *NiagaraComp = NiagaraActor->GetNiagaraComponent();
  bool bSuccess = false;

  if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase)) {
    double Value = 0.0;
    if (Payload->TryGetNumberField(TEXT("value"), Value)) {
      NiagaraComp->SetFloatParameter(FName(*ParameterName),
                                     static_cast<float>(Value));
      bSuccess = true;
    }
  } else if (ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
    const TSharedPtr<FJsonObject> *VectorObj = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *VectorArr = nullptr;
    if (Payload->TryGetObjectField(TEXT("value"), VectorObj) && VectorObj) {
      double VX = 0.0;
      double VY = 0.0;
      double VZ = 0.0;
      (*VectorObj)->TryGetNumberField(TEXT("x"), VX);
      (*VectorObj)->TryGetNumberField(TEXT("y"), VY);
      (*VectorObj)->TryGetNumberField(TEXT("z"), VZ);
      NiagaraComp->SetVectorParameter(FName(*ParameterName),
                                      FVector(VX, VY, VZ));
      bSuccess = true;
    } else if (Payload->TryGetArrayField(TEXT("value"), VectorArr) &&
               VectorArr && VectorArr->Num() >= 3) {
      const double VX = (*VectorArr)[0]->AsNumber();
      const double VY = (*VectorArr)[1]->AsNumber();
      const double VZ = (*VectorArr)[2]->AsNumber();
      NiagaraComp->SetVectorParameter(FName(*ParameterName),
                                      FVector(VX, VY, VZ));
      bSuccess = true;
    }
  } else if (ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase)) {
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *ColorArr = nullptr;
    if (Payload->TryGetObjectField(TEXT("value"), ColorObj) && ColorObj) {
      double R = 0.0;
      double G = 0.0;
      double B = 0.0;
      double A = 1.0;
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
      (*ColorObj)->TryGetNumberField(TEXT("a"), A);
      NiagaraComp->SetColorParameter(FName(*ParameterName),
                                     FLinearColor(R, G, B, A));
      bSuccess = true;
    } else if (Payload->TryGetArrayField(TEXT("value"), ColorArr) &&
               ColorArr && ColorArr->Num() >= 3) {
      const double R = (*ColorArr)[0]->AsNumber();
      const double G = (*ColorArr)[1]->AsNumber();
      const double B = (*ColorArr)[2]->AsNumber();
      const double A = ColorArr->Num() > 3 ? (*ColorArr)[3]->AsNumber() : 1.0;
      NiagaraComp->SetColorParameter(FName(*ParameterName),
                                     FLinearColor(R, G, B, A));
      bSuccess = true;
    }
  } else if (ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)) {
    bool Value = false;
    if (Payload->TryGetBoolField(TEXT("value"), Value)) {
      NiagaraComp->SetBoolParameter(FName(*ParameterName), Value);
      bSuccess = true;
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("parameterName"), ParameterName);
  Resp->SetStringField(TEXT("parameterType"), ParameterType);
  if (bSuccess && NiagaraActor) {
    McpHandlerUtils::AddVerification(Resp, NiagaraActor);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Niagara parameter modified successfully")
               : TEXT("Failed to modify parameter"),
      Resp, bSuccess ? FString() : TEXT("PARAMETER_SET_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("modify_niagara_parameter requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
