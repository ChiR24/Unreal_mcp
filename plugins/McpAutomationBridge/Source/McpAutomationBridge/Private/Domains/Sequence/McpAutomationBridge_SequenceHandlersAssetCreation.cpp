#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetDirectories.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
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
  FString Folder = Path.IsEmpty() ? TEXT("/Game") : Path;
  McpAssetPathCanonical::MapContentRootInline(Folder);
  // Without a name, a path that is no folder names the sequence itself, the way every other sequence action reads path.
  if (Name.IsEmpty() && !Path.IsEmpty()) {
    const FString Probe = SanitizeProjectRelativePath(Folder);
    if (!Probe.IsEmpty() && !DoesAssetDirectoryExistOnDisk(Probe)) {
      Name = FPackageName::GetShortName(Probe);
      Folder = FPackageName::GetLongPackagePath(Probe);
    }
  }
  if (Name.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_create requires name (path is then its folder, default /Game), or a path that names the sequence: /Game/Folder/SEQ_Name."),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // Sanitize (accepts the slashless Game/... alias), then the writable-path check
  // create_master_sequence applies; this passed the raw path to AssetTools.
  FString FullPath;
  FString PathError;
  if (!McpSequencePathSecurity::ValidateWritableAssetPath(SanitizeProjectRelativePath(Folder / Name), FullPath, PathError)) {
    SendAutomationResponse(Socket, RequestId, false, PathError, nullptr, TEXT("SEQUENCE_PATH_NOT_WRITABLE"));
    return true;
  }

  // sequence.create declares `sequencePath` as a REQUIRED output field; the gateway projects the
  // result to schema-declared names, so it must be present on every success.
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("sequencePath"), FullPath);
  if (McpAssetExists(FullPath)) {
    VerifyAssetExists(Resp, FullPath);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence already exists"), Resp, FString());
    return true;
  }

  ULevelSequence *NewSequence = McpSequence::CreateSequenceAsset(Name, FPackageName::GetLongPackagePath(FullPath));
  if (!NewSequence) {
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Failed to create the sequence asset (LevelSequenceFactoryNew unavailable or AssetTools refused)"),
        nullptr, TEXT("CREATE_ASSET_FAILED"));
    return true;
  }
  McpSafeAssetSave(NewSequence);
  GCurrentSequencePath = FullPath;
  McpHandlerUtils::AddVerification(Resp, NewSequence);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence created"), Resp, FString());
  return true;
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }

  if (GEditor) {
    if (UAssetEditorSubsystem *AssetEditorSS =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()) {
      AssetEditorSS->OpenEditorForAsset(SeqObj);
    }
  }
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
  Resp->SetStringField(TEXT("message"), TEXT("Sequence opened (asset editor)"));
  SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Sequence opened"), Resp, FString());
  return true;
}
