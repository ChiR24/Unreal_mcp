// McpTool_AnimationPhysics.cpp — animation_physics tool definition (55 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_AnimationPhysics : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("animation_physics"); }

	FString GetDescription() const override
	{
		return TEXT("Create animation blueprints, blend spaces, montages, "
			"state machines, Control Rig, IK rigs, ragdolls, and vehicle physics.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_animation_blueprint"),
				TEXT("create_animation_bp"),
				TEXT("create_anim_blueprint"),
				TEXT("create_blend_space"),
				TEXT("create_blend_space_1d"),
				TEXT("create_blend_space_2d"),
				TEXT("create_blend_tree"),
				TEXT("create_procedural_anim"),
				TEXT("create_aim_offset"),
				TEXT("add_aim_offset_sample"),
				TEXT("create_state_machine"),
				TEXT("add_state_machine"),
				TEXT("add_state"),
				TEXT("add_transition"),
				TEXT("set_transition_rules"),
				TEXT("add_blend_node"),
				TEXT("add_cached_pose"),
				TEXT("add_slot_node"),
				TEXT("create_control_rig"),
				TEXT("add_control"),
				TEXT("add_rig_unit"),
				TEXT("connect_rig_elements"),
				TEXT("create_ik_rig"),
				TEXT("add_ik_chain"),
				TEXT("setup_ik"),
				TEXT("create_pose_library"),
				TEXT("create_animation_asset"),
				TEXT("create_animation_sequence"),
				TEXT("set_sequence_length"),
				TEXT("add_bone_track"),
				TEXT("set_bone_key"),
				TEXT("set_curve_key"),
				TEXT("create_montage"),
				TEXT("add_montage_section"),
				TEXT("add_montage_slot"),
				TEXT("set_section_timing"),
				TEXT("add_montage_notify"),
				TEXT("set_blend_in"),
				TEXT("set_blend_out"),
				TEXT("link_sections"),
				TEXT("add_notify"),
				TEXT("play_montage"),
				TEXT("play_anim_montage"),
				TEXT("setup_ragdoll"),
				TEXT("activate_ragdoll"),
				TEXT("configure_vehicle"),
				TEXT("setup_physics_simulation"),
				TEXT("add_blend_sample"),
				TEXT("set_axis_settings"),
				TEXT("set_interpolation_settings"),
				TEXT("setup_retargeting"),
				TEXT("cleanup")
			}, TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.String(TEXT("parentClass"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("skeletonName"), TEXT(""))
			.String(TEXT("targetSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("blueprintName"), TEXT(""))
			.String(TEXT("montageName"), TEXT(""))
			.String(TEXT("animSequence"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("animPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("animAssetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("animMontagePath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("slotName"), TEXT(""))
			.String(TEXT("sectionName"), TEXT(""))
			.String(TEXT("notifyName"), TEXT(""))
			.String(TEXT("boneName"), TEXT("Name of the bone."))
			.String(TEXT("curveName"), TEXT(""))
			.String(TEXT("stateName"), TEXT(""))
			.String(TEXT("machineName"), TEXT(""))
			.String(TEXT("transitionName"), TEXT(""))
			.String(TEXT("blendSpacePath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Number(TEXT("numSamples"), TEXT(""))
			.String(TEXT("interpolationType"), TEXT(""))
			.String(TEXT("axisName"), TEXT(""))
			.Number(TEXT("min"), TEXT(""))
			.Number(TEXT("max"), TEXT(""))
			.ArrayOfObjects(TEXT("samples"), TEXT(""))
			.String(TEXT("animSequencePath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.Number(TEXT("playRate"), TEXT(""))
			.Number(TEXT("frame"), TEXT(""))
			.Number(TEXT("time"), TEXT(""))
			.Number(TEXT("length"), TEXT(""))
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
			.FreeformObject(TEXT("value"), TEXT("Generic value (any type)."))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.String(TEXT("rigPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("chainName"), TEXT(""))
			.String(TEXT("startBone"), TEXT("Name of the bone."))
			.String(TEXT("endBone"), TEXT("Name of the bone."))
			.String(TEXT("controlName"), TEXT(""))
			.String(TEXT("unitType"), TEXT(""))
			.String(TEXT("sourceNode"), TEXT(""))
			.String(TEXT("targetNode"), TEXT(""))
			.String(TEXT("sourcePin"), TEXT(""))
			.String(TEXT("targetPin"), TEXT(""))
			.String(TEXT("vehicleType"), TEXT(""))
			.FreeformObject(TEXT("wheelConfig"), TEXT(""))
			.Number(TEXT("engineTorque"), TEXT(""))
			.Number(TEXT("mass"), TEXT(""))
			.Number(TEXT("dragCoefficient"), TEXT(""))
			.Array(TEXT("artifacts"), TEXT(""))
			.Bool(TEXT("ragdollActive"), TEXT(""))
			.String(TEXT("sourceSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("retargetSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("retargetProfile"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_AnimationPhysics);
