// McpTool_ManageSessions.cpp — manage_sessions tool definition (16 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageSessions : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_sessions"); }

	FString GetDescription() const override
	{
		return TEXT("Configure local multiplayer: split-screen layouts, LAN "
			"hosting/joining, voice chat channels, and push-to-talk.\n\n"
			"Required parameters by action:\n"
			"- configure_local_session_settings: At least one setting param required\n"
			"- configure_session_interface: interfaceType\n"
			"- configure_split_screen: enabled or splitScreenType\n"
			"- set_split_screen_type: splitScreenType\n"
			"- add_local_player: controllerId (requires PIE)\n"
			"- remove_local_player: playerIndex (requires PIE)\n"
			"- configure_lan_play: enabled, serverPort, or serverPassword\n"
			"- host_lan_server: mapName (requires PIE)\n"
			"- join_lan_server: serverAddress (requires PIE)\n"
			"- enable_voice_chat: voiceEnabled\n"
			"- configure_voice_settings: voiceSettings\n"
			"- set_voice_channel: channelName\n"
			"- mute_player: playerName or targetPlayerId\n"
			"- set_voice_attenuation: attenuationRadius\n"
			"- configure_push_to_talk: pushToTalkEnabled\n"
			"- get_sessions_info: No additional params required");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("configure_local_session_settings"),
				TEXT("configure_session_interface"),
				TEXT("configure_split_screen"),
				TEXT("set_split_screen_type"),
				TEXT("add_local_player"),
				TEXT("remove_local_player"),
				TEXT("configure_lan_play"),
				TEXT("host_lan_server"),
				TEXT("join_lan_server"),
				TEXT("enable_voice_chat"),
				TEXT("configure_voice_settings"),
				TEXT("set_voice_channel"),
				TEXT("mute_player"),
				TEXT("set_voice_attenuation"),
				TEXT("configure_push_to_talk"),
				TEXT("get_sessions_info")
			}, TEXT("Sessions action to perform."))
			.String(TEXT("sessionName"), TEXT("Name of the session."))
			.Number(TEXT("maxPlayers"), TEXT(""))
			.Bool(TEXT("bIsLANMatch"), TEXT("Whether this is a LAN match."))
			.Bool(TEXT("bAllowJoinInProgress"), TEXT("Allow joining games in progress."))
			.Bool(TEXT("bAllowInvites"), TEXT("Allow player invites."))
			.Bool(TEXT("bUsesPresence"), TEXT("Use presence for session discovery."))
			.Bool(TEXT("bUseLobbiesIfAvailable"), TEXT("Use lobby system if available."))
			.Bool(TEXT("bShouldAdvertise"), TEXT("Advertise session publicly."))
			.StringEnum(TEXT("interfaceType"), {
				TEXT("Default"),
				TEXT("LAN"),
				TEXT("Null")
			}, TEXT("Type of session interface to use. REQUIRED for configure_session_interface."))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.StringEnum(TEXT("splitScreenType"), {
				TEXT("None"),
				TEXT("TwoPlayer_Horizontal"),
				TEXT("TwoPlayer_Vertical"),
				TEXT("ThreePlayer_FavorTop"),
				TEXT("ThreePlayer_FavorBottom"),
				TEXT("FourPlayer_Grid")
			}, TEXT("Split-screen layout type. REQUIRED for set_split_screen_type."))
			.Number(TEXT("playerIndex"), TEXT("Local player index. REQUIRED for remove_local_player."))
			.Number(TEXT("controllerId"), TEXT("Controller ID for player input. REQUIRED for add_local_player."))
			.String(TEXT("serverAddress"), TEXT("Server IP address. REQUIRED for join_lan_server."))
			.Number(TEXT("serverPort"), TEXT(""))
			.String(TEXT("serverPassword"), TEXT("Server password for protected games."))
			.String(TEXT("serverName"), TEXT("Display name for the server."))
			.String(TEXT("mapName"), TEXT("Map to load for hosting. REQUIRED for host_lan_server."))
			.String(TEXT("travelOptions"), TEXT("Travel URL options string."))
			.Bool(TEXT("voiceEnabled"), TEXT("Enable/disable voice chat. REQUIRED for enable_voice_chat."))
			.Object(TEXT("voiceSettings"), TEXT("Voice processing settings. REQUIRED for configure_voice_settings."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("volume"), TEXT("Voice volume (0.0 - 1.0)."))
					.Number(TEXT("noiseGateThreshold"), TEXT("Noise gate threshold."))
					.Bool(TEXT("noiseSuppression"), TEXT("Enable noise suppression."))
					.Bool(TEXT("echoCancellation"), TEXT("Enable echo cancellation."))
					.Number(TEXT("sampleRate"), TEXT("Audio sample rate in Hz."));
			})
			.String(TEXT("channelName"), TEXT("Voice channel name. REQUIRED for set_voice_channel."))
			.StringEnum(TEXT("channelType"), {
				TEXT("Team"),
				TEXT("Global"),
				TEXT("Proximity"),
				TEXT("Party")
			}, TEXT("Voice channel type."))
			.String(TEXT("playerName"), TEXT("Player name for voice operations. REQUIRED for mute_player (or targetPlayerId)."))
			.String(TEXT("targetPlayerId"), TEXT("Target player ID. REQUIRED for mute_player (or playerName)."))
			.Bool(TEXT("muted"), TEXT("Whether the item is muted."))
			.Number(TEXT("attenuationRadius"), TEXT("Radius for voice attenuation (Proximity chat). REQUIRED for set_voice_attenuation."))
			.Number(TEXT("attenuationFalloff"), TEXT("Falloff rate for voice attenuation."))
			.Bool(TEXT("pushToTalkEnabled"), TEXT("Enable push-to-talk mode. REQUIRED for configure_push_to_talk."))
			.String(TEXT("pushToTalkKey"), TEXT("Key binding for push-to-talk."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSessions);
