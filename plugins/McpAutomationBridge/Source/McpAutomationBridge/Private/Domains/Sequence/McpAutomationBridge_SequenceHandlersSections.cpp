#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"
#include "Sections/MovieSceneCameraCutSection.h"
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
  if (!Payload->TryGetNumberField(TEXT("endFrame"), EndFrame)) {
    Payload->TryGetNumberField(TEXT("end"), EndFrame);
  }
  FString BindingId;
  Payload->TryGetStringField(TEXT("bindingId"), BindingId);

  UMovieScene *MovieScene = nullptr;
  ULevelSequence *Sequence = McpSequence::LoadOrReply(this, RequestId, Socket, Payload, TEXT("add_section"), MovieScene);
  if (!Sequence) {
    return true;
  }
  FFrameNumber Start;
  FFrameNumber End;
  FString FrameError;
  if (!McpSequenceFrameMath::TryFrameNumber(StartFrame, Start, FrameError) ||
      !McpSequenceFrameMath::TryFrameNumber(EndFrame, End, FrameError)) {
    SendAutomationResponse(Socket, RequestId, false, FrameError, nullptr, TEXT("INVALID_ARGUMENT"));
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

  UMovieSceneSection *NewSection = Track->CreateNewSection();
  if (NewSection) {
    NewSection->SetRange(TRange<FFrameNumber>(Start, End));
    if (UMovieSceneCameraCutSection *CutSection =
            Cast<UMovieSceneCameraCutSection>(NewSection)) {
      CutSection->SetCameraBindingID(
          UE::MovieScene::FRelativeObjectBindingID(CameraBinding));
    }

    Track->AddSection(*NewSection);
    MovieScene->Modify();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("trackName"), Track->GetName());
    Resp->SetNumberField(TEXT("startFrame"), StartFrame);
    Resp->SetNumberField(TEXT("endFrame"), EndFrame);
    if (bCameraBound) {
      Resp->SetStringField(TEXT("cameraBindingId"), BindingId);
    }
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Section added to track"), Resp);
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create section"), nullptr,
                           TEXT("SECTION_CREATION_FAILED"));
  }
  return true;
}
