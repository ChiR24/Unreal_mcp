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
#include "Domains/Sequence/McpSeqFrameRate.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"
#include "Foundation/McpScopedEditorTransaction.h"

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

bool HandleRemoveKeyframe(UEngineWireSubsystem *Subsystem,
                          const FString &RequestId,
                          const TSharedPtr<FJsonObject> &LocalPayload,
                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(Subsystem, RequestId, RequestingSocket, LocalPayload,
                                                      TEXT("sequence_remove_keyframe"), MovieScene);
  if (!Sequence) {
    return true;
  }

  const FString TrackFilter =
      GetJsonStringField(LocalPayload, TEXT("trackName"));
  const FString BindingFilter =
      GetJsonStringField(LocalPayload, TEXT("bindingId"));
  // With neither filter every track matched, so a call naming only the sequence cleared every key in it.
  if (TrackFilter.IsEmpty() && BindingFilter.IsEmpty()) {
    Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                   TEXT("Name the track (trackName) or the binding (bindingId) whose keys go; nothing was removed."),
                                   TEXT("TRACK_REQUIRED"));
    return true;
  }
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
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     TEXT("frames must list at least one frame number"),
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
      Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                     FrameError,
                                     TEXT("INVALID_FRAME"));
      return true;
    }
    TargetTicks.Add(TargetTick);
  }

  // A removal that matches no track is a caller error, not a silent no-op, and
  // is refused before the undo step opens, so it leaves none. The shared
  // collector applies the trackName and bindingId filters the same way every
  // other track action resolves names.
  TArray<UMovieSceneTrack *> Candidates;
  CollectTracksByName(MovieScene, TrackFilter, BindingFilter, Candidates);
  if (Candidates.Num() == 0) {
    Subsystem->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("No track matched the supplied trackName/bindingId."),
        TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // No transaction was open, so the Modify() calls below recorded nothing and the removal had no undo.
  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Sequence Keys")),
                                          EMcpMutationDurability::EditorStateOnly, {Sequence, MovieScene});
  const int32 MatchedTracks = Candidates.Num();
  int32 RemovedKeys = 0;
  for (UMovieSceneTrack *Track : Candidates) {
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
