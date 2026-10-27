#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetDirs.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/McpSeqPathSecurity.h"

bool UEngineWireSubsystem::HandleSequenceCreate(
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
  // A level sequence already there is kept and answered as found; any other asset there was answered the same way.
  if (McpAssetExists(FullPath)) {
    UObject *Existing = McpLoadAsset(FullPath);
    if (!Cast<ULevelSequence>(Existing)) {
      SendAutomationResponse(Socket, RequestId, false,
          FString::Printf(TEXT("%s already holds a %s, not a level sequence"), *FullPath,
                          Existing ? *Existing->GetClass()->GetName() : TEXT("asset")),
          nullptr, TEXT("ASSET_ALREADY_EXISTS"));
      return true;
    }
    VerifyAssetExists(Resp, FullPath);
    Resp->SetBoolField(TEXT("alreadyExists"), true);
    McpHandlerUtils::MarkNoAssetsChanged(Resp);
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
  Resp->SetBoolField(TEXT("saved"), McpSafeAssetSave(NewSequence));
  GCurrentSequencePath = FullPath;
  McpHandlerUtils::AddVerification(Resp, NewSequence);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence created"), Resp, FString());
  return true;
}

bool UEngineWireSubsystem::HandleSequenceOpen(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(this, RequestId, Socket, LocalPayload, TEXT("open"), MovieScene);
  if (!Sequence) {
    return true;
  }
  // Any asset at the path was opened in its own editor, and an editor that did not open still answered "opened".
  UAssetEditorSubsystem *AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
  if (!AssetEditorSS || !AssetEditorSS->OpenEditorForAsset(Sequence)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequencer did not open for this sequence"), nullptr,
                           TEXT("EDITOR_NOT_OPEN"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence opened in Sequencer"), Resp, FString());
  return true;
}
