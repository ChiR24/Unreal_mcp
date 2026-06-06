#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_NiagaraHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateNiagaraRibbon(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_niagara_ribbon"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_niagara_ribbon payload missing"),
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

  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);

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

  FVector Start(0, 0, 0);
  const TSharedPtr<FJsonObject> *StartObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("start"), StartObj) && StartObj) {
    double X = 0;
    double Y = 0;
    double Z = 0;
    (*StartObj)->TryGetNumberField(TEXT("x"), X);
    (*StartObj)->TryGetNumberField(TEXT("y"), Y);
    (*StartObj)->TryGetNumberField(TEXT("z"), Z);
    Start = FVector(X, Y, Z);
  }

  ANiagaraActor *NiagaraActor = World->SpawnActor<ANiagaraActor>(
      ANiagaraActor::StaticClass(), Start, FRotator::ZeroRotator);
  if (!NiagaraActor) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to spawn Niagara actor"),
                        TEXT("SPAWN_FAILED"));
    return true;
  }

  NiagaraActor->SetActorLabel(Name.IsEmpty() ? TEXT("NiagaraRibbon") : Name);

  UNiagaraComponent *NiagaraComp = NiagaraActor->GetNiagaraComponent();
  if (NiagaraComp) {
    NiagaraComp->SetAsset(NiagaraSystem);
    NiagaraComp->SetVectorParameter(FName("User.RibbonStart"), Start);

    const TSharedPtr<FJsonObject> *EndObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("end"), EndObj) && EndObj) {
      double X = 0;
      double Y = 0;
      double Z = 0;
      (*EndObj)->TryGetNumberField(TEXT("x"), X);
      (*EndObj)->TryGetNumberField(TEXT("y"), Y);
      (*EndObj)->TryGetNumberField(TEXT("z"), Z);
      NiagaraComp->SetVectorParameter(FName("User.RibbonEnd"),
                                      FVector(X, Y, Z));
      NiagaraComp->SetVectorParameter(FName("User.BeamEnd"), FVector(X, Y, Z));
    }

    double Width = 10.0;
    if (Payload->TryGetNumberField(TEXT("width"), Width)) {
      NiagaraComp->SetFloatParameter(FName("User.RibbonWidth"),
                                     static_cast<float>(Width));
      NiagaraComp->SetFloatParameter(FName("User.BeamWidth"),
                                     static_cast<float>(Width));
    }

    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    FLinearColor ColorVal(1, 1, 1, 1);
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      double R = 1;
      double G = 1;
      double B = 1;
      double A = 1;
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
      (*ColorObj)->TryGetNumberField(TEXT("a"), A);
      ColorVal = FLinearColor(R, G, B, A);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *ColorArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("color"), ColorArr) && ColorArr &&
          ColorArr->Num() >= 3) {
        const double R = (*ColorArr)[0]->AsNumber();
        const double G = (*ColorArr)[1]->AsNumber();
        const double B = (*ColorArr)[2]->AsNumber();
        const double A = ColorArr->Num() > 3 ? (*ColorArr)[3]->AsNumber() : 1.0;
        ColorVal = FLinearColor(R, G, B, A);
      }
    }
    NiagaraComp->SetColorParameter(FName("User.RibbonColor"), ColorVal);
    NiagaraComp->SetColorParameter(FName("User.Color"), ColorVal);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorPath"), NiagaraActor->GetPathName());
  Resp->SetStringField(TEXT("actorName"), NiagaraActor->GetActorLabel());
  McpHandlerUtils::AddVerification(Resp, NiagaraActor);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Niagara ribbon created successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_niagara_ribbon requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
