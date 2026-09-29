#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"
#include "Misc/PackageName.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sound/SoundBase.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceAddSection(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TrackName;
  Payload->TryGetStringField(TEXT("trackName"), TrackName);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  double StartFrame = 0.0, EndFrame = 100.0;
  // The contract declares these as `start`/`end`; only the longer spellings
  // were read, so a caller following the schema silently got the 0-100 default
  // range instead of the one they asked for. Accept both.
  if (!Payload->TryGetNumberField(TEXT("startFrame"), StartFrame)) {
    Payload->TryGetNumberField(TEXT("start"), StartFrame);
  }
  const bool bHasEnd = Payload->TryGetNumberField(TEXT("endFrame"), EndFrame) ||
                       Payload->TryGetNumberField(TEXT("end"), EndFrame);
  FString BindingId;
  Payload->TryGetStringField(TEXT("bindingId"), BindingId);
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(this, RequestId, Socket, Payload, TEXT("add_section"), MovieScene);
  if (!Sequence) {
    return true;
  }
  // start/end are display-rate frames, like every other frame this tool takes;
  // the section range lives in tick resolution.
  FFrameNumber Start;
  FFrameNumber End;
  FString FrameError;
  if (!McpSequenceFrameMath::TryTransformFrame(StartFrame, MovieScene->GetDisplayRate(),
                                               MovieScene->GetTickResolution(), Start, FrameError) ||
      !McpSequenceFrameMath::TryTransformFrame(EndFrame, MovieScene->GetDisplayRate(),
                                               MovieScene->GetTickResolution(), End, FrameError)) {
    SendAutomationResponse(Socket, RequestId, false, FrameError, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // A sound with no end takes its length from the sound below, so the 0-100
  // default range must not be range-checked against a later start.
  if (End <= Start && (bHasEnd || SoundPath.IsEmpty())) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("end must be greater than start"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UMovieSceneTrack *Track = FindTrackByName(MovieScene, TrackName, true, ActorName);

  if (!Track) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Track not found"),
                           nullptr, TEXT("TRACK_NOT_FOUND"));
    return true;
  }

  // A camera cut section with no camera is inert: Movie Render Queue then
  // renders the default PIE view instead of the sequence camera, which looks
  // exactly like a broken shot rather than a missing binding. bindingId was
  // accepted by the schema and dropped here, so a renderable cinematic could
  // not be authored over the bridge at all. Checked before any section object
  // exists, so a refusal leaves nothing orphaned in the track's package.
  const bool bCameraCutTrack = Track->IsA<UMovieSceneCameraCutTrack>();
  FGuid CameraBinding;
  const bool bCameraBound = bCameraCutTrack && !BindingId.IsEmpty() &&
                            FGuid::Parse(BindingId, CameraBinding);
  if (bCameraCutTrack && !bCameraBound) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("A camera cut section needs 'bindingId' set to the camera's "
             "binding GUID (from edit_sequence_bindings). Without it the "
             "cut has no camera and Movie Render Queue renders the default "
             "view."),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // A sound goes onto an audio track only. Resolved before any section exists
  // so a refusal leaves nothing orphaned.
  UMovieSceneAudioTrack *AudioTrack = Cast<UMovieSceneAudioTrack>(Track);
  USoundBase *Sound = nullptr;
  if (!SoundPath.IsEmpty()) {
    if (!AudioTrack) {
      SendAutomationResponse(
          Socket, RequestId, false,
          FString::Printf(TEXT("soundPath only applies to an Audio track; '%s' is a %s"),
                          *Track->GetName(), *Track->GetClass()->GetName()),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Sound = LoadObject<USoundBase>(
        nullptr,
        *(SoundPath.Contains(TEXT(".")) ? SoundPath
                                        : SoundPath + TEXT(".") + FPackageName::GetShortName(SoundPath)),
        nullptr, LOAD_NoWarn);
    if (!Sound) {
      SendAutomationResponse(Socket, RequestId, false,
                             FString::Printf(TEXT("Sound not found: %s"), *SoundPath),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  // AddNewSoundOnRow sizes the section to the sound (one second for a looping
  // one), picks a free row and adds it to the track itself.
  UMovieSceneSection *NewSection =
      Sound ? AudioTrack->AddNewSoundOnRow(Sound, Start, INDEX_NONE) : Track->CreateNewSection();
  if (NewSection) {
    if (Sound && !bHasEnd) {
      EndFrame = FFrameRate::TransformTime(FFrameTime(NewSection->GetExclusiveEndFrame()),
                                           MovieScene->GetTickResolution(),
                                           MovieScene->GetDisplayRate()).AsDecimal();
    } else {
      NewSection->SetRange(TRange<FFrameNumber>(Start, End));
    }
    if (UMovieSceneCameraCutSection *CutSection =
            Cast<UMovieSceneCameraCutSection>(NewSection)) {
      CutSection->SetCameraBindingID(
          UE::MovieScene::FRelativeObjectBindingID(CameraBinding));
    }

    if (!Sound) {
      Track->AddSection(*NewSection);
    }
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), Track->GetName());
    Resp->SetNumberField(TEXT("startFrame"), StartFrame);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame);
    if (bCameraBound) {
      Resp->SetStringField(TEXT("cameraBindingId"), BindingId);
    }
    if (Sound) {
      Resp->SetStringField(TEXT("soundPath"), Sound->GetPathName());
      Resp->SetStringField(TEXT("soundName"), Sound->GetName());
    }
    SendAutomationResponse(
        Socket, RequestId, true,
        Sound ? FString::Printf(TEXT("Section added to track with sound %s"), *Sound->GetName())
              : FString(TEXT("Section added to track")),
        Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create section"), nullptr,
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
}
