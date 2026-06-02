// McpTool_ManageGameFramework.cpp — manage_game_framework tool definition (20 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageGameFramework : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_game_framework"); }

	FString GetDescription() const override
	{
		return TEXT("Create GameMode, GameState, PlayerController, PlayerState Blueprints. "
			"Configure match flow, teams, scoring, and spawning.");
	}

	FString GetCategory() const override { return TEXT("utility"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_game_mode"),
				TEXT("create_game_state"),
				TEXT("create_player_controller"),
				TEXT("create_player_state"),
				TEXT("create_game_instance"),
				TEXT("create_hud_class"),
				TEXT("set_default_pawn_class"),
				TEXT("set_player_controller_class"),
				TEXT("set_game_state_class"),
				TEXT("set_player_state_class"),
				TEXT("configure_game_rules"),
				TEXT("setup_match_states"),
				TEXT("configure_round_system"),
				TEXT("configure_team_system"),
				TEXT("configure_scoring_system"),
				TEXT("configure_spawn_system"),
				TEXT("configure_player_start"),
				TEXT("set_respawn_rules"),
				TEXT("configure_spectating"),
				TEXT("get_game_framework_info")
			}, TEXT("Game framework action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("gameModeBlueprint"), TEXT("Path to GameMode blueprint to configure."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.String(TEXT("parentClass"), TEXT("Path or name of the parent class."))
			.String(TEXT("pawnClass"), TEXT("Pawn class to use."))
			.String(TEXT("defaultPawnClass"), TEXT("Default pawn class for GameMode."))
			.String(TEXT("playerControllerClass"), TEXT("PlayerController class path."))
			.String(TEXT("gameStateClass"), TEXT("GameState class path."))
			.String(TEXT("playerStateClass"), TEXT("PlayerState class path."))
			.String(TEXT("spectatorClass"), TEXT("Spectator pawn class."))
			.String(TEXT("hudClass"), TEXT("HUD class path."))
			.Bool(TEXT("bDelayedStart"), TEXT("Whether to delay match start."))
			.ArrayOfObjects(TEXT("states"), TEXT("Match state definitions."))
			.Number(TEXT("numRounds"), TEXT(""))
			.Number(TEXT("roundTime"), TEXT(""))
			.Number(TEXT("intermissionTime"), TEXT(""))
			.Number(TEXT("numTeams"), TEXT(""))
			.Number(TEXT("teamSize"), TEXT(""))
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
			.Number(TEXT("respawnDelay"), TEXT(""))
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
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageGameFramework);
