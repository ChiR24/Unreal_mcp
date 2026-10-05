#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

namespace McpSequenceTracks {
bool HandleListTrackTypes(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // add_track's short names first, then every concrete track class.
  TArray<FString> Names = {TEXT("transform"), TEXT("3dtransform"), TEXT("audio"), TEXT("event")};
  for (TObjectIterator<UClass> It; It; ++It) {
    if (It->IsChildOf(UMovieSceneTrack::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
      Names.AddUnique(It->GetName());
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("types"), McpHandlerUtils::ToJsonStringArray(Names));
  Resp->SetNumberField(TEXT("count"), Names.Num());
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Available track types"), Resp);
  return true;
}

bool HandleListTracks(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(Subsystem, RequestId, RequestingSocket, LocalPayload, TEXT("list_tracks"), MovieScene);
  if (!Sequence) {
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> TracksArray;
  // Binding is null for a master track.
  auto AddTrack = [&](UMovieSceneTrack *Track, const FMovieSceneBinding *Binding) {
    if (!Track)
      return;
    TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
    TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
    TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
    TrackObj->SetStringField(TEXT("displayName"), Track->GetDisplayName().ToString());
    TrackObj->SetBoolField(TEXT("isMasterTrack"), Binding == nullptr);
    if (Binding) {
      TrackObj->SetStringField(TEXT("bindingName"), GetBindingLabel(MovieScene, Binding->GetObjectGuid()));
      TrackObj->SetStringField(TEXT("bindingGuid"), Binding->GetObjectGuid().ToString());
    }
    TrackObj->SetNumberField(TEXT("sectionCount"), Track->GetAllSections().Num());
    TrackObj->SetBoolField(TEXT("isCameraCut"), Track->IsA<UMovieSceneCameraCutTrack>());
    TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
  };
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene))
    AddTrack(Track, nullptr);
  // The camera cut track lives in its own UMovieScene member, not in GetTracks(),
  // so an added camera cut was invisible to this readback until listed explicitly.
  AddTrack(MovieScene->GetCameraCutTrack(), nullptr);
  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding))
      AddTrack(Track, &Binding);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("tracks"), TracksArray);
  Resp->SetNumberField(TEXT("trackCount"), TracksArray.Num());
  // Echo the canonical package path rather than whatever spelling the caller
  // passed in: `/Game/X.X` and `/Game/X` are the same asset, and a caller that
  // feeds this value back should not have the two forms alternate.
  Resp->SetStringField(TEXT("sequencePath"), Sequence->GetOutermost()->GetName());
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Found %d tracks"), TracksArray.Num()), Resp,
      FString());
  return true;
}
}
