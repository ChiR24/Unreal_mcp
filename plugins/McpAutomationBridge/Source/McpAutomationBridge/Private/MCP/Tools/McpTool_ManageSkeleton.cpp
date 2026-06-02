// McpTool_ManageSkeleton.cpp — manage_skeleton tool definition (29 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManageSkeleton : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_skeleton"); }

	FString GetDescription() const override
	{
		return TEXT("Edit skeletal meshes: add sockets, configure physics assets, "
			"set skin weights, and create morph targets.");
	}

	FString GetCategory() const override { return TEXT("gameplay"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("create_skeleton"),
				TEXT("add_bone"),
				TEXT("remove_bone"),
				TEXT("rename_bone"),
				TEXT("set_bone_transform"),
				TEXT("set_bone_parent"),
				TEXT("create_virtual_bone"),
				TEXT("create_socket"),
				TEXT("configure_socket"),
				TEXT("auto_skin_weights"),
				TEXT("set_vertex_weights"),
				TEXT("normalize_weights"),
				TEXT("prune_weights"),
				TEXT("copy_weights"),
				TEXT("mirror_weights"),
				TEXT("create_physics_asset"),
				TEXT("add_physics_body"),
				TEXT("configure_physics_body"),
				TEXT("add_physics_constraint"),
				TEXT("configure_constraint_limits"),
				TEXT("bind_cloth_to_skeletal_mesh"),
				TEXT("assign_cloth_asset_to_mesh"),
				TEXT("create_morph_target"),
				TEXT("set_morph_target_deltas"),
				TEXT("import_morph_targets"),
				TEXT("get_skeleton_info"),
				TEXT("list_bones"),
				TEXT("list_sockets"),
				TEXT("list_physics_bodies")
			}, TEXT("Skeleton action to perform"))
			.String(TEXT("skeletonPath"), TEXT("Skeleton asset path."))
			.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
			.String(TEXT("physicsAssetPath"), TEXT("Path to physics asset."))
			.String(TEXT("morphTargetPath"),
				TEXT("Path to morph target or FBX file for import."))
			.String(TEXT("clothAssetPath"), TEXT("Path to cloth asset."))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.String(TEXT("boneName"), TEXT("Name of the bone."))
			.String(TEXT("newBoneName"), TEXT("New name for renaming."))
			.String(TEXT("parentBoneName"), TEXT("Parent bone name."))
			.String(TEXT("sourceBoneName"), TEXT("Source bone name."))
			.String(TEXT("targetBoneName"), TEXT("Target bone name."))
			.Number(TEXT("boneIndex"), TEXT("Bone index for operations."))
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
			.String(TEXT("socketName"), TEXT("Name of the socket."))
			.String(TEXT("attachBoneName"), TEXT("Bone to attach to."))
			.Object(TEXT("relativeLocation"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("relativeRotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Object(TEXT("relativeScale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Number(TEXT("vertexIndex"), TEXT("Vertex index for weight operations."))
			.Array(TEXT("vertexIndices"), TEXT("Array of vertex indices."),
				TEXT("number"))
			.ArrayOfObjects(TEXT("weights"),
				TEXT("Array of {boneIndex, weight} pairs."))
			.Number(TEXT("threshold"), TEXT("Weight threshold for pruning (0-1)."))
			.StringEnum(TEXT("mirrorAxis"), {
				TEXT("X"),
				TEXT("Y"),
				TEXT("Z")
			}, TEXT("Axis for weight mirroring."))
			.FreeformObject(TEXT("mirrorTable"),
				TEXT("Bone name mapping for mirroring."))
			.StringEnum(TEXT("bodyType"), {
				TEXT("Capsule"),
				TEXT("Sphere"),
				TEXT("Box"),
				TEXT("Convex"),
				TEXT("Sphyl")
			}, TEXT("Physics body shape type."))
			.String(TEXT("bodyName"), TEXT("Physics body name."))
			.Number(TEXT("mass"), TEXT("Mass value."))
			.Number(TEXT("linearDamping"), TEXT("Linear damping factor."))
			.Number(TEXT("angularDamping"), TEXT("Angular damping factor."))
			.Bool(TEXT("collisionEnabled"),
				TEXT("Enable collision for this body."))
			.Bool(TEXT("simulatePhysics"), TEXT("Enable physics simulation."))
			.String(TEXT("constraintName"), TEXT("Constraint name."))
			.String(TEXT("bodyA"), TEXT("First physics body."))
			.String(TEXT("bodyB"), TEXT("Second physics body."))
			.Object(TEXT("limits"), TEXT("Constraint angular limits."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("swing1LimitAngle"), TEXT("Swing 1 limit in degrees."))
					.Number(TEXT("swing2LimitAngle"), TEXT("Swing 2 limit in degrees."))
					.Number(TEXT("twistLimitAngle"), TEXT("Twist limit in degrees."))
					.StringEnum(TEXT("swing1Motion"), {
						TEXT("Free"), TEXT("Limited"), TEXT("Locked")
					}, TEXT(""))
					.StringEnum(TEXT("swing2Motion"), {
						TEXT("Free"), TEXT("Limited"), TEXT("Locked")
					}, TEXT(""))
					.StringEnum(TEXT("twistMotion"), {
						TEXT("Free"), TEXT("Limited"), TEXT("Locked")
					}, TEXT(""));
			})
			.String(TEXT("morphTargetName"), TEXT("Morph target name."))
			.ArrayOfObjects(TEXT("deltas"),
				TEXT("Array of {vertexIndex, delta} for morph target."))
			.Number(TEXT("paintValue"), TEXT("Cloth weight paint value (0-1)."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.Bool(TEXT("overwrite"),
				TEXT("Overwrite if the asset/file already exists."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManageSkeleton);
