#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

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
    TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
  }

  for (const FMovieSceneBinding &Binding :
       const_cast<const UMovieScene *>(MovieScene)->GetBindings()) {
    FString BindingName;
    if (FMovieScenePossessable *Possessable =
            MovieScene->FindPossessable(Binding.GetObjectGuid())) {
      BindingName = Possessable->GetName();
    } else if (FMovieSceneSpawnable *Spawnable =
                   MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
      BindingName = Spawnable->GetName();
    }

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
      TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("tracks"), TracksArray);
  Resp->SetNumberField(TEXT("trackCount"), TracksArray.Num());
  Resp->SetStringField(TEXT("sequencePath"), SeqPath);
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
