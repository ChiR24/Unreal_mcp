// McpTool_ManageNetworking.cpp — manage_networking tool definition (73 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageNetworking : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_networking"); }

	FString GetDescription() const override
	{
		return TEXT("Configure multiplayer and player flow: replication, RPCs "
			"(Server/Client/Multicast), authority, relevancy, prediction, sessions, game framework, and input.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageNetworking(),
				TEXT("Networking action to perform"))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("propertyName"), TEXT("Name of the property."))
			.Bool(TEXT("replicated"), TEXT("Whether property should be replicated."))
			.StringEnum(TEXT("condition"), {
				TEXT("COND_None"),
				TEXT("COND_InitialOnly"),
				TEXT("COND_OwnerOnly"),
				TEXT("COND_SkipOwner"),
				TEXT("COND_SimulatedOnly"),
				TEXT("COND_AutonomousOnly"),
				TEXT("COND_SimulatedOrPhysics"),
				TEXT("COND_InitialOrOwner"),
				TEXT("COND_Custom"),
				TEXT("COND_ReplayOrOwner"),
				TEXT("COND_ReplayOnly"),
				TEXT("COND_SimulatedOnlyNoReplay"),
				TEXT("COND_SimulatedOrPhysicsNoReplay"),
				TEXT("COND_SkipReplay"),
				TEXT("COND_Never")
			}, TEXT("Replication condition."))
			.String(TEXT("repNotifyFunc"), TEXT("RepNotify function name."))
			.Number(TEXT("netUpdateFrequency"), TEXT("How often actor replicates (Hz, default 100)."))
			.Number(TEXT("minNetUpdateFrequency"), TEXT("Minimum update frequency when idle (Hz, default 2)."))
			.Number(TEXT("netPriority"), TEXT("Network priority for bandwidth (default 1.0)."))
			.StringEnum(TEXT("dormancy"), {
				TEXT("DORM_Never"),
				TEXT("DORM_Awake"),
				TEXT("DORM_DormantAll"),
				TEXT("DORM_DormantPartial"),
				TEXT("DORM_Initial")
			}, TEXT("Net dormancy mode."))
			.String(TEXT("functionName"), TEXT("Name of the function."))
			.StringEnum(TEXT("rpcType"), {
				TEXT("Server"),
				TEXT("Client"),
				TEXT("NetMulticast")
			}, TEXT("Type of RPC."))
			.Bool(TEXT("reliable"), TEXT("Whether the operation is reliable."))
			.Bool(TEXT("withValidation"), TEXT("Enable RPC validation."))
			.String(TEXT("ownerActorName"), TEXT("Name of owner actor (null to clear)."))
			.Bool(TEXT("isAutonomousProxy"), TEXT("Configure as autonomous proxy."))
			.Number(TEXT("netCullDistanceSquared"), TEXT("Network cull distance squared."))
			.Bool(TEXT("useOwnerNetRelevancy"), TEXT("Use owner relevancy."))
			.Bool(TEXT("alwaysRelevant"), TEXT("Always relevant to all clients."))
			.Bool(TEXT("onlyRelevantToOwner"), TEXT("Only relevant to owner."))
			.String(TEXT("structName"), TEXT("Name of struct for custom serialization."))
			.Bool(TEXT("usePushModel"), TEXT("Use push-model replication."))
			.Bool(TEXT("enablePrediction"), TEXT("Enable client-side prediction."))
			.Number(TEXT("correctionThreshold"), TEXT("Server correction threshold."))
			.Number(TEXT("smoothingRate"), TEXT("Smoothing rate for corrections."))
			.StringEnum(TEXT("dataType"), {
				TEXT("Transform"),
				TEXT("Vector"),
				TEXT("Rotator"),
				TEXT("Float")
			}, TEXT("Network prediction data type."))
			.StringEnum(TEXT("networkSmoothingMode"), {
				TEXT("Disabled"),
				TEXT("Linear"),
				TEXT("Exponential")
			}, TEXT("Movement smoothing mode."))
			.Number(TEXT("networkMaxSmoothUpdateDistance"), TEXT("Max smooth update distance."))
			.Number(TEXT("networkNoSmoothUpdateDistance"), TEXT("No smooth update distance."))
			.Number(TEXT("maxClientRate"), TEXT("Max client rate."))
			.Number(TEXT("maxInternetClientRate"), TEXT("Max internet client rate."))
			.Number(TEXT("netServerMaxTickRate"), TEXT("Server max tick rate."))
			.StringEnum(TEXT("role"), {
				TEXT("ROLE_None"),
				TEXT("ROLE_SimulatedProxy"),
				TEXT("ROLE_AutonomousProxy"),
				TEXT("ROLE_Authority")
			}, TEXT("Net role."))
			.Bool(TEXT("replicateMovement"), TEXT("Replicate movement."))
			.Bool(TEXT("spatiallyLoaded"), TEXT("Spatially loaded for replication graph."))
			.Bool(TEXT("netLoadOnClient"), TEXT("Net load on client for replication graph."))
			.String(TEXT("replicationPolicy"), TEXT("Replication policy for replication graph."))
			.Bool(TEXT("customSerialization"), TEXT("Use custom serialization."))
			.Number(TEXT("predictionThreshold"), TEXT("Prediction threshold for client prediction."))
			.String(TEXT("sessionName"), TEXT("Session name."))
			.Number(TEXT("maxPlayers"), TEXT("Maximum player count."))
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
			}, TEXT("Type of session interface to use."))
			.Bool(TEXT("enabled"), TEXT("Enable or disable the feature."))
			.StringEnum(TEXT("splitScreenType"), {
				TEXT("None"),
				TEXT("TwoPlayer_Horizontal"),
				TEXT("TwoPlayer_Vertical"),
				TEXT("ThreePlayer_FavorTop"),
				TEXT("ThreePlayer_FavorBottom"),
				TEXT("FourPlayer_Grid")
			}, TEXT("Split-screen layout type."))
			.Number(TEXT("playerIndex"), TEXT("Local player index."))
			.Number(TEXT("controllerId"), TEXT("Controller ID for player input."))
			.String(TEXT("serverAddress"), TEXT("Server IP address."))
			.Number(TEXT("serverPort"), TEXT("Server port."))
			.String(TEXT("serverPassword"), TEXT("Server password for protected games."))
			.String(TEXT("serverName"), TEXT("Display name for the server."))
			.String(TEXT("mapName"), TEXT("Map to load for hosting."))
			.String(TEXT("travelOptions"), TEXT("Travel URL options string."))
			.Bool(TEXT("voiceEnabled"), TEXT("Enable or disable voice chat."))
			.Object(TEXT("voiceSettings"), TEXT("Voice processing settings."),
				[](FMcpSchemaBuilder& S) {
					S.Number(TEXT("volume"))
						.Number(TEXT("noiseGateThreshold"))
						.Bool(TEXT("noiseSuppression"))
						.Bool(TEXT("echoCancellation"))
						.Number(TEXT("sampleRate"));
				})
			.String(TEXT("channelName"), TEXT("Voice channel name."))
			.StringEnum(TEXT("channelType"), {
				TEXT("Team"),
				TEXT("Global"),
				TEXT("Proximity"),
				TEXT("Party")
			}, TEXT("Voice channel type."))
			.String(TEXT("playerName"), TEXT("Player name for voice operations."))
			.String(TEXT("targetPlayerId"), TEXT("Target player ID."))
			.Bool(TEXT("muted"), TEXT("Whether the player is muted."))
			.Number(TEXT("attenuationRadius"), TEXT("Radius for voice attenuation."))
			.Number(TEXT("attenuationFalloff"), TEXT("Falloff rate for voice attenuation."))
			.Bool(TEXT("pushToTalkEnabled"), TEXT("Enable push-to-talk mode."))
			.String(TEXT("pushToTalkKey"), TEXT("Key binding for push-to-talk."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("gameModeBlueprint"), TEXT("Path to GameMode blueprint to configure."))
			.String(TEXT("parentClass"), TEXT("Parent class path."))
			.String(TEXT("pawnClass"), TEXT("Pawn class to use."))
			.String(TEXT("defaultPawnClass"), TEXT("Default pawn class for GameMode."))
			.String(TEXT("playerControllerClass"), TEXT("PlayerController class path."))
			.String(TEXT("gameStateClass"), TEXT("GameState class path."))
			.String(TEXT("playerStateClass"), TEXT("PlayerState class path."))
			.String(TEXT("spectatorClass"), TEXT("Spectator pawn class."))
			.String(TEXT("hudClass"), TEXT("HUD class path."))
			.Bool(TEXT("bDelayedStart"), TEXT("Whether to delay match start."))
			.ArrayOfObjects(TEXT("states"), TEXT("Match state definitions."),
				[](FMcpSchemaBuilder& S) {
					S.StringEnum(TEXT("name"), {
						TEXT("waiting"),
						TEXT("warmup"),
						TEXT("in_progress"),
						TEXT("post_match"),
						TEXT("custom")
					}, TEXT("Match state name."))
						.Number(TEXT("duration"))
						.String(TEXT("customName"), TEXT("Custom state name."));
				})
			.Number(TEXT("numRounds"), TEXT("Number of rounds."))
			.Number(TEXT("roundTime"), TEXT("Round duration."))
			.Number(TEXT("intermissionTime"), TEXT("Intermission duration."))
			.Number(TEXT("numTeams"), TEXT("Number of teams."))
			.Number(TEXT("teamSize"), TEXT("Players per team."))
			.Bool(TEXT("autoBalance"), TEXT("Enable automatic team balancing."))
			.Bool(TEXT("friendlyFire"), TEXT("Enable friendly fire damage."))
			.Number(TEXT("teamIndex"), TEXT("Team index for PlayerStart."))
			.Number(TEXT("scorePerKill"), TEXT("Points awarded per kill."))
			.Number(TEXT("scorePerObjective"), TEXT("Points awarded per objective."))
			.Number(TEXT("scorePerAssist"), TEXT("Points awarded per assist."))
			.StringEnum(TEXT("spawnSelectionMethod"), {
				TEXT("Random"),
				TEXT("RoundRobin"),
				TEXT("FarthestFromEnemies")
			}, TEXT("How to select spawn points."))
			.Number(TEXT("respawnDelay"), TEXT("Respawn delay."))
			.StringEnum(TEXT("respawnLocation"), {
				TEXT("PlayerStart"),
				TEXT("LastDeath"),
				TEXT("TeamBase")
			}, TEXT("Where players respawn."))
			.Bool(TEXT("usePlayerStarts"), TEXT("Use PlayerStart actors."))
			.Bool(TEXT("allowSpectating"), TEXT("Allow spectator mode."))
			.StringEnum(TEXT("spectatorViewMode"), {
				TEXT("FreeCam"),
				TEXT("ThirdPerson"),
				TEXT("FirstPerson"),
				TEXT("DeathCam")
			}, TEXT("Spectator view mode."))
			.Bool(TEXT("save"), TEXT("Save the asset after the operation."))
			.String(TEXT("contextPath"), TEXT("Input mapping context asset path."))
			.String(TEXT("actionPath"), TEXT("Input action asset path."))
			.String(TEXT("key"), TEXT("Input key name."))
			.String(TEXT("triggerType"), TEXT("Input trigger type."))
			.String(TEXT("modifierType"), TEXT("Input modifier type."))
			.String(TEXT("assetPath"), TEXT("Asset path."))
			.Number(TEXT("priority"), TEXT("Priority for input mapping context."))
			.Number(TEXT("timeoutMs"), TEXT("Timeout in milliseconds."))
			.Bool(TEXT("canRespawn"), TEXT("Whether players can respawn."))
			.Bool(TEXT("executeTravel"), TEXT("Execute travel for the session operation."))
			.Bool(TEXT("forceRespawn"), TEXT("Force respawn behavior."))
			.Number(TEXT("localPlayerNum"), TEXT("Local player number."))
			.Number(TEXT("maxRespawns"), TEXT("Maximum respawn count."))
			.Number(TEXT("respawnLives"), TEXT("Respawn life count."))
			.Number(TEXT("scorePerDeath"), TEXT("Points awarded per death."))
			.Bool(TEXT("systemWide"), TEXT("Apply voice operation system-wide."))
			.String(TEXT("variableName"), TEXT("Variable name."))
			.Number(TEXT("winScore"), TEXT("Score needed to win."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageNetworking);
