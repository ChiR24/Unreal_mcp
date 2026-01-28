#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Async/Async.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraScriptFactoryNew.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateNiagaraSystem(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_niagara_system"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_niagara_system payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SystemName;
  if (!Payload->TryGetStringField(TEXT("name"), SystemName) ||
      SystemName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SavePath;
  if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
      SavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("savePath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Check for Niagara plugin availability via module system
  // Previous check for asset existence failed even when Niagara was enabled
  // because it was looking for engine content which requires "Show Engine
  // Content" in Content Browser
  if (!FModuleManager::Get().IsModuleLoaded(TEXT("Niagara"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Niagara plugin module is not loaded. Please "
                             "enable and restart the editor."),
                        TEXT("DEPENDENCY_MISSING"));
    return true;
  }

  UNiagaraSystemFactoryNew *Factory = NewObject<UNiagaraSystemFactoryNew>();
  if (!Factory) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara system factory"),
                        TEXT("FACTORY_FAILED"));
    return true;
  }

  FString PackagePath = SavePath;
  FString AssetName = SystemName;
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
      AssetName, PackagePath, UNiagaraSystem::StaticClass(), Factory);
  UNiagaraSystem *NiagaraSystem = Cast<UNiagaraSystem>(NewAsset);
  McpSafeAssetSave(NiagaraSystem);

  if (!NiagaraSystem) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara system asset"),
                        TEXT("ASSET_CREATION_FAILED"));
    return true;
  }


  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("systemPath"), NiagaraSystem->GetPathName());
  Resp->SetStringField(TEXT("systemName"), SystemName);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Niagara system created successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_niagara_system requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateNiagaraEmitter(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("create_niagara_emitter"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("create_niagara_emitter payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString EmitterName;
  if (!Payload->TryGetStringField(TEXT("name"), EmitterName) ||
      EmitterName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("name required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SavePath;
  if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
      SavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("savePath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Check for Niagara plugin availability via module system
  if (!FModuleManager::Get().IsModuleLoaded(TEXT("Niagara"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Niagara plugin module is not loaded. Please "
                             "enable and restart the editor."),
                        TEXT("DEPENDENCY_MISSING"));
    return true;
  }

  UNiagaraEmitterFactoryNew *Factory = NewObject<UNiagaraEmitterFactoryNew>();
  if (!Factory) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara emitter factory"),
                        TEXT("FACTORY_FAILED"));
    return true;
  }

  FString PackagePath = SavePath;
  FString AssetName = EmitterName;
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
  UObject *NewAsset = AssetToolsModule.Get().CreateAsset(
      AssetName, PackagePath, UNiagaraEmitter::StaticClass(), Factory);
  UNiagaraEmitter *NiagaraEmitter = Cast<UNiagaraEmitter>(NewAsset);
  McpSafeAssetSave(NiagaraEmitter);

  if (!NiagaraEmitter) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara emitter asset"),
                        TEXT("ASSET_CREATION_FAILED"));
    return true;
  }


  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("emitterPath"), NiagaraEmitter->GetPathName());
  Resp->SetStringField(TEXT("emitterName"), EmitterName);

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Niagara emitter created successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("create_niagara_emitter requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

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

  double X = 0.0, Y = 0.0, Z = 0.0;
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

  FVector Location(X, Y, Z);
  ANiagaraActor *NiagaraActor = World->SpawnActor<ANiagaraActor>(
      ANiagaraActor::StaticClass(), Location, FRotator::ZeroRotator);

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

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorPath"), NiagaraActor->GetPathName());
  Resp->SetStringField(TEXT("actorName"), NiagaraActor->GetActorLabel());
  Resp->SetStringField(TEXT("systemPath"), SystemPath);

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
  if (ParameterType.IsEmpty())
    ParameterType = TEXT("Float");

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
      if (NiagaraActor)
        break;
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
      double VX = 0.0, VY = 0.0, VZ = 0.0;
      (*VectorObj)->TryGetNumberField(TEXT("x"), VX);
      (*VectorObj)->TryGetNumberField(TEXT("y"), VY);
      (*VectorObj)->TryGetNumberField(TEXT("z"), VZ);
      NiagaraComp->SetVectorParameter(FName(*ParameterName),
                                      FVector(VX, VY, VZ));
      bSuccess = true;
    } else if (Payload->TryGetArrayField(TEXT("value"), VectorArr) &&
               VectorArr && VectorArr->Num() >= 3) {
      double VX = (*VectorArr)[0]->AsNumber();
      double VY = (*VectorArr)[1]->AsNumber();
      double VZ = (*VectorArr)[2]->AsNumber();
      NiagaraComp->SetVectorParameter(FName(*ParameterName),
                                      FVector(VX, VY, VZ));
      bSuccess = true;
    }
  } else if (ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase)) {
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *ColorArr = nullptr;

    if (Payload->TryGetObjectField(TEXT("value"), ColorObj) && ColorObj) {
      double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
      (*ColorObj)->TryGetNumberField(TEXT("a"), A);
      NiagaraComp->SetColorParameter(FName(*ParameterName),
                                     FLinearColor(R, G, B, A));
      bSuccess = true;
    } else if (Payload->TryGetArrayField(TEXT("value"), ColorArr) && ColorArr &&
               ColorArr->Num() >= 3) {
      double R = (*ColorArr)[0]->AsNumber();
      double G = (*ColorArr)[1]->AsNumber();
      double B = (*ColorArr)[2]->AsNumber();
      double A = (ColorArr->Num() > 3) ? (*ColorArr)[3]->AsNumber() : 1.0;
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

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("actorName"), ActorName);
  Resp->SetStringField(TEXT("parameterName"), ParameterName);
  Resp->SetStringField(TEXT("parameterType"), ParameterType);

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
    double X = 0, Y = 0, Z = 0;
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

    // Set Parameters
    NiagaraComp->SetVectorParameter(FName("User.RibbonStart"), Start);

    const TSharedPtr<FJsonObject> *EndObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("end"), EndObj) && EndObj) {
      double X = 0, Y = 0, Z = 0;
      (*EndObj)->TryGetNumberField(TEXT("x"), X);
      (*EndObj)->TryGetNumberField(TEXT("y"), Y);
      (*EndObj)->TryGetNumberField(TEXT("z"), Z);
      // Often needed to ensure the beam has an endpoint
      NiagaraComp->SetVectorParameter(FName("User.RibbonEnd"),
                                      FVector(X, Y, Z));
      NiagaraComp->SetVectorParameter(FName("User.BeamEnd"), FVector(X, Y, Z));
    }

    double Width = 10.0;
    if (Payload->TryGetNumberField(TEXT("width"), Width)) {
      NiagaraComp->SetFloatParameter(FName("User.RibbonWidth"), (float)Width);
      NiagaraComp->SetFloatParameter(FName("User.BeamWidth"), (float)Width);
    }

    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    FLinearColor ColorVal(1, 1, 1, 1);
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      double R = 1, G = 1, B = 1, A = 1;
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
      (*ColorObj)->TryGetNumberField(TEXT("a"), A);
      ColorVal = FLinearColor(R, G, B, A);
    } else {
      const TArray<TSharedPtr<FJsonValue>> *ColorArr = nullptr;
      if (Payload->TryGetArrayField(TEXT("color"), ColorArr) && ColorArr &&
          ColorArr->Num() >= 3) {
        double R = (*ColorArr)[0]->AsNumber();
        double G = (*ColorArr)[1]->AsNumber();
        double B = (*ColorArr)[2]->AsNumber();
        double A = (ColorArr->Num() > 3) ? (*ColorArr)[3]->AsNumber() : 1.0;
        ColorVal = FLinearColor(R, G, B, A);
      }
    }
    NiagaraComp->SetColorParameter(FName("User.RibbonColor"), ColorVal);
    NiagaraComp->SetColorParameter(FName("User.Color"), ColorVal);
  }

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("actorPath"), NiagaraActor->GetPathName());
  Resp->SetStringField(TEXT("actorName"), NiagaraActor->GetActorLabel());

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