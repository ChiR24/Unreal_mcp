#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Validation/McpSeqFrameMath.h"
#include "Misc/PackageName.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sound/SoundBase.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

bool UEngineWireSubsystem::HandleSequenceAddSection(
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
    SendAutomationError(Socket, RequestId, FrameError, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // A sound with no end takes its length from the sound below, so the 0-100
  // default range must not be range-checked against a later start.
  if (End <= Start && (bHasEnd || SoundPath.IsEmpty())) {
    SendAutomationError(Socket, RequestId, TEXT("end must be greater than start"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Ambiguous;
  UMovieSceneTrack *Track = FindTrackByName(MovieScene, TrackName, true, ActorName, &Ambiguous);

  if (!Track) {
    McpSequence::ReplyTrackMiss(this, RequestId, Socket, TrackName, Ambiguous);
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
    SendAutomationError(
        Socket, RequestId,
        TEXT("A camera cut section needs 'bindingId' set to the camera's "
             "binding GUID (from edit_sequence_bindings). Without it the "
             "cut has no camera and Movie Render Queue renders the default "
             "view."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // A sound goes onto an audio track only. Resolved before any section exists
  // so a refusal leaves nothing orphaned.
  UMovieSceneAudioTrack *AudioTrack = Cast<UMovieSceneAudioTrack>(Track);
  USoundBase *Sound = nullptr;
  if (!SoundPath.IsEmpty()) {
    if (!AudioTrack) {
      SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("soundPath only applies to an Audio track; '%s' is a %s"),
                          *Track->GetName(), *Track->GetClass()->GetName()),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // /Content maps to /Game; a traversal or an unmounted root is refused before anything loads.
    FString SoundObjectPath = SoundPath;
    McpAssetPathCanonical::MapContentRootInline(SoundObjectPath);
    SoundObjectPath = SanitizeProjectRelativePath(SoundObjectPath);
    if (SoundObjectPath.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             McpPathRefusalMessage(TEXT("soundPath"), SoundPath),
                             nullptr, TEXT("INVALID_PATH"));
      return true;
    }
    Sound = LoadObject<USoundBase>(
        nullptr,
        *McpToObjectPath(SoundObjectPath),
        nullptr, LOAD_NoWarn);
    if (!Sound) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Sound not found: %s"), *SoundPath),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
  }

  // A track caches the field it is evaluated from and rebuilds it only when its own signature changes, and the
  // engine's AddNewSoundOnRow and AddSection change neither the signature nor the undo record. A track compiled
  // before this call (a sequence open in Sequencer, a Play) kept a field without the new section, saved with the
  // asset, so the section never played, in the editor or the game. Modify the track first, as Sequencer does.
  Track->Modify();
  // AddNewSoundOnRow sizes the section to the sound (one second for a looping
  // one), picks a free row and adds it to the track itself.
  UMovieSceneSection *NewSection =
      Sound ? AudioTrack->AddNewSoundOnRow(Sound, Start, INDEX_NONE) : Track->CreateNewSection();
  if (NewSection) {
    if (Sound) {
      // The row AddNewSoundOnRow picked fits the sound's own length. The range that stands is the
      // one asked for, or the sound's; a zero-length sound (5.0-5.2 leave it empty, 5.3+ give it
      // a second) gets a second on every version. Placing the section again for a different range
      // keeps it from overlapping another section on its row.
      const FFrameNumber SoundEnd = NewSection->GetExclusiveEndFrame();
      const FFrameNumber Length =
          bHasEnd ? End - Start
                  : (Start < SoundEnd ? SoundEnd - Start
                                      : MovieScene->GetTickResolution().AsFrameNumber(1.0));
      if (Start + Length != SoundEnd) {
        NewSection->InitialPlacementOnRow(Track->GetAllSections(), Start, Length.Value, INDEX_NONE);
      }
      if (!bHasEnd) {
        EndFrame = FFrameRate::TransformTime(FFrameTime(Start + Length),
                                             MovieScene->GetTickResolution(),
                                             MovieScene->GetDisplayRate()).AsDecimal();
      }
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
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create section"),
                        TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
}
