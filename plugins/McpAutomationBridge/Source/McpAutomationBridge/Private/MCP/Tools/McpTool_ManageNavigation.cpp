// McpTool_ManageNavigation.cpp — manage_navigation tool definition (12 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageNavigation : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_navigation"); }

	FString GetDescription() const override
	{
		return TEXT("Configure NavMesh settings, add nav modifiers, "
			"create nav links and smart links for pathfinding.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("configure_nav_mesh_settings"),
				TEXT("set_nav_agent_properties"),
				TEXT("rebuild_navigation"),
				TEXT("create_nav_modifier_component"),
				TEXT("set_nav_area_class"),
				TEXT("configure_nav_area_cost"),
				TEXT("create_nav_link_proxy"),
				TEXT("configure_nav_link"),
				TEXT("set_nav_link_type"),
				TEXT("create_smart_link"),
				TEXT("configure_smart_link_behavior"),
				TEXT("get_navigation_info")
			}, TEXT("Navigation action to perform"))
			.String(TEXT("navMeshPath"), TEXT("Path to NavMesh data asset."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Number(TEXT("agentRadius"), TEXT("Navigation agent radius (default: 35)."))
			.Number(TEXT("agentHeight"), TEXT("Navigation agent height (default: 144)."))
			.Number(TEXT("agentStepHeight"), TEXT("Maximum step height agent can climb (default: 35)."))
			.Number(TEXT("agentMaxSlope"), TEXT("Maximum slope angle in degrees (default: 44)."))
			.Number(TEXT("cellSize"), TEXT("NavMesh cell size (default: 19)."))
			.Number(TEXT("cellHeight"), TEXT("NavMesh cell height (default: 10)."))
			.Number(TEXT("tileSizeUU"), TEXT("NavMesh tile size in UU (default: 1000)."))
			.Number(TEXT("minRegionArea"), TEXT("Minimum region area to keep."))
			.Number(TEXT("mergeRegionSize"), TEXT("Region merge threshold."))
			.Number(TEXT("maxSimplificationError"), TEXT("Edge simplification error."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("areaClass"), TEXT("Navigation area class path."))
			.String(TEXT("areaClassToReplace"), TEXT("Area class to replace (optional modifier behavior)."))
			.Object(TEXT("failsafeExtent"), TEXT("Failsafe extent for nav modifier when actor has no collision."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("bIncludeAgentHeight"), TEXT("Expand lower bounds by agent height."))
			.Number(TEXT("areaCost"), TEXT("Pathfinding cost multiplier for area (1.0 = normal)."))
			.Number(TEXT("fixedAreaEnteringCost"), TEXT("Fixed cost added when entering the area."))
			.String(TEXT("linkName"), TEXT("Name of the link."))
			.Object(TEXT("startPoint"), TEXT("Start point of navigation link (relative to actor)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("endPoint"), TEXT("End point of navigation link (relative to actor)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.StringEnum(TEXT("direction"), {
				TEXT("BothWays"),
				TEXT("LeftToRight"),
				TEXT("RightToLeft")
			}, TEXT("Link traversal direction."))
			.Number(TEXT("snapRadius"), TEXT("Snap radius for link endpoints (default: 30)."))
			.Bool(TEXT("linkEnabled"), TEXT("Whether the link is enabled."))
			.StringEnum(TEXT("linkType"), {
				TEXT("simple"),
				TEXT("smart")
			}, TEXT("Type of navigation link."))
			.Bool(TEXT("bSmartLinkIsRelevant"), TEXT("Toggle smart link relevancy."))
			.String(TEXT("enabledAreaClass"), TEXT("Area class when smart link is enabled."))
			.String(TEXT("disabledAreaClass"), TEXT("Area class when smart link is disabled."))
			.Number(TEXT("broadcastRadius"), TEXT("Radius for state change broadcast."))
			.Number(TEXT("broadcastInterval"), TEXT("Interval for state change broadcast (0 = single)."))
			.Bool(TEXT("bCreateBoxObstacle"), TEXT("Add box obstacle during nav generation."))
			.Object(TEXT("obstacleOffset"), TEXT("Offset of simple box obstacle."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("obstacleExtent"), TEXT("Extent of simple box obstacle."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("obstacleAreaClass"), TEXT("Area class for box obstacle."))
			.Object(TEXT("location"), TEXT("World location for nav link proxy."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("Rotation for nav link proxy."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.String(TEXT("filter"), TEXT("General search filter."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageNavigation);
