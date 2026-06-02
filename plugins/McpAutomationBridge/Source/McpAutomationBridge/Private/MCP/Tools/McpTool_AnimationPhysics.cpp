// McpTool_AnimationPhysics.cpp — animation_physics tool definition (55 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

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
			.StringEnum(TEXT("action"), McpConsolidatedActions::AnimationPhysics(),
				TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("path"), TEXT("Path to save or create the asset."))
			.String(TEXT("assetPath"), TEXT("Asset path."))
			.String(TEXT("animationPath"), TEXT("Animation asset path."))
			.String(TEXT("savePath"), TEXT("Path to save the asset."))
			.String(TEXT("skeletonPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("skeletalMeshPath"), TEXT("Skeletal mesh path."))
			.String(TEXT("parentClass"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("targetSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("slotName"), TEXT(""))
			.String(TEXT("sectionName"), TEXT(""))
			.String(TEXT("notifyName"), TEXT(""))
			.String(TEXT("boneName"), TEXT("Name of the bone."))
			.String(TEXT("curveName"), TEXT(""))
			.String(TEXT("stateName"), TEXT(""))
			.String(TEXT("machineName"), TEXT(""))
			.String(TEXT("interpolationType"), TEXT(""))
			.String(TEXT("axisName"), TEXT(""))
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
			.String(TEXT("vehicleType"), TEXT(""))
			.Number(TEXT("mass"), TEXT(""))
			.Number(TEXT("dragCoefficient"), TEXT(""))
			.Array(TEXT("artifacts"), TEXT(""))
			.String(TEXT("physicsAssetName"), TEXT("Physics asset name for setup_physics_simulation."))
			.Bool(TEXT("assignToMesh"), TEXT("Assign the created physics asset to the skeletal mesh."))
			.String(TEXT("sourceSkeleton"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("sourceIKRigPath"), TEXT("Source IK Rig asset path for create_ik_retargeter."))
			.String(TEXT("targetIKRigPath"), TEXT("Target IK Rig asset path for create_ik_retargeter."))
			.Bool(TEXT("save"), TEXT("Save the asset(s) after the operation."))
			.String(TEXT("additiveAnimType"), TEXT("Additive animation type."))
			.Number(TEXT("basePoseFrame"), TEXT("Base pose frame."))
			.String(TEXT("basePoseType"), TEXT("Base pose type."))
			.Number(TEXT("blendTime"), TEXT("Blend time."))
			.String(TEXT("blendType"), TEXT("Blend type."))
			.String(TEXT("blueprintPath"), TEXT("Blueprint asset path."))
			.ArrayOfObjects(TEXT("boneTracks"), TEXT("Bone track data."))
			.String(TEXT("cacheName"), TEXT("Cached pose name."))
			.Object(TEXT("center"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Bool(TEXT("compileReferencers"), TEXT("Compile referencing animation blueprints."))
			.ArrayOfObjects(TEXT("deltas"), TEXT("Morph target deltas."))
			.Bool(TEXT("enableRootMotion"), TEXT("Enable root motion."))
			.Number(TEXT("endFrame"), TEXT("End frame."))
			.Bool(TEXT("forceRootLock"), TEXT("Force root lock."))
			.String(TEXT("fromSection"), TEXT("Source montage section."))
			.String(TEXT("fromState"), TEXT("Source state."))
			.ArrayOfObjects(TEXT("layerSetup"), TEXT("Layered blend setup."))
			.FreeformObject(TEXT("limits"), TEXT("Constraint angular limits."))
			.Number(TEXT("linearDamping"), TEXT("Linear damping factor."))
			.Number(TEXT("lodIndex"), TEXT("LOD index."))
			.String(TEXT("markerName"), TEXT("Sync marker name."))
			.Number(TEXT("maxValue"), TEXT("Maximum value."))
			.Number(TEXT("minValue"), TEXT("Minimum value."))
			.String(TEXT("montagePath"), TEXT("Animation montage path."))
			.String(TEXT("morphTargetName"), TEXT("Morph target name."))
			.String(TEXT("morphTargetPath"), TEXT("Path to morph target or FBX file for import."))
			.String(TEXT("newBoneName"), TEXT("New bone name."))
			.String(TEXT("nodeName"), TEXT("Animation graph node name."))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.Bool(TEXT("overwrite"), TEXT("Overwrite if the asset/file already exists."))
			.String(TEXT("parentBoneName"), TEXT("Parent bone name."))
			.String(TEXT("physicsAssetPath"), TEXT("Path to physics asset."))
			.String(TEXT("profileName"), TEXT("Skin weight profile name."))
			.String(TEXT("propertyName"), TEXT("Property name."))
			.Number(TEXT("radius"), TEXT("Radius value."))
			.Bool(TEXT("rebuildBlendParameters"), TEXT("Also rebuild blend parameter metadata."))
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
			.Bool(TEXT("removeChildren"), TEXT("Remove child bones or objects."))
			.String(TEXT("rootBoneName"), TEXT("Root bone name."))
			.String(TEXT("rootMotionRootLock"), TEXT("Root motion lock mode."))
			.Number(TEXT("sampleValue"), TEXT("Blend sample value."))
			.Bool(TEXT("simulatePhysics"), TEXT("Enable physics simulation."))
			.String(TEXT("socketName"), TEXT("Name of the socket."))
			.String(TEXT("attachBoneName"), TEXT("Bone to attach to."))
			.String(TEXT("sourceBoneName"), TEXT("Source bone name."))
			.String(TEXT("sourceChain"), TEXT("Source retarget chain."))
			.String(TEXT("sourceMeshPath"), TEXT("Source mesh path."))
			.Number(TEXT("startFrame"), TEXT("Start frame."))
			.Number(TEXT("startTime"), TEXT("Start time."))
			.String(TEXT("stateMachineName"), TEXT("State machine name."))
			.String(TEXT("suffix"), TEXT("Name suffix."))
			.String(TEXT("targetBoneName"), TEXT("Target bone name."))
			.String(TEXT("targetChain"), TEXT("Target retarget chain."))
			.String(TEXT("targetMeshPath"), TEXT("Target mesh path."))
			.Number(TEXT("threshold"), TEXT("Weight threshold."))
			.String(TEXT("toSection"), TEXT("Target montage section."))
			.String(TEXT("toState"), TEXT("Target state."))
			.Number(TEXT("trackIndex"), TEXT("Track index."))
			.ArrayOfObjects(TEXT("weights"), TEXT("Skin weight data."))
			.Number(TEXT("yaw"), TEXT("Yaw value."))
			.String(TEXT("notifyClass"), TEXT("Anim notify class."))
			.String(TEXT("clothAssetPath"), TEXT("Path to cloth asset."))
			.Number(TEXT("vertexIndex"), TEXT("Vertex index."))
			.Array(TEXT("vertexIndices"), TEXT("Array of vertex indices."), TEXT("number"))
			.Number(TEXT("boneIndex"), TEXT("Bone index."))
			.String(TEXT("bodyName"), TEXT("Physics body name."))
			.String(TEXT("bodyType"), TEXT("Physics body shape type."))
			.String(TEXT("constraintName"), TEXT("Constraint name."))
			.String(TEXT("bodyA"), TEXT("First physics body."))
			.String(TEXT("bodyB"), TEXT("Second physics body."))
			.Number(TEXT("angularDamping"), TEXT("Angular damping factor."))
			.Bool(TEXT("collisionEnabled"), TEXT("Enable collision."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_AnimationPhysics);
