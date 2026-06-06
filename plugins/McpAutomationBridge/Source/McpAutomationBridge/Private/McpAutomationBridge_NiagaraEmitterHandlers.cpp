#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_NiagaraHandlersPrivate.h"

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
  const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

  if (!FModuleManager::Get().IsModuleLoaded(TEXT("Niagara"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Niagara plugin module is not loaded. Please "
                             "enable and restart the editor."),
                        TEXT("DEPENDENCY_MISSING"));
    return true;
  }

  FString PackagePath = SavePath;
  const FString AssetName = EmitterName;

  if (!PackagePath.EndsWith(TEXT("/"))) {
    PackagePath += TEXT("/");
  }
  const FString FullPath = PackagePath + AssetName;
  const FString ActualPackagePath =
      FPackageName::ObjectPathToPackageName(FullPath);

  UPackage *Package = CreatePackage(*ActualPackagePath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"),
                        TEXT("PACKAGE_ERROR"));
    return true;
  }

  UNiagaraEmitter *NiagaraEmitter =
      NewObject<UNiagaraEmitter>(Package, FName(*AssetName),
                                 RF_Public | RF_Standalone | RF_Transactional);

  if (!NiagaraEmitter) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara emitter"),
                        TEXT("CREATE_FAILED"));
    return true;
  }

#if MCP_HAS_NIAGARA_EMITTER_FACTORY_NEW
  if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor"))) {
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
  }
  UNiagaraEmitterFactoryNew::InitializeEmitter(NiagaraEmitter, true);
  NiagaraEmitter->SetUniqueEmitterName(EmitterName);
#else
  UNiagaraScriptSource *EmitterSource =
      NewObject<UNiagaraScriptSource>(NiagaraEmitter, NAME_None,
                                      RF_Transactional);
  if (EmitterSource) {
    UNiagaraGraph *EmitterGraph =
        NewObject<UNiagaraGraph>(EmitterSource, NAME_None, RF_Transactional);
    EmitterSource->NodeGraph = EmitterGraph;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
    NiagaraEmitter->GraphSource = EmitterSource;

    if (NiagaraEmitter->SpawnScriptProps.Script) {
      NiagaraEmitter->SpawnScriptProps.Script->SetLatestSource(EmitterSource);
    }
    if (NiagaraEmitter->UpdateScriptProps.Script) {
      NiagaraEmitter->UpdateScriptProps.Script->SetLatestSource(EmitterSource);
    }
#if WITH_EDITORONLY_DATA
    if (NiagaraEmitter->EmitterSpawnScriptProps.Script) {
      NiagaraEmitter->EmitterSpawnScriptProps.Script->SetLatestSource(
          EmitterSource);
    }
    if (NiagaraEmitter->EmitterUpdateScriptProps.Script) {
      NiagaraEmitter->EmitterUpdateScriptProps.Script->SetLatestSource(
          EmitterSource);
    }
#endif
#else
    FVersionedNiagaraEmitterData *EmitterData =
        NiagaraEmitter->GetLatestEmitterData();
    if (EmitterData) {
      EmitterData->GraphSource = EmitterSource;

      if (EmitterData->SpawnScriptProps.Script) {
        EmitterData->SpawnScriptProps.Script->SetLatestSource(EmitterSource);
      }
      if (EmitterData->UpdateScriptProps.Script) {
        EmitterData->UpdateScriptProps.Script->SetLatestSource(EmitterSource);
      }
#if WITH_EDITORONLY_DATA
      if (EmitterData->EmitterSpawnScriptProps.Script) {
        EmitterData->EmitterSpawnScriptProps.Script->SetLatestSource(
            EmitterSource);
      }
      if (EmitterData->EmitterUpdateScriptProps.Script) {
        EmitterData->EmitterUpdateScriptProps.Script->SetLatestSource(
            EmitterSource);
      }
#endif
    }
#endif
  }
#endif

  FAssetRegistryModule::AssetCreated(NiagaraEmitter);
  if (bSave) {
    McpSafeAssetSave(NiagaraEmitter);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("emitterPath"), NiagaraEmitter->GetPathName());
  Resp->SetStringField(TEXT("emitterName"), EmitterName);
  McpHandlerUtils::AddVerification(Resp, NiagaraEmitter);

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
