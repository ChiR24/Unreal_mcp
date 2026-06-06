#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!FModuleManager::Get().IsModuleLoaded(TEXT("LevelSequenceEditor"))) {
    if (!FModuleManager::Get().ModuleExists(TEXT("LevelSequenceEditor")) ||
        !FModuleManager::Get().LoadModule(TEXT("LevelSequenceEditor"))) {
      SendAutomationError(
          Socket, RequestId,
          TEXT("LevelSequenceEditor plugin is not enabled in this project. Enable the Level Sequence Editor plugin to use Sequencer features."),
          TEXT("LEVELSEQUENCEEDITOR_PLUGIN_NOT_ENABLED"));
      return true;
    }
  }

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Name.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_create requires name"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString FullPath = Path.IsEmpty()
                         ? FString::Printf(TEXT("/Game/%s"), *Name)
                         : FString::Printf(TEXT("%s/%s"), *Path, *Name);

  FString DestFolder = Path.IsEmpty() ? TEXT("/Game") : Path;
  if (DestFolder.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
    DestFolder = FString::Printf(TEXT("/Game%s"), *DestFolder.RightChop(8));
  }

  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceCreate: Handing RequestID=%s Path=%s"),
         *RequestIdArg, *FullPath);

  if (UEditorAssetLibrary::DoesAssetExist(FullPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    VerifyAssetExists(Resp, FullPath);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("HandleSequenceCreate: Sequence exists, sending response for "
                "RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                      TEXT("Sequence already exists"), Resp,
                                      FString());
    return true;
  }

  UClass *FactoryClass = FindObject<UClass>(
      nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
  if (!FactoryClass)
    FactoryClass = LoadClass<UClass>(
        nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));

  if (FactoryClass) {
    UFactory *Factory =
        NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools"));
    UObject *NewObj = AssetToolsModule.Get().CreateAsset(
        Name, DestFolder, ULevelSequence::StaticClass(), Factory);
    if (NewObj) {
      McpSafeAssetSave(NewObj);
      GCurrentSequencePath = FullPath;
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(Resp, NewObj);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleSequenceCreate: Created sequence, sending response "
                  "for RequestID=%s"),
             *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                        TEXT("Sequence created"), Resp,
                                        FString());
    } else {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Error,
          TEXT("HandleSequenceCreate: Failed to create asset for RequestID=%s"),
          *RequestIdArg);
      Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                        TEXT("Failed to create sequence asset"),
                                        nullptr, TEXT("CREATE_ASSET_FAILED"));
    }
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("HandleSequenceCreate: Factory not found for RequestID=%s"),
           *RequestIdArg);
    Subsystem->SendAutomationResponse(
        Socket, RequestIdArg, false,
        TEXT("LevelSequenceFactoryNew class not found (Module not loaded?)"),
        nullptr, TEXT("FACTORY_NOT_AVAILABLE"));
  }
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_create requires editor build"), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceOpen(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_open requires a sequence path"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  FString RequestIdArg = RequestId;
  UMcpAutomationBridgeSubsystem *Subsystem = this;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opening sequence %s for RequestID=%s"),
         *SeqPath, *RequestIdArg);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestIdArg, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if MCP_HAS_LEVELSEQUENCE_EDITOR_SUBSYSTEM
  if (ULevelSequence *LevelSeq = Cast<ULevelSequence>(SeqObj)) {
    if (GEditor) {
      if (ULevelSequenceEditorSubsystem *LSES =
              GEditor->GetEditorSubsystem<ULevelSequenceEditorSubsystem>()) {
        if (UAssetEditorSubsystem *AssetEditorSS =
                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
          AssetEditorSS->OpenEditorForAsset(LevelSeq);
          Resp->SetStringField(TEXT("sequencePath"), SeqPath);
          Resp->SetStringField(TEXT("message"), TEXT("Sequence opened"));
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("HandleSequenceOpen: Successfully opened in LSES, "
                      "sending response for RequestID=%s"),
                 *RequestIdArg);
          Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                            TEXT("Sequence opened"), Resp,
                                            FString());
          return true;
        }
      }
    }
  }
#endif

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      AssetEditorSS->OpenEditorForAsset(SeqObj);
    }
  }
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Resp->SetStringField(TEXT("message"), TEXT("Sequence opened (asset editor)"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleSequenceOpen: Opened via AssetEditorSS, sending response "
              "for RequestID=%s"),
         *RequestIdArg);
  Subsystem->SendAutomationResponse(Socket, RequestIdArg, true,
                                    TEXT("Sequence opened"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_open requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
