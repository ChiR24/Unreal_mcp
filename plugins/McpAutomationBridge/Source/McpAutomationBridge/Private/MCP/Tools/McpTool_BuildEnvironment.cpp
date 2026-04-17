// McpTool_BuildEnvironment.cpp — build_environment tool definition (22 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

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
			.StringEnum(TEXT("action"), {
				TEXT("create_landscape"),
				TEXT("sculpt"),
				TEXT("sculpt_landscape"),
				TEXT("add_foliage"),
				TEXT("paint_foliage"),
				TEXT("create_procedural_terrain"),
				TEXT("create_procedural_foliage"),
				TEXT("add_foliage_instances"),
				TEXT("get_foliage_instances"),
				TEXT("remove_foliage"),
				TEXT("paint_landscape"),
				TEXT("paint_landscape_layer"),
				TEXT("modify_heightmap"),
				TEXT("set_landscape_material"),
				TEXT("create_landscape_grass_type"),
				TEXT("generate_lods"),
				TEXT("bake_lightmap"),
				TEXT("export_snapshot"),
				TEXT("import_snapshot"),
				TEXT("delete"),
				TEXT("create_sky_sphere"),
				TEXT("set_time_of_day"),
				TEXT("create_fog_volume")
			}, TEXT("Action"))
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
			.Object(TEXT("scale"), TEXT("3D scale (x, y, z)."),
				[](FMcpSchemaBuilder& S) {
				S.Number(TEXT("x")).Number(TEXT("y")).Number(TEXT("z"));
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
			.Number(TEXT("brushSize"), TEXT(""))
			.String(TEXT("layerName"), TEXT(""))
			.Bool(TEXT("eraseMode"), TEXT(""))
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
			.Bool(TEXT("enableWorldPartition"), TEXT(""))
			.String(TEXT("runtimeGrid"), TEXT(""))
			.Bool(TEXT("isSpatiallyLoaded"), TEXT(""))
			.Array(TEXT("dataLayers"), TEXT(""))
			.Number(TEXT("count"), TEXT(""))
			.Array(TEXT("assets"), TEXT(""))
			.Number(TEXT("numLODs"), TEXT(""))
			.Number(TEXT("subdivisions"), TEXT(""))
			.FreeformObject(TEXT("settings"), TEXT(""))
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
