#include "Core/Compatibility/McpVersionCompatibility.h"
#include "MovieSceneNameableTrack.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceTracks {
bool HandleAddTrack(UMcpAutomationBridgeSubsystem *Subsystem,
                    const FString &RequestId,
                    const TSharedPtr<FJsonObject> &LocalPayload,
                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(Subsystem, RequestId, RequestingSocket, LocalPayload, TEXT("add_track"), MovieScene);
  if (!Sequence) {
    return true;
  }
  const FString SeqPath = Sequence->GetPathName();

  FString TrackType;
  LocalPayload->TryGetStringField(TEXT("trackType"), TrackType);
  if (TrackType.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("trackType required (e.g., Transform, Animation, Audio, Event)"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString TrackName;
  LocalPayload->TryGetStringField(TEXT("trackName"), TrackName);

  FString ActorName;
  LocalPayload->TryGetStringField(TEXT("actorName"), ActorName);

  const FGuid BindingGuid =
      McpSequenceKeyframes::ResolveBindingGuid(MovieScene, FString(), ActorName);
  if (!ActorName.IsEmpty()) {
    if (!BindingGuid.IsValid()) {
      Subsystem->SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Binding not found for actor: %s"), *ActorName),
          nullptr, TEXT("BINDING_NOT_FOUND"));
      return true;
    }
  }

  UMovieSceneTrack *NewTrack = nullptr;
  // "transform" resolved to UMovieSceneTransformTrack, the property-track base,
  // which reports sectionCount 0 and can never hold a section -- so the caller
  // got a dead track and add_keyframe then created a second, correct
  // MovieScene3DTransformTrack beside it. Route the friendly alias to the one
  // that actually animates a bound actor.
  FString ResolvedTrackType = TrackType;
  if (ResolvedTrackType.Equals(TEXT("transform"), ESearchCase::IgnoreCase)) {
    ResolvedTrackType = TEXT("MovieScene3DTransformTrack");
  }
  // UClass names carry no U prefix, so "Audio" and "MovieSceneAudioTrack" are the spellings that can match.
  UClass *TrackClass = ResolveUClass(ResolvedTrackType);
  if (!TrackClass) {
    TrackClass = ResolveUClass(TEXT("MovieScene") + ResolvedTrackType + TEXT("Track"));
  }

  if (TrackClass && TrackClass->IsChildOf(UMovieSceneTrack::StaticClass())) {
    if (BindingGuid.IsValid()) {
      NewTrack = MovieScene->AddTrack(TrackClass, BindingGuid);
    } else {
      // An unbound track (an Audio track for cutscene music) is a master track before 5.2.
      NewTrack = MCP_ADD_MOVIESCENE_TRACK(MovieScene, TrackClass);
    }
  } else if (TrackClass) {
    Subsystem->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Class '%s' is not a UMovieSceneTrack"),
                        *TrackClass->GetName()),
        TEXT("INVALID_CLASS_TYPE"));
    return true;
  }

  if (NewTrack) {
    Sequence->MarkPackageDirty();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("sequencePath"), SeqPath);
    // Honour trackName (dogfood #124): callers address tracks by the display name they chose.
    if (UMovieSceneNameableTrack *Nameable = (NewTrack && !TrackName.IsEmpty()) ? Cast<UMovieSceneNameableTrack>(NewTrack) : nullptr)
    {
      Nameable->SetDisplayName(FText::FromString(TrackName));
    }
    Resp->SetStringField(TEXT("trackType"), TrackType);
    if (NewTrack) {
      Resp->SetStringField(TEXT("trackId"), NewTrack->GetName()); // dogfood #124: addressable id
      Resp->SetStringField(TEXT("trackClass"), NewTrack->GetClass()->GetName());
      Resp->SetStringField(TEXT("trackPath"), NewTrack->GetPathName());
    }
    Resp->SetStringField(TEXT("trackName"),
                         TrackName.IsEmpty() ? TrackType : TrackName);
    if (!ActorName.IsEmpty()) {
      Resp->SetStringField(TEXT("actorName"), ActorName);
      Resp->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
    }
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                      TEXT("Track added successfully"), Resp,
                                      FString());
  } else {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Failed to add track of type: %s"), *TrackType),
        nullptr, TEXT("TRACK_CREATION_FAILED"));
  }
  return true;
}
}
