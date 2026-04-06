// McpTool_ManageSplines.cpp — manage_splines tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageSplines : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_splines"); }

	FString GetDescription() const override
	{
		return TEXT("Create spline actors, add/modify points, attach meshes along "
			"splines, and query spline data.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_spline_actor"),
				TEXT("add_spline_point"),
				TEXT("remove_spline_point"),
				TEXT("set_spline_point_position"),
				TEXT("set_spline_point_tangents"),
				TEXT("set_spline_point_rotation"),
				TEXT("set_spline_point_scale"),
				TEXT("set_spline_type"),
				TEXT("create_spline_mesh_component"),
				TEXT("set_spline_mesh_asset"),
				TEXT("configure_spline_mesh_axis"),
				TEXT("set_spline_mesh_material"),
				TEXT("scatter_meshes_along_spline"),
				TEXT("configure_mesh_spacing"),
				TEXT("configure_mesh_randomization"),
				TEXT("create_road_spline"),
				TEXT("create_river_spline"),
				TEXT("create_fence_spline"),
				TEXT("create_wall_spline"),
				TEXT("create_cable_spline"),
				TEXT("create_pipe_spline"),
				TEXT("get_splines_info")
			}, TEXT("Spline action to perform"))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.String(TEXT("splineName"), TEXT("Name of spline component."))
			.String(TEXT("componentName"), TEXT("Name of the component."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.Object(TEXT("location"), TEXT("Location for spline actor."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("Rotation for spline actor."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("Scale for spline actor."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("pointIndex"),
				TEXT("Index of spline point to modify."))
			.Object(TEXT("position"), TEXT("Position for spline point."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("arriveTangent"),
				TEXT("Arrive tangent for spline point (incoming direction)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("leaveTangent"),
				TEXT("Leave tangent for spline point (outgoing direction)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("tangent"),
				TEXT("Unified tangent (sets both arrive and leave)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("pointRotation"),
				TEXT("Rotation at spline point."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("pointScale"), TEXT("Scale at spline point."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.StringEnum(TEXT("coordinateSpace"), {
				TEXT("Local"),
				TEXT("World")
			}, TEXT("Coordinate space for position/tangent values (default: Local)."))
			.StringEnum(TEXT("splineType"), {
				TEXT("Linear"),
				TEXT("Curve"),
				TEXT("Constant"),
				TEXT("CurveClamped"),
				TEXT("CurveCustomTangent")
			}, TEXT("Type of spline interpolation."))
			.Bool(TEXT("bClosedLoop"),
				TEXT("Whether spline forms a closed loop."))
			.Bool(TEXT("bUpdateSpline"),
				TEXT("Update spline after modification (default: true)."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.StringEnum(TEXT("forwardAxis"), {
				TEXT("X"),
				TEXT("Y"),
				TEXT("Z")
			}, TEXT("Forward axis for spline mesh deformation."))
			.Object(TEXT("startPos"),
				TEXT("Start position for spline mesh segment."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("startTangent"),
				TEXT("Start tangent for spline mesh segment."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("endPos"),
				TEXT("End position for spline mesh segment."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("endTangent"),
				TEXT("End tangent for spline mesh segment."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("startScale"),
				TEXT("X/Y scale at spline mesh start."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Object(TEXT("endScale"),
				TEXT("X/Y scale at spline mesh end."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.Number(TEXT("startRoll"),
				TEXT("Roll angle at spline mesh start (radians)."))
			.Number(TEXT("endRoll"),
				TEXT("Roll angle at spline mesh end (radians)."))
			.Bool(TEXT("bSmoothInterpRollScale"),
				TEXT("Use smooth interpolation for roll/scale."))
			.Number(TEXT("spacing"),
				TEXT("Distance between scattered meshes."))
			.Number(TEXT("startOffset"),
				TEXT("Offset from spline start for first mesh."))
			.Number(TEXT("endOffset"),
				TEXT("Offset from spline end for last mesh."))
			.Bool(TEXT("bAlignToSpline"),
				TEXT("Align scattered meshes to spline direction."))
			.Bool(TEXT("bRandomizeRotation"),
				TEXT("Apply random rotation to scattered meshes."))
			.Object(TEXT("rotationRandomRange"),
				TEXT("Random rotation range (degrees)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Bool(TEXT("bRandomizeScale"),
				TEXT("Apply random scale to scattered meshes."))
			.Number(TEXT("scaleMin"),
				TEXT("Minimum random scale multiplier."))
			.Number(TEXT("scaleMax"),
				TEXT("Maximum random scale multiplier."))
			.Number(TEXT("randomSeed"),
				TEXT("Seed for randomization (for reproducible results)."))
			.StringEnum(TEXT("templateType"), {
				TEXT("road"),
				TEXT("river"),
				TEXT("fence"),
				TEXT("wall"),
				TEXT("cable"),
				TEXT("pipe")
			}, TEXT("Type of spline template to create."))
			.Number(TEXT("width"), TEXT("Width value."))
			.Number(TEXT("segmentLength"),
				TEXT("Length of mesh segments for deformation."))
			.Number(TEXT("postSpacing"),
				TEXT("Spacing between fence posts."))
			.Number(TEXT("railHeight"), TEXT("Height of fence rails."))
			.Number(TEXT("pipeRadius"), TEXT("Radius for pipe template."))
			.Number(TEXT("cableSlack"),
				TEXT("Slack/sag amount for cable template."))
			.ArrayOfObjects(TEXT("points"), TEXT(""))
			.String(TEXT("filter"), TEXT("General search filter."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSplines);
