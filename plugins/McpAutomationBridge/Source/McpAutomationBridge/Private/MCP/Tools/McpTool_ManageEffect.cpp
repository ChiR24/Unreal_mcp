// McpTool_ManageEffect.cpp — manage_effect tool definition (58 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageEffect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_effect"); }

	FString GetDescription() const override
	{
		return TEXT("Niagara particle systems, VFX, debug shapes, and GPU simulations. "
			"Create systems, emitters, modules, and control particle effects.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	FString GetDispatchAction() const override { return FString(); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("particle"),
				TEXT("niagara"),
				TEXT("debug_shape"),
				TEXT("spawn_niagara"),
				TEXT("create_dynamic_light"),
				TEXT("create_niagara_system"),
				TEXT("create_niagara_emitter"),
				TEXT("create_volumetric_fog"),
				TEXT("create_particle_trail"),
				TEXT("create_environment_effect"),
				TEXT("create_impact_effect"),
				TEXT("create_niagara_ribbon"),
				TEXT("activate"),
				TEXT("activate_effect"),
				TEXT("deactivate"),
				TEXT("reset"),
				TEXT("advance_simulation"),
				TEXT("add_niagara_module"),
				TEXT("connect_niagara_pins"),
				TEXT("remove_niagara_node"),
				TEXT("set_niagara_parameter"),
				TEXT("clear_debug_shapes"),
				TEXT("cleanup"),
				TEXT("list_debug_shapes"),
				TEXT("add_emitter_to_system"),
				TEXT("set_emitter_properties"),
				TEXT("add_spawn_rate_module"),
				TEXT("add_spawn_burst_module"),
				TEXT("add_spawn_per_unit_module"),
				TEXT("add_initialize_particle_module"),
				TEXT("add_particle_state_module"),
				TEXT("add_force_module"),
				TEXT("add_velocity_module"),
				TEXT("add_acceleration_module"),
				TEXT("add_size_module"),
				TEXT("add_color_module"),
				TEXT("add_sprite_renderer_module"),
				TEXT("add_mesh_renderer_module"),
				TEXT("add_ribbon_renderer_module"),
				TEXT("add_light_renderer_module"),
				TEXT("add_collision_module"),
				TEXT("add_kill_particles_module"),
				TEXT("add_camera_offset_module"),
				TEXT("add_user_parameter"),
				TEXT("set_parameter_value"),
				TEXT("bind_parameter_to_source"),
				TEXT("add_skeletal_mesh_data_interface"),
				TEXT("add_static_mesh_data_interface"),
				TEXT("add_spline_data_interface"),
				TEXT("add_audio_spectrum_data_interface"),
				TEXT("add_collision_query_data_interface"),
				TEXT("add_event_generator"),
				TEXT("add_event_receiver"),
				TEXT("configure_event_payload"),
				TEXT("enable_gpu_simulation"),
				TEXT("add_simulation_stage"),
				TEXT("get_niagara_info"),
				TEXT("validate_niagara_system")
			}, TEXT("Effect/Niagara action to perform."))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("template"), TEXT(""))
			.String(TEXT("system"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("systemPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("systemName"), TEXT(""))
			.String(TEXT("emitter"), TEXT(""))
			.String(TEXT("emitterName"), TEXT(""))
			.String(TEXT("emitterTemplate"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("effect"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("effectId"), TEXT(""))
			.String(TEXT("effectHandle"), TEXT(""))
			.String(TEXT("niagaraHandle"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.Bool(TEXT("reset"), TEXT(""))
			.Number(TEXT("time"), TEXT(""))
			.String(TEXT("shape"), TEXT(""))
			.String(TEXT("shapeType"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Array(TEXT("color"), TEXT(""), TEXT("number"))
			.Number(TEXT("duration"), TEXT(""))
			.String(TEXT("lightType"), TEXT(""))
			.Number(TEXT("intensity"), TEXT(""))
			.String(TEXT("preset"), TEXT(""))
			.String(TEXT("type"), TEXT(""))
			.Number(TEXT("width"), TEXT(""))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("scattering"), TEXT(""))
			.String(TEXT("attachTo"), TEXT(""))
			.String(TEXT("ribbonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("surfaceType"), TEXT(""))
			.String(TEXT("impactType"), TEXT(""))
			.String(TEXT("effectType"), TEXT(""))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.String(TEXT("parameterType"), TEXT(""))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("moduleName"), TEXT(""))
			.String(TEXT("fromNode"), TEXT(""))
			.String(TEXT("toNode"), TEXT(""))
			.String(TEXT("fromPin"), TEXT("Name of the source pin."))
			.String(TEXT("toPin"), TEXT("Name of the target pin."))
			.String(TEXT("outputPin"), TEXT("Name of the source pin."))
			.String(TEXT("inputPin"), TEXT("Name of the target pin."))
			.String(TEXT("node"), TEXT(""))
			.String(TEXT("loopBehavior"), TEXT(""))
			.Number(TEXT("spawnRate"), TEXT(""))
			.Number(TEXT("count"), TEXT(""))
			.Number(TEXT("loopCount"), TEXT(""))
			.Number(TEXT("unitsPerSpawn"), TEXT(""))
			.FreeformObject(TEXT("attributes"), TEXT(""))
			.String(TEXT("updateScript"), TEXT(""))
			.String(TEXT("forceType"), TEXT(""))
			.Number(TEXT("strength"), TEXT(""))
			.String(TEXT("velocityMode"), TEXT(""))
			.Number(TEXT("speedMin"), TEXT(""))
			.Number(TEXT("speedMax"), TEXT(""))
			.Object(TEXT("acceleration"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("sizeMode"), TEXT(""))
			.Object(TEXT("sizeMin"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("sizeMax"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("colorMode"), TEXT(""))
			.Array(TEXT("gradientStart"), TEXT(""), TEXT("number"))
			.Array(TEXT("gradientEnd"), TEXT(""), TEXT("number"))
			.String(TEXT("material"), TEXT("Material asset path."))
			.String(TEXT("mesh"), TEXT("Mesh asset path."))
			.Number(TEXT("lightIntensity"), TEXT(""))
			.Number(TEXT("lightRadius"), TEXT(""))
			.String(TEXT("collisionMode"), TEXT(""))
			.Number(TEXT("collisionRadius"), TEXT(""))
			.String(TEXT("killCondition"), TEXT(""))
			.String(TEXT("offsetMode"), TEXT(""))
			.Number(TEXT("offsetAmount"), TEXT(""))
			.String(TEXT("paramName"), TEXT(""))
			.String(TEXT("paramType"), TEXT(""))
			.String(TEXT("sourceActor"), TEXT(""))
			.String(TEXT("skeletalMesh"), TEXT("Mesh asset path."))
			.String(TEXT("staticMesh"), TEXT("Mesh asset path."))
			.String(TEXT("splineComponent"), TEXT(""))
			.String(TEXT("audioComponent"), TEXT(""))
			.String(TEXT("queryChannel"), TEXT(""))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("condition"), TEXT(""))
			.String(TEXT("receiverScript"), TEXT(""))
			.FreeformObject(TEXT("payload"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT(""))
			.String(TEXT("stageName"), TEXT(""))
			.String(TEXT("stageType"), TEXT(""))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageEffect);
