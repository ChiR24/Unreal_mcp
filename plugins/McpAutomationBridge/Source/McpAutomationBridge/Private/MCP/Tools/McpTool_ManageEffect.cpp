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
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset.Asset)."))
			.Number(TEXT("timeoutMs"), TEXT("Client-side request timeout in milliseconds."))
			.Bool(TEXT("save"), TEXT("Whether to save modified assets."))
			.String(TEXT("system"), TEXT("Niagara system asset path alias."))
			.String(TEXT("systemPath"), TEXT("Niagara system asset path."))
			.String(TEXT("systemName"), TEXT("Niagara actor/system label."))
			.String(TEXT("emitter"), TEXT("Emitter name alias."))
			.String(TEXT("emitterName"), TEXT("Emitter name in a Niagara system."))
			.String(TEXT("emitterPath"), TEXT("Niagara emitter asset path."))
			.FreeformObject(TEXT("emitterProperties"), TEXT("Emitter properties to update."))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("attachToActor"), TEXT("Actor label to attach spawned Niagara actor to."))
			.Bool(TEXT("reset"), TEXT(""))
			.String(TEXT("filter"), TEXT("Cleanup actor-label prefix filter."))
			.Number(TEXT("deltaTime"), TEXT("Simulation delta time."))
			.Integer(TEXT("steps"), TEXT("Simulation step count."))
			.String(TEXT("preset"), TEXT("Particle preset or asset path."))
			.String(TEXT("shape"), TEXT(""))
			.String(TEXT("shapeType"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Array(TEXT("color"), TEXT(""), TEXT("number"))
			.Number(TEXT("duration"), TEXT(""))
			.String(TEXT("lightType"), TEXT(""))
			.Number(TEXT("intensity"), TEXT(""))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("scattering"), TEXT(""))
			.Number(TEXT("extinction"), TEXT(""))
			.String(TEXT("modulePath"), TEXT("Niagara module script asset path."))
			.String(TEXT("scriptType"), TEXT("Niagara script target, e.g. Spawn or Update."))
			.Bool(TEXT("autoConnect"), TEXT("Automatically connect a compatible Niagara graph pin pair."))
			.String(TEXT("nodeId"), TEXT("ID of the node."))
			.String(TEXT("parameterName"), TEXT("Name of the parameter."))
			.String(TEXT("parameterType"), TEXT(""))
			.FreeformObject(TEXT("parameterValue"), TEXT("Generic parameter value (any type)."))
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.String(TEXT("sourceBinding"), TEXT("Niagara source binding, e.g. Emitter.Age."))
			.Number(TEXT("spawnRate"), TEXT(""))
			.Number(TEXT("burstCount"), TEXT(""))
			.Number(TEXT("burstTime"), TEXT(""))
			.Number(TEXT("spawnPerUnit"), TEXT(""))
			.Number(TEXT("lifetime"), TEXT(""))
			.Number(TEXT("mass"), TEXT(""))
			.String(TEXT("forceType"), TEXT(""))
			.Number(TEXT("forceStrength"), TEXT(""))
			.Object(TEXT("forceVector"), TEXT("3D force vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("velocity"), TEXT("3D velocity vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("velocityMode"), TEXT(""))
			.Object(TEXT("acceleration"), TEXT("3D acceleration vector (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.String(TEXT("sizeMode"), TEXT(""))
			.Number(TEXT("uniformSize"), TEXT(""))
			.String(TEXT("colorMode"), TEXT(""))
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("alignment"), TEXT("Sprite alignment mode."))
			.String(TEXT("facingMode"), TEXT("Sprite facing mode."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("lightRadius"), TEXT(""))
			.String(TEXT("collisionMode"), TEXT(""))
			.Number(TEXT("restitution"), TEXT(""))
			.Number(TEXT("friction"), TEXT(""))
			.Bool(TEXT("dieOnCollision"), TEXT(""))
			.String(TEXT("killCondition"), TEXT(""))
			.Number(TEXT("cameraOffset"), TEXT(""))
			.String(TEXT("eventName"), TEXT("Name of the event."))
			.String(TEXT("eventType"), TEXT(""))
			.Bool(TEXT("spawnOnEvent"), TEXT(""))
			.Number(TEXT("eventSpawnCount"), TEXT(""))
			.ArrayOfObjects(TEXT("eventPayload"), TEXT("Niagara event payload attributes."),
				[](FMcpSchemaBuilder& S) {
				S.String(TEXT("name"), TEXT("Attribute name."))
				 .String(TEXT("type"), TEXT("Attribute Niagara type."));
			})
			.Bool(TEXT("fixedBoundsEnabled"), TEXT(""))
			.Bool(TEXT("deterministicEnabled"), TEXT(""))
			.String(TEXT("stageName"), TEXT(""))
			.String(TEXT("stageIterationSource"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageEffect);
