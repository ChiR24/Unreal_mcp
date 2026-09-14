#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

namespace McpSequenceTracks {
bool HandleListTrackTypes(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TArray<TSharedPtr<FJsonValue>> Types;
  Types.Add(MakeShared<FJsonValueString>(TEXT("transform")));
  Types.Add(MakeShared<FJsonValueString>(TEXT("3dtransform")));
  Types.Add(MakeShared<FJsonValueString>(TEXT("audio")));
  Types.Add(MakeShared<FJsonValueString>(TEXT("event")));

  TSet<FString> AddedNames;
  AddedNames.Add(TEXT("transform"));
  AddedNames.Add(TEXT("3dtransform"));
  AddedNames.Add(TEXT("audio"));
  AddedNames.Add(TEXT("event"));

  for (TObjectIterator<UClass> It; It; ++It) {
    if (It->IsChildOf(UMovieSceneTrack::StaticClass()) &&
        !It->HasAnyClassFlags(CLASS_Abstract) &&
        !AddedNames.Contains(It->GetName())) {
      Types.Add(MakeShared<FJsonValueString>(It->GetName()));
      AddedNames.Add(It->GetName());
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("types"), Types);
  Resp->SetNumberField(TEXT("count"), Types.Num());
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Available track types"), Resp);
  return true;
}

bool HandleListTracks(UMcpAutomationBridgeSubsystem *Subsystem,
                      const FString &RequestId,
                      const TSharedPtr<FJsonObject> &LocalPayload,
                      TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_list_tracks requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

#if WITH_EDITOR
  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  if (!Sequence) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("Level sequence not found"), nullptr,
                                      TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  UMovieScene *MovieScene = Sequence->GetMovieScene();
  if (!MovieScene) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("MovieScene not available"), nullptr,
                                      TEXT("MOVIESCENE_UNAVAILABLE"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> TracksArray;
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (!Track)
      continue;
    TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
    TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
    TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
    TrackObj->SetStringField(TEXT("displayName"),
                             Track->GetDisplayName().ToString());
    TrackObj->SetBoolField(TEXT("isMasterTrack"), true);
    TrackObj->SetNumberField(TEXT("sectionCount"),
                             Track->GetAllSections().Num());
    TrackObj->SetBoolField(TEXT("isCameraCut"),
                           Track->IsA<UMovieSceneCameraCutTrack>());
    TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
  }

  // The camera cut track is stored in its own UMovieScene member
  // (GetCameraCutTrack()), NOT in the Tracks array that GetTracks() returns —
  // so a successfully-added camera cut was invisible to this readback and a
  // caller was told "Camera cut added" only to find no such track afterwards.
  // Enumerate it explicitly.
  if (UMovieSceneTrack *CameraCutTrack = MovieScene->GetCameraCutTrack()) {
    TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
    TrackObj->SetStringField(TEXT("trackName"), CameraCutTrack->GetName());
    TrackObj->SetStringField(TEXT("trackType"),
                             CameraCutTrack->GetClass()->GetName());
    TrackObj->SetStringField(TEXT("displayName"),
                             CameraCutTrack->GetDisplayName().ToString());
    TrackObj->SetBoolField(TEXT("isMasterTrack"), true);
    TrackObj->SetNumberField(TEXT("sectionCount"),
                             CameraCutTrack->GetAllSections().Num());
    TrackObj->SetBoolField(TEXT("isCameraCut"), true);
    TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
  }

  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    FString BindingName = GetBindingName(MovieScene, Binding.GetObjectGuid());

    for (UMovieSceneTrack *Track : MCP_GET_BINDING_TRACKS(Binding)) {
      if (!Track)
        continue;
      TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
      TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
      TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
      TrackObj->SetStringField(TEXT("displayName"),
                               Track->GetDisplayName().ToString());
      TrackObj->SetBoolField(TEXT("isMasterTrack"), false);
      TrackObj->SetStringField(TEXT("bindingName"), BindingName);
      TrackObj->SetStringField(TEXT("bindingGuid"),
                               Binding.GetObjectGuid().ToString());
      TrackObj->SetNumberField(TEXT("sectionCount"),
                               Track->GetAllSections().Num());
      TrackObj->SetBoolField(TEXT("isCameraCut"),
                             Track->IsA<UMovieSceneCameraCutTrack>());
      TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("tracks"), TracksArray);
  Resp->SetNumberField(TEXT("trackCount"), TracksArray.Num());
  // Echo the canonical package path rather than whatever spelling the caller
  // passed in: `/Game/X.X` and `/Game/X` are the same asset, and a caller that
  // feeds this value back should not have the two forms alternate.
  Resp->SetStringField(TEXT("sequencePath"),
                       Sequence->GetOutermost()
                           ? Sequence->GetOutermost()->GetName()
                           : SeqPath);
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Found %d tracks"), TracksArray.Num()), Resp,
      FString());
  return true;
#else
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("sequence_list_tracks requires editor build"),
                                    nullptr, TEXT("EDITOR_ONLY"));
  return true;
#endif
}
}
