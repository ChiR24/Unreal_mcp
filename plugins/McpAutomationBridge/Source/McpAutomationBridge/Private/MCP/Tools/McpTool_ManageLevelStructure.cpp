// McpTool_ManageLevelStructure.cpp — manage_level_structure tool definition

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManageLevelStructure : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_level_structure"); }

	FString GetDescription() const override
	{
		return TEXT("Create levels and sublevels. Configure World Partition, streaming, "
			"data layers, HLOD, level instances, and world volumes.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::ManageLevelStructure(),
				TEXT("Level structure action to perform."))
			.String(TEXT("levelName"), TEXT(""))
			.String(TEXT("levelPath"), TEXT("Level asset path."))
			.String(TEXT("parentLevel"), TEXT("Parent level path."))
			.Bool(TEXT("bCreateWorldPartition"),
				TEXT("Create with World Partition enabled."))
			.Bool(TEXT("bUseExternalActors"),
				TEXT("Enable One File Per Actor (OFPA/External Actors) for Data Layer "
					"compatibility. Automatically enabled when bCreateWorldPartition "
					"is true."))
			.String(TEXT("sublevelName"), TEXT("Name of the sublevel."))
			.String(TEXT("sublevelPath"), TEXT("Level asset path."))
			.StringEnum(TEXT("streamingMethod"), {
				TEXT("Blueprint"),
				TEXT("AlwaysLoaded"),
				TEXT("Disabled")
			}, TEXT("Level streaming method."))
			.Bool(TEXT("bShouldBeVisible"),
				TEXT("Level should be visible when loaded."))
			.Bool(TEXT("bShouldBlockOnLoad"),
				TEXT("Block game until level is loaded."))
			.Bool(TEXT("bDisableDistanceStreaming"),
				TEXT("Disable distance-based streaming."))
			.Number(TEXT("streamingDistance"),
				TEXT("Distance/radius for streaming volume "
					"(creates ALevelStreamingVolume)."))
			.StringEnum(TEXT("streamingUsage"), {
				TEXT("Loading"),
				TEXT("LoadingAndVisibility"),
				TEXT("VisibilityBlockingOnLoad"),
				TEXT("BlockingOnLoad"),
				TEXT("LoadingNotVisible")
			}, TEXT("Streaming volume usage mode (default: LoadingAndVisibility)."))
			.Bool(TEXT("createVolume"),
				TEXT("Create a streaming volume (true) or just report existing "
					"volumes (false). Default: true."))
			.Object(TEXT("boundsOrigin"), TEXT("Origin of level bounds."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("boundsExtent"), TEXT("Extent of level bounds."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("bAutoCalculateBounds"),
				TEXT("Auto-calculate bounds from content."))
			.Bool(TEXT("bEnableWorldPartition"),
				TEXT("Enable World Partition for level."))
			.Number(TEXT("gridCellSize"),
				TEXT("World Partition grid cell size."))
			.Number(TEXT("loadingRange"),
				TEXT("Loading range for grid cells."))
			.String(TEXT("gridName"), TEXT("World Partition grid name."))
			.Bool(TEXT("bBlockOnSlowStreaming"),
				TEXT("Block when slow streaming is detected."))
			.Bool(TEXT("createIfMissing"),
				TEXT("Create the target structure if it is missing."))
			.String(TEXT("dataLayerName"), TEXT("Name of the data layer."))
			.Bool(TEXT("bIsInitiallyVisible"),
				TEXT("Data layer initially visible."))
			.Bool(TEXT("bIsInitiallyLoaded"),
				TEXT("Data layer initially loaded."))
			.StringEnum(TEXT("dataLayerType"), {
				TEXT("Runtime"),
				TEXT("Editor")
			}, TEXT("Type of data layer."))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.String(TEXT("hlodLayerName"), TEXT("Name of the HLOD layer."))
			.String(TEXT("hlodLayerPath"), TEXT("Path to HLOD layer."))
			.String(TEXT("layerType"), TEXT("HLOD layer type."))
			.Bool(TEXT("bIsSpatiallyLoaded"),
				TEXT("HLOD is spatially loaded."))
			.Number(TEXT("cellSize"), TEXT("HLOD cell size."))
			.Number(TEXT("loadingDistance"), TEXT("HLOD loading distance."))
			.String(TEXT("volumeName"), TEXT("Name of the volume."))
			.Object(TEXT("volumeLocation"),
				TEXT("Location of the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("volumeExtent"), TEXT("Extent of the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("location"), TEXT("World location for the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("Rotation of the volume."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("extent"),
				TEXT("Extent (half-size) of the volume in each axis."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("sphereRadius"),
				TEXT("Radius for sphere trigger volumes."))
			.Number(TEXT("capsuleRadius"),
				TEXT("Radius for capsule trigger volumes."))
			.Number(TEXT("capsuleHalfHeight"),
				TEXT("Half-height for capsule trigger volumes."))
			.Object(TEXT("boxExtent"),
				TEXT("Extent for box trigger volumes."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("bPainCausing"),
				TEXT("Whether the volume causes pain/damage."))
			.Number(TEXT("damagePerSec"),
				TEXT("Damage per second for pain volumes."))
			.Bool(TEXT("bWaterVolume"),
				TEXT("Whether this is a water volume."))
			.Number(TEXT("fluidFriction"),
				TEXT("Fluid friction for physics volumes."))
			.Number(TEXT("terminalVelocity"),
				TEXT("Terminal velocity in the volume."))
			.Number(TEXT("priority"), TEXT("Priority value."))
			.Bool(TEXT("bEnabled"),
				TEXT("Whether the audio volume is enabled."))
			.Number(TEXT("reverbVolume"),
				TEXT("Volume level for reverb (0.0-1.0)."))
			.Number(TEXT("fadeTime"), TEXT("Fade time in seconds."))
			.ArrayOfObjects(TEXT("cullDistances"),
				TEXT("Array of size/distance pairs for cull distance volumes."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("size"));
				S.Number(TEXT("cullDistance"));
			})
			.Bool(TEXT("bUnbound"),
				TEXT("Whether post process volume affects entire world."))
			.Number(TEXT("blendRadius"),
				TEXT("Blend radius for post process volume."))
			.Number(TEXT("blendWeight"),
				TEXT("Blend weight (0.0-1.0) for post process."))
			.Array(TEXT("bounds"),
				TEXT("Bounds array [minX, minY, minZ, maxX, maxY, maxZ]."),
				TEXT("number"))
			.String(TEXT("filter"), TEXT("General search filter."))
			.String(TEXT("volumeType"),
				TEXT("Type filter for get_volumes_info (e.g., \"Trigger\", \"Physics\")."))
			.String(TEXT("nodeClass"), TEXT("Node class path."))
			.Object(TEXT("nodePosition"),
				TEXT("Position of node in graph."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.String(TEXT("nodeName"), TEXT("Name of the node."))
			.String(TEXT("sourceNodeName"), TEXT("Source node name."))
			.String(TEXT("sourcePinName"), TEXT("Name of the source pin."))
			.String(TEXT("targetNodeName"), TEXT("Target node name."))
			.String(TEXT("targetPinName"), TEXT("Name of the target pin."))
			.String(TEXT("levelInstanceName"),
				TEXT("Level instance name."))
			.String(TEXT("levelAssetPath"),
				TEXT("Path to the level asset for instancing."))
			.Object(TEXT("instanceLocation"),
				TEXT("Location of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("instanceRotation"),
				TEXT("Rotation of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("instanceScale"),
				TEXT("Scale of the level instance."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("packedLevelName"),
				TEXT("Name for the packed level actor."))
			.Bool(TEXT("bPackBlueprints"),
				TEXT("Include blueprints in packed level."))
			.Bool(TEXT("bPackStaticMeshes"),
				TEXT("Include static meshes in packed level."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageLevelStructure);
