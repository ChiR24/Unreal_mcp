#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Validation/McpSeqIntegerValidate.h"
#include "Domains/Sequence/Cinematics/McpSeqCine.h"
#include "Domains/Sequence/Media/McpSeqMedia.h"
#include "Domains/Sequence/Metadata/McpSeqMetadata.h"
#include "Domains/Sequence/MovieRender/McpSeqMRQ.h"
#include "Domains/Sequence/Planner/McpAutoEdit.h"
#include "Domains/Sequence/Planner/McpFlyThrough.h"
#include "Domains/Sequence/Planner/McpFrameShot.h"
#include "Domains/Sequence/RecordReplay/McpSeqRecordReplay.h"
#include "ScopedTransaction.h"

bool UEngineWireSubsystem::HandleSequenceAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  // The handler table's manage_sequence row: the sub-action names the handler below as sequence_<sub-action>.
  const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(LocalPayload);
  FString EffectiveAction = SubAction;
  if (!EffectiveAction.StartsWith(TEXT("sequence_"))) {
    EffectiveAction = TEXT("sequence_") + EffectiveAction;
  }

  FString IntegerError;
  if (!McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
          LocalPayload, IntegerError)) {
    SendAutomationError(RequestingSocket, RequestId, IntegerError,
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // An edit of a sequence's content is one undo step: these handlers Modify the sequence and its movie scene, which with
  // no transaction open recorded nothing, so control_editor undo reverted the step before the call. The ones that open
  // their own step, asset creation, playback and the reads are not listed.
  static const TSet<FString> ContentEdits = {
      TEXT("sequence_set_properties"), TEXT("sequence_add_actor"), TEXT("sequence_add_actors"),
      TEXT("sequence_add_spawnable_from_class"), TEXT("sequence_remove_actors"), TEXT("sequence_add_keyframe"),
      TEXT("sequence_add_section"), TEXT("sequence_set_section"), TEXT("sequence_set_tick_resolution"),
      TEXT("sequence_set_view_range"), TEXT("sequence_remove_track")};
  TOptional<FScopedTransaction> Step;
  if (ContentEdits.Contains(EffectiveAction)) {
    Step.Emplace(FText::FromString(FString::Printf(TEXT("Sequencer %s"), *SubAction)));
  }

  if (EffectiveAction == TEXT("sequence_create"))
    return HandleSequenceCreate(RequestId, LocalPayload, RequestingSocket);
  // A master sequence is create with its own defaults; its replies are create's.
  if (EffectiveAction == TEXT("sequence_create_master_sequence"))
    return HandleSequenceCreate(RequestId, McpSequence::MasterSequenceAsCreate(LocalPayload), RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_display_rate"))
    return HandleSequenceSetDisplayRate(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_properties"))
    return HandleSequenceSetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_open"))
    return HandleSequenceOpen(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_camera"))
    return HandleSequenceAddCamera(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_play"))
    return HandleSequencePlay(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actor"))
    return HandleSequenceAddActor(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_actors"))
    return HandleSequenceAddActors(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_spawnable_from_class"))
    return HandleSequenceAddSpawnable(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_actors"))
    return HandleSequenceRemoveActors(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_bindings"))
    return HandleSequenceGetBindings(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_properties"))
    return HandleSequenceGetProperties(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_playback_speed"))
    return HandleSequenceSetPlaybackSpeed(RequestId, LocalPayload,
                                          RequestingSocket);
  if (EffectiveAction == TEXT("sequence_pause"))
    return HandleSequencePause(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_stop"))
    return HandleSequenceStop(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list"))
    return HandleSequenceList(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_duplicate"))
    return HandleSequenceDuplicate(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_rename"))
    return HandleSequenceRename(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_delete"))
    return HandleSequenceDelete(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_get_metadata"))
    return McpSequenceMetadata::HandleGetMetadata(this, RequestId, LocalPayload,
                                                  RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_metadata"))
    return HandleSequenceSetMetadata(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_keyframe"))
    return HandleSequenceAddKeyframe(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_section"))
    return HandleSequenceAddSection(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_tick_resolution"))
    return HandleSequenceSetTickResolution(RequestId, LocalPayload,
                                           RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_view_range"))
    return HandleSequenceSetViewRange(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_muted"))
    return HandleSequenceSetTrackMuted(RequestId, LocalPayload,
                                       RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_solo"))
    return HandleSequenceSetTrackSolo(RequestId, LocalPayload,
                                      RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_track_locked"))
    return HandleSequenceSetTrackLocked(RequestId, LocalPayload,
                                        RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_section"))
    return HandleSequenceSetSection(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_track"))
    return HandleSequenceRemoveTrack(RequestId, LocalPayload, RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list_track_types"))
    return McpSequenceTracks::HandleListTrackTypes(this, RequestId,
                                                   RequestingSocket);
  if (EffectiveAction == TEXT("sequence_add_track"))
    return McpSequenceTracks::HandleAddTrack(this, RequestId, LocalPayload,
                                             RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list_tracks"))
    return McpSequenceTracks::HandleListTracks(this, RequestId, LocalPayload,
                                               RequestingSocket);
  if (EffectiveAction == TEXT("sequence_list_track_keys"))
    return McpSequenceTracks::HandleListTrackKeys(this, RequestId, LocalPayload,
                                                  RequestingSocket);
  if (EffectiveAction == TEXT("sequence_remove_keyframe"))
    return McpSequenceTracks::HandleRemoveKeyframe(this, RequestId, LocalPayload,
                                                   RequestingSocket);
  if (EffectiveAction == TEXT("sequence_set_work_range"))
    return McpSequenceRanges::HandleSetWorkRange(this, RequestId, LocalPayload,
                                                 RequestingSocket);

  if (EffectiveAction == TEXT("sequence_auto_edit"))
    return McpAutoEdit::Handle(*this, RequestId, LocalPayload, RequestingSocket,
                               [this](const FString &Name) { return FindActorByName(Name, true); });
  if (EffectiveAction == TEXT("sequence_fly_through"))
    return McpFlyThrough::Handle(*this, RequestId, LocalPayload, RequestingSocket,
                                 [this](const FString &Name) { return FindActorByName(Name, true); });
  if (EffectiveAction == TEXT("sequence_frame_shot"))
    return McpFrameShot::Handle(*this, RequestId, LocalPayload, RequestingSocket,
                                [this](const FString &Name) { return FindActorByName(Name, true); });

  // The sub-dispatchers below are keyed by the sub-action without its sequence_ prefix.
  const FString BareAction = EffectiveAction.RightChop(9);
  TSharedPtr<FJsonObject> CinematicsResult;
  if (McpSequenceCinematics::TryHandleCinematics(
          BareAction, LocalPayload, CinematicsResult)) {
    bool bSuccess = false;
    FString Message;
    FString ErrorCode;
    if (CinematicsResult.IsValid()) {
      CinematicsResult->TryGetBoolField(TEXT("success"), bSuccess);
      if (!CinematicsResult->TryGetStringField(TEXT("message"), Message)) {
        CinematicsResult->TryGetStringField(TEXT("error"), Message);
      }
      CinematicsResult->TryGetStringField(TEXT("errorCode"), ErrorCode);
    }
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message,
                           CinematicsResult, ErrorCode);
    return true;
  }

  if (McpSequenceMovieRender::TryHandle(this, RequestId, BareAction,
                                        LocalPayload, RequestingSocket))
    return true;
  if (McpSequenceMedia::TryHandleMediaAction(this, RequestId, BareAction,
                                             LocalPayload, RequestingSocket))
    return true;
  if (McpSequenceRecordReplay::TryHandle(this, RequestId, BareAction,
                                         LocalPayload, RequestingSocket))
    return true;

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Sequence action not implemented by plugin: %s"),
                      *SubAction),
      TEXT("NOT_IMPLEMENTED"));
  return true;
}
