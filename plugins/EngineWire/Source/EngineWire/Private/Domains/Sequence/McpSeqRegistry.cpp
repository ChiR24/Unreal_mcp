#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"

FString GCurrentSequencePath;

FString McpSequence::ResolvePath(const TSharedPtr<FJsonObject> &Payload) {
  FString Path;
  if (Payload.IsValid()) {
    for (const TCHAR *Field :
         {TEXT("path"), TEXT("sequencePath"), TEXT("assetPath")}) {
      if (Payload->TryGetStringField(Field, Path) && !Path.IsEmpty())
        break;
      Path.Reset();
    }
  }
  if (!Path.IsEmpty()) {
    if (McpAssetExists(Path)) {
      UObject *Obj = McpLoadAsset(Path);
      if (Obj) {
        return Obj->GetPathName();
      }
    }
    return Path;
  }
  if (!GCurrentSequencePath.IsEmpty())
    return GCurrentSequencePath;
  return FString();
}

FString McpSequence::ResolvePathOrReply(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                        TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                                        const TCHAR *Action) {
  const FString Path = ResolvePath(Payload);
  if (Path.IsEmpty()) {
    Subsystem->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("%s requires a sequence path"), Action), TEXT("INVALID_SEQUENCE"));
  }
  return Path;
}

UObject *McpSequence::LoadAssetOrReply(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                                       const TCHAR *Action, FString &OutPath) {
  OutPath = ResolvePathOrReply(Subsystem, RequestId, Socket, Payload, Action);
  if (OutPath.IsEmpty()) {
    return nullptr;
  }
  UObject *Asset = McpLoadAsset(OutPath);
  if (!Asset) {
    Subsystem->SendAutomationError(Socket, RequestId, TEXT("Sequence not found"),
                                   TEXT("INVALID_SEQUENCE"));
  }
  return Asset;
}

ULevelSequence *McpSequence::LoadOrReply(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject> &Payload,
                                         const TCHAR *Action, UMovieScene *&OutMovieScene) {
  OutMovieScene = nullptr;
  const FString Path = ResolvePathOrReply(Subsystem, RequestId, Socket, Payload, Action);
  if (Path.IsEmpty()) {
    return nullptr;
  }
  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
  if (!Sequence) {
    Subsystem->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Level sequence not found: %s"), *Path), TEXT("INVALID_SEQUENCE"));
    return nullptr;
  }
  OutMovieScene = Sequence->GetMovieScene();
  if (!OutMovieScene) {
    Subsystem->SendAutomationError(Socket, RequestId,
        TEXT("MovieScene not available"), TEXT("MOVIESCENE_UNAVAILABLE"));
    return nullptr;
  }
  return Sequence;
}

void McpSequence::ReplyTrackMiss(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                 TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &TrackName,
                                 const FString &Ambiguous) {
  if (Ambiguous.IsEmpty()) {
    Subsystem->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("No track named '%s'; list_tracks gives each track's trackName and displayName"), *TrackName),
        TEXT("TRACK_NOT_FOUND"));
    return;
  }
  Subsystem->SendAutomationError(Socket, RequestId,
      FString::Printf(TEXT("'%s' fits several tracks: %s. Pass one trackName exactly as list_tracks gives it"),
                      *TrackName, *Ambiguous),
      TEXT("AMBIGUOUS_TRACK"));
}

ULevelSequence *McpSequence::CreateSequenceAsset(const FString &Name, const FString &Folder, FString &OutNameError) {
  if (!McpAssetToolsCanCreate(Folder, Name, OutNameError)) {
    return nullptr;
  }
  UClass *FactoryClass = LoadClass<UFactory>(nullptr, TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
  if (!FactoryClass) {
    return nullptr;
  }
  UFactory *Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
  return Cast<ULevelSequence>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
                                  .Get()
                                  .CreateAsset(Name, Folder, ULevelSequence::StaticClass(), Factory));
}

FString UEngineWireSubsystem::ResolveSequencePath(
    const TSharedPtr<FJsonObject> &Payload) {
  return McpSequence::ResolvePath(Payload);
}

