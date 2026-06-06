#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceTracks {
bool HandleAddTrack(UMcpAutomationBridgeSubsystem *Subsystem,
                    const FString &RequestId,
                    const TSharedPtr<FJsonObject> &LocalPayload,
                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_add_track requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }

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

  FGuid BindingGuid;
  if (!ActorName.IsEmpty()) {
    const UMovieScene *ConstMovieScene = MovieScene;
    for (const FMovieSceneBinding &Binding : ConstMovieScene->GetBindings()) {
      FString BindingName;
      if (FMovieScenePossessable *Possessable =
              MovieScene->FindPossessable(Binding.GetObjectGuid())) {
        BindingName = Possessable->GetName();
      } else if (FMovieSceneSpawnable *Spawnable =
                     MovieScene->FindSpawnable(Binding.GetObjectGuid())) {
        BindingName = Spawnable->GetName();
      }

      if (BindingName.Contains(ActorName)) {
        BindingGuid = Binding.GetObjectGuid();
        break;
      }
    }
    if (!BindingGuid.IsValid()) {
      Subsystem->SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Binding not found for actor: %s"), *ActorName),
          nullptr, TEXT("BINDING_NOT_FOUND"));
      return true;
    }
  }

  UMovieSceneTrack *NewTrack = nullptr;
  UClass *TrackClass = ResolveUClass(TrackType);
  if (!TrackClass) {
    TrackClass = ResolveUClass(
        FString::Printf(TEXT("UMovieScene%sTrack"), *TrackType));
  }
  if (!TrackClass) {
    TrackClass =
        ResolveUClass(FString::Printf(TEXT("MovieScene%sTrack"), *TrackType));
  }
  if (!TrackClass) {
    TrackClass = ResolveUClass(FString::Printf(TEXT("U%s"), *TrackType));
  }

  if (TrackClass && TrackClass->IsChildOf(UMovieSceneTrack::StaticClass())) {
    if (BindingGuid.IsValid()) {
      NewTrack = MovieScene->AddTrack(TrackClass, BindingGuid);
    } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      NewTrack = MovieScene->AddTrack(TrackClass);
#else
      Subsystem->SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Adding tracks without binding is not supported in UE 5.0. Please provide an actor or object binding."),
          TEXT("NOT_SUPPORTED"));
      return true;
#endif
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
    Resp->SetStringField(TEXT("trackType"), TrackType);
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
