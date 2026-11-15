#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/HandlerUtils/McpOptionalModule.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetDirs.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/McpSeqPathSecurity.h"
#include "Domains/Sequence/McpSeqFrameRate.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"
#include "Safety/McpSafeOperationsAssetDelete.h"

TSharedPtr<FJsonObject> McpSequence::MasterSequenceAsCreate(const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> Create = MakeShared<FJsonObject>(*Payload);
  FString SequencePath = GetJsonStringField(Payload, TEXT("sequencePath"), GetJsonStringField(Payload, TEXT("assetPath")));
  const FString Name = GetJsonStringField(Payload, TEXT("name"));
  SequencePath.RemoveFromEnd(TEXT("/"));
  if (!SequencePath.IsEmpty()) {
    // sequencePath ending in /name named the sequence itself, as path does for create without a name.
    if (!Name.IsEmpty() && SequencePath.EndsWith(TEXT("/") + Name, ESearchCase::IgnoreCase)) {
      Create->RemoveField(TEXT("name"));
    }
    Create->SetStringField(TEXT("path"), SequencePath);
  } else if (!Create->HasField(TEXT("path"))) {
    Create->SetStringField(TEXT("path"), TEXT("/Game/Cinematics"));
  }
  if (!Create->HasField(TEXT("frameRate"))) {
    Create->SetNumberField(TEXT("frameRate"), 24);
  }
  if (!Create->HasField(TEXT("durationFrames")) && !Create->HasField(TEXT("endFrame"))) {
    Create->SetNumberField(TEXT("durationFrames"), 240);
  }
  return Create;
}

bool UEngineWireSubsystem::HandleSequenceCreate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!McpLoadOptionalModule(TEXT("LevelSequenceEditor"))) {
    SendAutomationError(
        Socket, RequestId,
        TEXT("LevelSequenceEditor plugin is not enabled in this project. Enable the Level Sequence Editor plugin to use Sequencer features."),
        TEXT("LEVELSEQUENCEEDITOR_PLUGIN_NOT_ENABLED"));
    return true;
  }

  FString Name;
  Payload->TryGetStringField(TEXT("name"), Name);
  FString Path;
  Payload->TryGetStringField(TEXT("path"), Path);
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
    SendAutomationError(Socket, RequestId,
                        TEXT("sequence_create requires name (path is then its folder, default /Game), or a path that names the sequence: /Game/Folder/SEQ_Name."),
                        TEXT("INVALID_ARGUMENT"));
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
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("%s already holds a %s, not a level sequence"), *FullPath,
                          Existing ? *Existing->GetClass()->GetName() : TEXT("asset")),
          TEXT("ASSET_ALREADY_EXISTS"));
      return true;
    }
    VerifyAssetExists(Resp, FullPath);
    Resp->SetBoolField(TEXT("alreadyExists"), true);
    McpHandlerUtils::MarkNoAssetsChanged(Resp);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence already exists"), Resp, FString());
    return true;
  }

  // The display rate is read before anything is made, so a bad one leaves no asset to clean up.
  FFrameRate DisplayRate;
  FString RateError;
  const bool bHasRate = Payload->HasField(TEXT("frameRate"));
  if (bHasRate && !McpSequenceFrameRate::TryParse(Payload, TEXT("frameRate"), DisplayRate, RateError)) {
    SendAutomationError(Socket, RequestId, RateError, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString NameError;
  ULevelSequence *NewSequence = McpSequence::CreateSequenceAsset(Name, FPackageName::GetLongPackagePath(FullPath), NameError);
  if (!NameError.IsEmpty()) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Invalid sequence name '%s': %s"), *Name, *NameError),
                        TEXT("INVALID_NAME"));
    return true;
  }
  if (!NewSequence) {
    SendAutomationError(Socket, RequestId,
        TEXT("Failed to create the sequence asset (LevelSequenceFactoryNew unavailable or AssetTools refused)"),
        TEXT("CREATE_ASSET_FAILED"));
    return true;
  }
  // The display rate and playback range the call sends (a range field left out keeps the new sequence's own).
  UMovieScene *MovieScene = NewSequence->GetMovieScene();
  if (MovieScene && bHasRate) {
    MovieScene->SetDisplayRate(DisplayRate);
  }
  if (MovieScene && (Payload->HasField(TEXT("startFrame")) || Payload->HasField(TEXT("durationFrames")) ||
                     Payload->HasField(TEXT("endFrame")))) {
    const int32 OwnLength = McpSequenceCinematics::InnerSequenceDisplayFrames(NewSequence, MovieScene, 240);
    FString FrameError;
    if (!McpSequenceFrameMath::ValidateCinematicFrameRequest(Payload, MovieScene, FrameError, OwnLength)) {
      McpSafeOperations::McpDeleteAssetAndFile(FullPath);
      SendAutomationError(Socket, RequestId, FrameError, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    MovieScene->SetPlaybackRange(McpSequenceCinematics::GetFrame(Payload, MovieScene, TEXT("startFrame")),
                                 McpSequenceCinematics::GetDuration(Payload, MovieScene, OwnLength));
  }
  Resp->SetBoolField(TEXT("saved"), McpSafeAssetSave(NewSequence));
  McpHandlerUtils::AddVerification(Resp, NewSequence);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence created"), Resp, FString());
  return true;
}

bool UEngineWireSubsystem::HandleSequenceOpen(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(this, RequestId, Socket, Payload, TEXT("open"), MovieScene);
  if (!Sequence) {
    return true;
  }
  // Any asset at the path was opened in its own editor, and an editor that did not open still answered "opened".
  UAssetEditorSubsystem *AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS || !AssetEditorSS->OpenEditorForAsset(Sequence)) {
    SendAutomationError(Socket, RequestId, TEXT("Sequencer did not open for this sequence"),
                        TEXT("EDITOR_NOT_OPEN"));
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Sequence opened in Sequencer"), Resp, FString());
  return true;
}
