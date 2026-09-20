// Reads the keys already on a sequence's tracks.
//
// Until this existed the sequence surface was write-only: you could add a
// keyframe but never see one. That is how an add_keyframe that silently did
// nothing survived -- a section created by FindOrAddSection carries an EMPTY
// range, keys inside it cover no time, and no capability could show it. The
// per-section `rangeIsEmpty` flag below is the readback that makes that
// visible, and it is why frames are reported in DISPLAY units: every other
// keyframe capability speaks display frames, so a readback in ticks would be
// its own trap.

#include "Core/Compatibility/McpVersionCompatibility.h"
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

/** Ticks to DISPLAY frames -- the unit every keyframe capability speaks in. */
double TickToDisplayFrame(const UMovieScene *MovieScene, FFrameNumber Tick) {
  return FFrameRate::TransformTime(FFrameTime(Tick),
                                   MovieScene->GetTickResolution(),
                                   MovieScene->GetDisplayRate())
      .AsDecimal();
}

/** Append one channel family's key times and values to OutChannels. */
template <typename ChannelType>
void DescribeChannels(const UMovieScene *MovieScene,
                      FMovieSceneChannelProxy &Proxy, const TCHAR *TypeName,
                      TArray<TSharedPtr<FJsonValue>> &OutChannels) {
  TArrayView<ChannelType *> Channels = Proxy.GetChannels<ChannelType>();
  TArrayView<const FMovieSceneChannelMetaData> Meta =
      Proxy.GetMetaData<ChannelType>();
  for (int32 Index = 0; Index < Channels.Num(); ++Index) {
    ChannelType *Channel = Channels[Index];
    if (!Channel) {
      continue;
    }
    TSharedPtr<FJsonObject> ChannelObj = McpHandlerUtils::CreateResultObject();
    ChannelObj->SetNumberField(TEXT("channelIndex"), Index);
    ChannelObj->SetStringField(TEXT("channelType"), TypeName);
    if (Meta.IsValidIndex(Index)) {
      ChannelObj->SetStringField(TEXT("channelName"),
                                 Meta[Index].Name.ToString());
    }
    TArray<TSharedPtr<FJsonValue>> KeysArray;
    const TArrayView<const FFrameNumber> Times = Channel->GetData().GetTimes();
    const auto Values = Channel->GetData().GetValues();
    for (int32 KeyIndex = 0; KeyIndex < Times.Num(); ++KeyIndex) {
      TSharedPtr<FJsonObject> KeyObj = McpHandlerUtils::CreateResultObject();
      KeyObj->SetNumberField(TEXT("frame"),
                             TickToDisplayFrame(MovieScene, Times[KeyIndex]));
      if (Values.IsValidIndex(KeyIndex)) {
        KeyObj->SetNumberField(TEXT("value"), Values[KeyIndex].Value);
      }
      KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
    }
    ChannelObj->SetNumberField(TEXT("keyCount"), KeysArray.Num());
    ChannelObj->SetArrayField(TEXT("keys"), KeysArray);
    OutChannels.Add(MakeShared<FJsonValueObject>(ChannelObj));
  }
}

/** One section: its range plus every channel's keys. */
TSharedPtr<FJsonObject> DescribeSectionKeys(const UMovieScene *MovieScene,
                                            UMovieSceneSection *Section,
                                            int32 &OutKeyCount) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetStringField(TEXT("sectionName"), Section->GetName());
  const TRange<FFrameNumber> Range = Section->GetRange();
  Obj->SetBoolField(TEXT("rangeIsEmpty"), Range.IsEmpty());
  if (!Range.IsEmpty()) {
    if (Range.GetLowerBound().IsClosed()) {
      Obj->SetNumberField(
          TEXT("startFrame"),
          TickToDisplayFrame(MovieScene, Range.GetLowerBoundValue()));
    }
    if (Range.GetUpperBound().IsClosed()) {
      Obj->SetNumberField(
          TEXT("endFrame"),
          TickToDisplayFrame(MovieScene, Range.GetUpperBoundValue()));
    }
  }
  TArray<TSharedPtr<FJsonValue>> ChannelsArray;
  FMovieSceneChannelProxy &Proxy = Section->GetChannelProxy();
  DescribeChannels<FMovieSceneDoubleChannel>(MovieScene, Proxy, TEXT("double"),
                                             ChannelsArray);
  DescribeChannels<FMovieSceneFloatChannel>(MovieScene, Proxy, TEXT("float"),
                                            ChannelsArray);
  for (const TSharedPtr<FJsonValue> &ChannelValue : ChannelsArray) {
    if (ChannelValue.IsValid() && ChannelValue->AsObject().IsValid()) {
      OutKeyCount += static_cast<int32>(
          ChannelValue->AsObject()->GetNumberField(TEXT("keyCount")));
    }
  }
  Obj->SetArrayField(TEXT("channels"), ChannelsArray);
  return Obj;
}

} // namespace
#endif

bool HandleListTrackKeys(UMcpAutomationBridgeSubsystem *Subsystem,
                         const FString &RequestId,
                         const TSharedPtr<FJsonObject> &LocalPayload,
                         TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  const FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("sequence_list_track_keys requires a sequence path"), nullptr,
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
  // Same collector as remove_keyframe: substring name match, and bindings from
  // GetBindings() so spawnable-bound tracks are listed too.
  TArray<UMovieSceneTrack *> Tracks;
  CollectTracksByName(MovieScene, TrackFilter, FString(), Tracks);

  TArray<TSharedPtr<FJsonValue>> TracksArray;
  int32 TotalKeys = 0;
  for (UMovieSceneTrack *Track : Tracks) {
    TSharedPtr<FJsonObject> TrackObj = McpHandlerUtils::CreateResultObject();
    TrackObj->SetStringField(TEXT("trackName"), Track->GetName());
    TrackObj->SetStringField(TEXT("trackType"), Track->GetClass()->GetName());
    TArray<TSharedPtr<FJsonValue>> SectionsArray;
    for (UMovieSceneSection *Section : Track->GetAllSections()) {
      if (!Section) {
        continue;
      }
      SectionsArray.Add(MakeShared<FJsonValueObject>(
          DescribeSectionKeys(MovieScene, Section, TotalKeys)));
    }
    TrackObj->SetArrayField(TEXT("sections"), SectionsArray);
    TracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("sequencePath"), SeqPath);
  Result->SetArrayField(TEXT("trackKeys"), TracksArray);
  Result->SetNumberField(TEXT("trackCount"), TracksArray.Num());
  Result->SetNumberField(TEXT("keyCount"), TotalKeys);
  Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                                    TEXT("Track keys listed"), Result);
  return true;
#else
  Subsystem->SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("sequence_list_track_keys requires editor build."), nullptr,
      TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

} // namespace McpSequenceTracks
