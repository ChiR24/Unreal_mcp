#define MCP_SUBSYSTEM_SEQUENCE_DECLARATIONS \
FString ResolveSequencePath(const TSharedPtr<FJsonObject>& Payload); \
TSharedPtr<FJsonObject> EnsureSequenceEntry(const FString& SeqPath); \
MCP_DECLARE_ACTION_HANDLER(HandleAddSequencerKeyframe); \
MCP_DECLARE_ACTION_HANDLER(HandleManageSequencerTrack); \
MCP_DECLARE_ACTION_HANDLER(HandleAddCameraTrack); \
MCP_DECLARE_ACTION_HANDLER(HandleAddAnimationTrack); \
MCP_DECLARE_ACTION_HANDLER(HandleAddTransformTrack); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceCreate); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetDisplayRate); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetProperties); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceOpen); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddCamera); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequencePlay); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddActor); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddActors); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddSpawnable); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceRemoveActors); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceGetBindings); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceGetProperties); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetPlaybackSpeed); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequencePause); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceStop); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceList); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceDuplicate); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceRename); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceDelete); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceGetMetadata); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddKeyframe); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceAddSection); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetTickResolution); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetViewRange); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetTrackMuted); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetTrackSolo); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceSetTrackLocked); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSequenceRemoveTrack);
