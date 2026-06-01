#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"
#include "MCP/McpConsolidatedActionRouting.h"

class FMcpTool_ManagePCG : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_pcg"); }

	FString GetDescription() const override
	{
		return TEXT("Create, edit, execute, and configure PCG graphs: graph assets, input/sampler/filter/spawner nodes, "
			"pin connections, node settings, and partition grid size.");
	}

	FString GetCategory() const override { return TEXT("world"); }

	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), McpConsolidatedActions::PCG(),
				TEXT("PCG graph action to perform."))
			.String(TEXT("graphPath"), TEXT("PCG graph asset path (e.g., /Game/PCG/PCG_MyGraph)."))
			.String(TEXT("parentGraphPath"), TEXT("Parent PCG graph asset path for subgraph insertion."))
			.String(TEXT("subgraphPath"), TEXT("PCG subgraph asset path used by subgraph nodes."))
			.String(TEXT("assetPath"), TEXT("Asset path (e.g., /Game/Path/Asset)."))
			.String(TEXT("path"), TEXT("Directory path for asset creation."))
			.String(TEXT("name"), TEXT("Name of the graph or node."))
			.String(TEXT("nodeType"), TEXT("PCG node type alias or UPCGSettings class path/name."))
			.String(TEXT("settingsClass"), TEXT("UPCGSettings class path/name for the PCG node."))
			.String(TEXT("nodeId"), TEXT("PCG node object name, authored title, or index."))
			.String(TEXT("nodeName"), TEXT("PCG node authored title."))
			.String(TEXT("title"), TEXT("PCG node title."))
			.String(TEXT("sourceNodeId"), TEXT("Source PCG node identifier."))
			.String(TEXT("targetNodeId"), TEXT("Target PCG node identifier."))
			.String(TEXT("sourcePin"), TEXT("Source output pin label."))
			.String(TEXT("targetPin"), TEXT("Target input pin label."))
			.String(TEXT("inputName"), TEXT("Target input pin label alias."))
			.String(TEXT("outputName"), TEXT("Source output pin label alias."))
			.String(TEXT("actorName"), TEXT("Actor name for PCG component execution or actor-based settings."))
			.String(TEXT("componentName"), TEXT("PCG component name for execution."))
			.String(TEXT("componentPath"), TEXT("PCG component path/name for execution."))
			.String(TEXT("classPath"), TEXT("Class path/name for actor-spawner settings or class-based PCG settings."))
			.String(TEXT("actorClass"), TEXT("Actor class path/name for spawn actor PCG nodes."))
			.String(TEXT("meshPath"), TEXT("Static mesh asset path for mesh/spawner PCG settings."))
			.String(TEXT("texturePath"), TEXT("Texture asset path for texture sampler PCG settings."))
			.StringEnum(TEXT("scope"), {TEXT("world"), TEXT("component")}, TEXT("Partition grid target scope."))
			.Bool(TEXT("createComponent"), TEXT("Create a PCG component on actor when executing and none exists."))
			.Bool(TEXT("force"), TEXT("Force PCG generation."))
			.Bool(TEXT("wait"), TEXT("Reserved for future synchronous generation waiting."))
			.Number(TEXT("gridSize"), TEXT("PCG partition grid size in centimeters."))
			.FreeformObject(TEXT("settings"), TEXT("Node settings keyed by reflected PCG settings property name."))
			.Number(TEXT("x"), TEXT("Node editor X position."))
			.Number(TEXT("y"), TEXT("Node editor Y position."))
			.Number(TEXT("posX"), TEXT("Node editor X position alias."))
			.Number(TEXT("posY"), TEXT("Node editor Y position alias."))
			.Bool(TEXT("save"), TEXT("Save modified PCG assets."))
			.Bool(TEXT("overwrite"), TEXT("Allow an existing graph asset to be reused."))
			.Number(TEXT("timeoutMs"), TEXT("Client request timeout in milliseconds."))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManagePCG);
