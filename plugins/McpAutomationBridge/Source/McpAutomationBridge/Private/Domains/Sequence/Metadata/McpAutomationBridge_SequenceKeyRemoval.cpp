// Removes keys from a sequence track.
//
// Without this a track could only ever be added to. Re-authoring a camera move
// meant new keys interpolating against whatever was already there, so a track
// could not be cleanly replaced -- the practical effect was that a shot's
// framing drifted toward the old keys no matter what you wrote.
//
// Omitting `frame` and `frames` clears every key on the matching track, which is
// the operation you actually want before re-authoring; `frame` removes the keys
// on one frame and `frames` those on several. Either way the count of keys actually removed is reported,
// because "removed 0" and "removed 12" must not look the same to a caller.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneParticleSection.h"

namespace McpSequenceTracks {

namespace {

/** Delete keys on one channel family; returns how many went. */
template <typename ChannelType>
int32 RemoveChannelKeys(FMovieSceneChannelProxy &Proxy, bool bAllFrames,
                        const TArray<FFrameNumber> &TargetTicks) {
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
      if (TargetTicks.Contains(Times[Index])) {
        Data.RemoveKey(Index);
        ++Removed;
      }
    }
  }
  return Removed;
}

} // namespace

bool HandleRemoveKeyframe(UMcpAutomationBridgeSubsystem *Subsystem,
                          const FString &RequestId,
                          const TSharedPtr<FJsonObject> &LocalPayload,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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
  // frame removes the keys on one frame, frames those on each listed frame; neither clears the track. An empty
  // frames list is refused rather than read as "clear everything".
  TArray<double> FrameValues;
  double FrameValue = 0.0;
  if (LocalPayload->TryGetNumberField(TEXT("frame"), FrameValue)) {
    FrameValues.Add(FrameValue);
  }
  const TArray<TSharedPtr<FJsonValue>> *FrameList = nullptr;
  if (LocalPayload->TryGetArrayField(TEXT("frames"), FrameList) && FrameList) {
    for (const TSharedPtr<FJsonValue> &Entry : *FrameList) {
      if (!Entry.IsValid() || !Entry->TryGetNumber(FrameValue)) {
        FrameList = nullptr;
        break;
      }
      FrameValues.Add(FrameValue);
    }
    if (!FrameList || FrameList->Num() == 0) {
      Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                        TEXT("frames must list at least one frame number"), nullptr,
                                        TEXT("INVALID_FRAME"));
      return true;
    }
  }
  const bool bHasFrame = FrameValues.Num() > 0;
  TArray<FFrameNumber> TargetTicks;
  for (const double Value : FrameValues) {
    FFrameNumber TargetTick(0);
    FString FrameError;
    if (!McpSequenceFrameMath::TryTransformFrameFloor(
            Value, MovieScene->GetDisplayRate(),
            MovieScene->GetTickResolution(), TargetTick, FrameError)) {
      Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
                                        FrameError, nullptr,
                                        TEXT("INVALID_FRAME"));
      return true;
    }
    TargetTicks.Add(TargetTick);
  }

  // A removal that matches no track is a caller error, not a silent no-op, so
  // track matches are counted separately from keys removed. The shared
  // collector applies the trackName and bindingId filters the same way every
  // other track action resolves names.
  TArray<UMovieSceneTrack *> Candidates;
  CollectTracksByName(MovieScene, TrackFilter, BindingFilter, Candidates);

  int32 MatchedTracks = 0;
  int32 RemovedKeys = 0;
  for (UMovieSceneTrack *Track : Candidates) {
    ++MatchedTracks;
    Track->Modify();
    for (UMovieSceneSection *Section : Track->GetAllSections()) {
      if (!Section) {
        continue;
      }
      Section->Modify();
      FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
      // Every key family: a Visibility (bool) or particle key used to stay put under "Removed 0 key(s)".
      RemovedKeys += RemoveChannelKeys<FMovieSceneDoubleChannel>(Proxy, !bHasFrame, TargetTicks);
      RemovedKeys += RemoveChannelKeys<FMovieSceneFloatChannel>(Proxy, !bHasFrame, TargetTicks);
      RemovedKeys += RemoveChannelKeys<FMovieSceneBoolChannel>(Proxy, !bHasFrame, TargetTicks);
      RemovedKeys += RemoveChannelKeys<FMovieSceneByteChannel>(Proxy, !bHasFrame, TargetTicks);
      RemovedKeys += RemoveChannelKeys<FMovieSceneIntegerChannel>(Proxy, !bHasFrame, TargetTicks);
      RemovedKeys += RemoveChannelKeys<FMovieSceneParticleChannel>(Proxy, !bHasFrame, TargetTicks);
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
  // Modify() alone never got remove_track's change offered for saving (see
  // HandleSequenceRemoveTrack); the dirty mark is what makes the removal
  // reach disk instead of resurrecting the keys on the next editor start.
  Sequence->MarkPackageDirty();
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
}

} // namespace McpSequenceTracks
