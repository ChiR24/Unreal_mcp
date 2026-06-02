// McpTool_ManageVolumes.cpp — manage_volumes tool definition (27 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageVolumes : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_volumes"); }

	FString GetDescription() const override
	{
		return TEXT("Create trigger volumes, blocking volumes, physics volumes, "
			"audio volumes, and navigation bounds.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_trigger_volume"),
				TEXT("add_trigger_volume"),
				TEXT("create_trigger_box"),
				TEXT("create_trigger_sphere"),
				TEXT("create_trigger_capsule"),
				TEXT("create_blocking_volume"),
				TEXT("add_blocking_volume"),
				TEXT("create_kill_z_volume"),
				TEXT("add_kill_z_volume"),
				TEXT("create_pain_causing_volume"),
				TEXT("create_physics_volume"),
				TEXT("add_physics_volume"),
				TEXT("create_audio_volume"),
				TEXT("create_reverb_volume"),
				TEXT("create_cull_distance_volume"),
				TEXT("add_cull_distance_volume"),
				TEXT("create_precomputed_visibility_volume"),
				TEXT("create_lightmass_importance_volume"),
				TEXT("create_nav_mesh_bounds_volume"),
				TEXT("create_nav_modifier_volume"),
				TEXT("create_camera_blocking_volume"),
				TEXT("create_post_process_volume"),
				TEXT("add_post_process_volume"),
				TEXT("set_volume_extent"),
				TEXT("set_volume_bounds"),
				TEXT("set_volume_properties"),
				TEXT("remove_volume"),
				TEXT("get_volumes_info")
			}, TEXT("Volume action to perform"))
			.String(TEXT("volumeName"), TEXT("Name of the volume."))
			.String(TEXT("actorPath"), TEXT("Path to actor."))
			.Object(TEXT("location"),
				TEXT("World location for the volume."),
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
				TEXT("Array of size/distance pairs for cull distance volumes."))
			.Bool(TEXT("bUnbound"),
				TEXT("Whether post process volume affects entire world."))
			.Number(TEXT("blendRadius"),
				TEXT("Blend radius for post process volume."))
			.Number(TEXT("blendWeight"),
				TEXT("Blend weight (0.0-1.0) for post process."))
			.String(TEXT("filter"), TEXT("General search filter."))
			.String(TEXT("volumeType"),
				TEXT("Type filter for get_volumes_info "
					"(e.g., \"Trigger\", \"Physics\")."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageVolumes);
