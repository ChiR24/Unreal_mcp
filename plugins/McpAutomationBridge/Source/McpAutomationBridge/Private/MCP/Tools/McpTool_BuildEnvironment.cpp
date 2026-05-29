// McpTool_BuildEnvironment.cpp — build_environment tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_BuildEnvironment : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("build_environment"); }

	FString GetDescription() const override
	{
		return TEXT("Create/sculpt landscapes, paint foliage, and generate procedural "
			"terrain/biomes.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	// Pattern A: default GetDispatchAction() returns GetName()

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
				.StringEnum(TEXT("action"), McpConsolidatedActions::BuildEnvironment(),
					TEXT("Action"))
			.String(TEXT("name"), TEXT("Name identifier."))
			.String(TEXT("landscapeName"), TEXT(""))
			.Array(TEXT("heightData"), TEXT(""), TEXT("number"))
			.Number(TEXT("minX"), TEXT(""))
			.Number(TEXT("minY"), TEXT(""))
			.Number(TEXT("maxX"), TEXT(""))
			.Number(TEXT("maxY"), TEXT(""))
			.Bool(TEXT("updateNormals"), TEXT(""))
			.Object(TEXT("location"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.Object(TEXT("rotation"), TEXT("3D rotation (pitch, yaw, roll)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("pitch")).Number(TEXT("yaw")).Number(TEXT("roll"));
			})
			.Number(TEXT("sizeX"), TEXT(""))
			.Number(TEXT("sizeY"), TEXT(""))
			.Number(TEXT("sectionSize"), TEXT(""))
			.Number(TEXT("sectionsPerComponent"), TEXT(""))
			.Object(TEXT("componentCount"), TEXT("2D vector."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y"));
			})
			.String(TEXT("materialPath"), TEXT("Material asset path."))
			.String(TEXT("tool"), TEXT(""))
			.Number(TEXT("radius"), TEXT(""))
			.Number(TEXT("strength"), TEXT(""))
			.Number(TEXT("falloff"), TEXT(""))
			.String(TEXT("layerName"), TEXT(""))
			.String(TEXT("actorName"), TEXT("Name of the actor."))
			.String(TEXT("foliageType"), TEXT(""))
			.String(TEXT("foliageTypePath"),
				TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("meshPath"), TEXT("Mesh asset path."))
			.Number(TEXT("density"), TEXT(""))
			.Number(TEXT("minScale"), TEXT(""))
			.Number(TEXT("maxScale"), TEXT(""))
			.Number(TEXT("cullDistance"), TEXT(""))
			.Bool(TEXT("alignToNormal"), TEXT(""))
			.Bool(TEXT("randomYaw"), TEXT(""))
			.ArrayOfObjects(TEXT("locations"), TEXT(""))
			.ArrayOfObjects(TEXT("transforms"), TEXT(""))
			.Object(TEXT("position"), TEXT("3D location (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
			})
			.FreeformObject(TEXT("bounds"), TEXT(""))
			.String(TEXT("volumeName"), TEXT(""))
			.Number(TEXT("seed"), TEXT(""))
			.ArrayOfObjects(TEXT("foliageTypes"), TEXT(""))
			.Number(TEXT("quadsPerSection"), TEXT(""))
			.Number(TEXT("count"), TEXT(""))
			.Array(TEXT("assets"), TEXT(""))
			.Number(TEXT("numLODs"), TEXT(""))
			.Number(TEXT("subdivisions"), TEXT(""))
			.Number(TEXT("tileSize"), TEXT(""))
			.String(TEXT("quality"), TEXT(""))
			.String(TEXT("staticMesh"), TEXT("Mesh asset path."))
			.Number(TEXT("timeoutMs"), TEXT(""))
			.String(TEXT("path"), TEXT("Path to a directory."))
			.String(TEXT("filename"), TEXT(""))
			.Array(TEXT("assetPaths"), TEXT(""))
			.Array(TEXT("names"), TEXT(""))
			.Number(TEXT("time"), TEXT(""))
			.Number(TEXT("spacing"), TEXT(""))
			.Number(TEXT("heightScale"), TEXT(""))
			.String(TEXT("material"), TEXT("Material asset path."))
			.Number(TEXT("hour"), TEXT(""))
			.Number(TEXT("intensity"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_BuildEnvironment);
