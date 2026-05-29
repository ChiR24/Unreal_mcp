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
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_AnimationPhysics);
