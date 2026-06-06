#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("sequence_")) &&
      !Lower.Equals(TEXT("manage_sequence")))
    return false;

  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString EffectiveAction = Lower;

  if (Lower.Equals(TEXT("manage_sequence"))) {
    FString Sub;
    if (LocalPayload->TryGetStringField(TEXT("subAction"), Sub) &&
        !Sub.IsEmpty()) {
      EffectiveAction = Sub.ToLower();
      if (EffectiveAction == TEXT("create"))
        EffectiveAction = TEXT("sequence_create");
      else if (!EffectiveAction.StartsWith(TEXT("sequence_")))
        EffectiveAction = TEXT("sequence_") + EffectiveAction;
    }
  }

  if (EffectiveAction == TEXT("sequence_create"))
    return HandleSequenceCreate(RequestId, LocalPayload, RequestingSocket);
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
    return HandleSequenceGetMetadata(RequestId, LocalPayload, RequestingSocket);
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
  if (EffectiveAction == TEXT("sequence_set_work_range"))
    return McpSequenceRanges::HandleSetWorkRange(this, RequestId, LocalPayload,
                                                 RequestingSocket);

  SendAutomationResponse(
      RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Sequence action not implemented by plugin: %s"),
                      *Action),
      nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
}
