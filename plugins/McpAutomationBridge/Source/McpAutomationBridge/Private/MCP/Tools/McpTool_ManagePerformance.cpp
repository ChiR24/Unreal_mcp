// McpTool_ManagePerformance.cpp — manage_performance tool definition (20 actions)

#include "McpVersionCompatibility.h"
#include "MCP/McpToolDefinition.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpSchemaBuilder.h"

class FMcpTool_ManagePerformance : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("manage_performance"); }

	FString GetDescription() const override
	{
		return TEXT("Run profiling/benchmarks, configure scalability, LOD, Nanite, "
			"and optimization settings.");
	}

	FString GetCategory() const override { return TEXT("utility"); }


	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {
				TEXT("start_profiling"),
				TEXT("stop_profiling"),
				TEXT("run_benchmark"),
				TEXT("show_fps"),
				TEXT("show_stats"),
				TEXT("generate_memory_report"),
				TEXT("set_scalability"),
				TEXT("set_resolution_scale"),
				TEXT("set_vsync"),
				TEXT("set_frame_rate_limit"),
				TEXT("enable_gpu_timing"),
				TEXT("configure_texture_streaming"),
				TEXT("configure_lod"),
				TEXT("apply_baseline_settings"),
				TEXT("optimize_draw_calls"),
				TEXT("merge_actors"),
				TEXT("configure_occlusion_culling"),
				TEXT("optimize_shaders"),
				TEXT("configure_nanite"),
				TEXT("configure_world_partition")
			}, TEXT("Action"))
			.StringEnum(TEXT("type"), {
				TEXT("CPU"),
				TEXT("GPU"),
				TEXT("Memory"),
				TEXT("RenderThread"),
				TEXT("GameThread"),
				TEXT("All")
			}, TEXT(""))
			.Number(TEXT("duration"), TEXT(""))
			.String(TEXT("outputPath"), TEXT("Output file or directory path."))
			.Bool(TEXT("detailed"), TEXT(""))
			.String(TEXT("category"), TEXT(""))
			.Number(TEXT("level"), TEXT(""))
			.Number(TEXT("scale"), TEXT(""))
			.Bool(TEXT("enabled"), TEXT("Whether the item/feature is enabled."))
			.Number(TEXT("maxFPS"), TEXT(""))
			.Bool(TEXT("verbose"), TEXT(""))
			.Number(TEXT("poolSize"), TEXT(""))
			.Bool(TEXT("boostPlayerLocation"), TEXT(""))
			.Number(TEXT("forceLOD"), TEXT(""))
			.Number(TEXT("lodBias"), TEXT(""))
			.Number(TEXT("distanceScale"), TEXT(""))
			.Number(TEXT("skeletalBias"), TEXT(""))
			.Bool(TEXT("hzb"), TEXT(""))
			.Bool(TEXT("enableInstancing"), TEXT(""))
			.Bool(TEXT("enableBatching"), TEXT(""))
			.Bool(TEXT("mergeActors"), TEXT(""))
			.Array(TEXT("actors"), TEXT(""))
			.Bool(TEXT("freezeRendering"), TEXT(""))
			.Bool(TEXT("compileOnDemand"), TEXT(""))
			.Bool(TEXT("cacheShaders"), TEXT(""))
			.Bool(TEXT("reducePermutations"), TEXT(""))
			.Number(TEXT("maxPixelsPerEdge"), TEXT(""))
			.Number(TEXT("streamingPoolSize"), TEXT(""))
			.Number(TEXT("streamingDistance"), TEXT(""))
			.Number(TEXT("cellSize"), TEXT(""))
			.Required({TEXT("action")})
			.Build();
	}
};

MCP_REGISTER_TOOL(FMcpTool_ManagePerformance);
