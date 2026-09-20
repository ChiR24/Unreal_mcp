// Removes keys from a sequence track.
//
// Without this a track could only ever be added to. Re-authoring a camera move
// meant new keys interpolating against whatever was already there, so a track
// could not be cleanly replaced -- the practical effect was that a shot's
// framing drifted toward the old keys no matter what you wrote.
//
// Omitting `frame` clears every key on the matching track, which is the
// operation you actually want before re-authoring; passing `frame` removes
// just that one. Either way the count of keys actually removed is reported,
// because "removed 0" and "removed 12" must not look the same to a caller.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

#if WITH_EDITOR
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSection.h"
#endif

namespace McpSequenceTracks {

#if WITH_EDITOR
namespace {

/** Delete keys on one channel family; returns how many went. */
template <typename ChannelType>
int32 RemoveChannelKeys(FMovieSceneChannelProxy &Proxy, bool bAllFrames,
                        FFrameNumber TargetTick) {
  int32 Removed = 0;
  for (ChannelType *Channel : Proxy.GetChannels<ChannelType>()) {
    if (!Channel) {
      continue;
    }
    auto Data = Channel->GetData();
    if (bAllFrames) {
      Removed += Data.GetTimes().Num();
      Channel->Reset();
      continue;
    }
    // Walk backwards: deleting by index invalidates the indices after it.
    const TArrayView<const FFrameNumber> Times = Data.GetTimes();
    for (int32 Index = Times.Num() - 1; Index >= 0; --Index) {
      if (Times[Index] == TargetTick) {
        Data.RemoveKey(Index);
        ++Removed;
      }
    }
  }
  return Removed;
}

} // namespace
#endif

bool HandleRemoveKeyframe(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          const TSharedPtr<FJsonObject> &LocalPayload,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  const FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_remove_keyframe requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
  ULevelSequence *Sequence = LoadObject<ULevelSequence>(nullptr, *SeqPath);
  UMovieScene *MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
  if (!MovieScene) {
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                      TEXT("Level sequence not found"), nullptr,
                                      TEXT("SEQUENCE_NOT_FOUND"));
    return true;
  }

  const FString TrackFilter =
      GetJsonStringField(LocalPayload, TEXT("trackName"));
  const FString BindingFilter =
      GetJsonStringField(LocalPayload, TEXT("bindingId"));
  double FrameValue = 0.0;
  const bool bHasFrame =
      LocalPayload.IsValid() &&
      LocalPayload->TryGetNumberField(TEXT("frame"), FrameValue);

  FFrameNumber TargetTick(0);
  if (bHasFrame) {
    FString FrameError;
    if (!McpSequenceFrameMath::TryTransformFrameFloor(
            FrameValue, MovieScene->GetDisplayRate(),
            MovieScene->GetTickResolution(), TargetTick, FrameError)) {
      Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                        FrameError, nullptr,
                                        TEXT("INVALID_FRAME"));
      return true;
    }
  }

  // A removal that matches no track is a caller error, not a silent no-op, so
  // track matches are counted separately from keys removed.
  TArray<UMovieSceneTrack *> Candidates;
  for (UMovieSceneTrack *Track : MCP_GET_MOVIESCENE_TRACKS(MovieScene)) {
    if (Track) {
      Candidates.Add(Track);
    }
  }
  if (UMovieSceneTrack *CameraCutTrack = MovieScene->GetCameraCutTrack()) {
    Candidates.Add(CameraCutTrack);
  }
  for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index) {
    const FGuid Guid = MovieScene->GetPossessable(Index).GetGuid();
    if (!BindingFilter.IsEmpty() && Guid.ToString() != BindingFilter) {
      continue;
    }
    for (UMovieSceneTrack *Track :
         MovieScene->FindTracks(UMovieSceneTrack::StaticClass(), Guid)) {
      if (Track) {
        Candidates.Add(Track);
      }
    }
  }

  int32 MatchedTracks = 0;
  int32 RemovedKeys = 0;
  for (UMovieSceneTrack *Track : Candidates) {
    if (!TrackFilter.IsEmpty() && Track->GetName() != TrackFilter) {
      continue;
    }
    ++MatchedTracks;
    Track->Modify();
    for (UMovieSceneSection *Section : Track->GetAllSections()) {
      if (!Section) {
        continue;
      }
      Section->Modify();
      FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
      RemovedKeys += RemoveChannelKeys<FMovieSceneDoubleChannel>(
          Proxy, !bHasFrame, TargetTick);
      RemovedKeys += RemoveChannelKeys<FMovieSceneFloatChannel>(
          Proxy, !bHasFrame, TargetTick);
    }
  }

  if (MatchedTracks == 0) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("No track matched the supplied trackName/bindingId."), nullptr,
        TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  MovieScene->Modify();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("sequencePath"), SeqPath);
  Result->SetNumberField(TEXT("matchedTracks"), MatchedTracks);
  Result->SetNumberField(TEXT("removedKeys"), RemovedKeys);
  Result->SetBoolField(TEXT("clearedAllFrames"), !bHasFrame);
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      FString::Printf(TEXT("Removed %d key(s) from %d track(s)"), RemovedKeys,
                      MatchedTracks),
      Result);
  return true;
#else
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("sequence_remove_keyframe requires editor build."), nullptr,
      TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

} // namespace McpSequenceTracks
