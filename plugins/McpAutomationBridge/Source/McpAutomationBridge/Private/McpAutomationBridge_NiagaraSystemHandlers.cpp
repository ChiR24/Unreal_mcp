#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_NiagaraHandlersPrivate.h"

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
  const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

  if (!FModuleManager::Get().IsModuleLoaded(TEXT("Niagara"))) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Niagara plugin module is not loaded. Please "
                             "enable and restart the editor."),
                        TEXT("DEPENDENCY_MISSING"));
    return true;
  }

  FString PackagePath = SavePath;
  const FString AssetName = SystemName;

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

  UNiagaraSystem *NiagaraSystem =
      NewObject<UNiagaraSystem>(Package, FName(*AssetName),
                                RF_Public | RF_Standalone | RF_Transactional);
  if (NiagaraSystem) {
#if MCP_HAS_NIAGARA_SYSTEM_FACTORY_NEW
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("NiagaraEditor"))) {
      FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    }
    UNiagaraSystemFactoryNew::InitializeSystem(NiagaraSystem, false);
#else
    UNiagaraScript *SystemSpawnScript = NiagaraSystem->GetSystemSpawnScript();
    UNiagaraScript *SystemUpdateScript = NiagaraSystem->GetSystemUpdateScript();
    UNiagaraScriptSource *SystemScriptSource =
        NewObject<UNiagaraScriptSource>(SystemSpawnScript,
                                        TEXT("SystemScriptSource"),
                                        RF_Transactional);
    if (SystemScriptSource) {
      UNiagaraGraph *SystemGraph =
          NewObject<UNiagaraGraph>(SystemScriptSource,
                                   TEXT("SystemScriptGraph"),
                                   RF_Transactional);
      SystemScriptSource->NodeGraph = SystemGraph;

      if (SystemSpawnScript) {
        SystemSpawnScript->SetLatestSource(SystemScriptSource);
      }
      if (SystemUpdateScript) {
        SystemUpdateScript->SetLatestSource(SystemScriptSource);
      }
    }
#endif
    NiagaraSystem->RequestCompile(false);
  }

  if (!NiagaraSystem) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create Niagara system"),
                        TEXT("CREATE_FAILED"));
    return true;
  }

  FAssetRegistryModule::AssetCreated(NiagaraSystem);
  if (bSave) {
    McpSafeAssetSave(NiagaraSystem);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("systemPath"), NiagaraSystem->GetPathName());
  Resp->SetStringField(TEXT("systemName"), SystemName);
  McpHandlerUtils::AddVerification(Resp, NiagaraSystem);

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
