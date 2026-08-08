#include "MCP/Registry/McpSchemaBuilder.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"

namespace
{
class FMcpTool_DebugSession : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("debug_session"); }
	FString GetDescription() const override { return TEXT("Manage Unreal native debug sessions. Native control requires the sidecar and VS Code debug host."); }
	FString GetCategory() const override { return TEXT("core"); }
	bool EnforceStrictArguments() const override { return true; }
	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder()
			.StringEnum(TEXT("action"), {TEXT("list_targets"), TEXT("start"), TEXT("status"), TEXT("pause"), TEXT("continue"), TEXT("next"), TEXT("step_in"), TEXT("step_out"), TEXT("stop")}, TEXT("Debug session operation."))
			.StringEnum(TEXT("mode"), {TEXT("pie_observe"), TEXT("standalone_debug"), TEXT("attach")}, TEXT("Session mode."))
			.String(TEXT("sessionId"), TEXT("Debug session identifier."))
			.Number(TEXT("targetPid"), TEXT("Allow-listed Unreal target process ID."))
			.Required({TEXT("action")}).Build();
	}
};

class FMcpTool_DebugBreakpoint : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("debug_breakpoint"); }
	FString GetDescription() const override { return TEXT("Manage source, function, exception, and log breakpoints through the VS Code debug host."); }
	FString GetCategory() const override { return TEXT("core"); }
	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder().StringEnum(TEXT("action"), {TEXT("upsert"), TEXT("remove"), TEXT("list"), TEXT("clear")}, TEXT("Breakpoint operation."))
			.String(TEXT("sessionId"), TEXT("Debug session identifier."))
			.StringEnum(TEXT("kind"), {TEXT("source"), TEXT("function"), TEXT("exception"), TEXT("log")}, TEXT("Breakpoint kind."))
			.String(TEXT("source"), TEXT("Absolute source path."))
			.Number(TEXT("line"), TEXT("One-based source line."))
			.String(TEXT("function"), TEXT("Function breakpoint name."))
			.String(TEXT("condition"), TEXT("Optional condition."))
			.Required({TEXT("action")}).Build();
	}
};

class FMcpTool_DebugInspect : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("debug_inspect"); }
	FString GetDescription() const override { return TEXT("Inspect threads, stacks, scopes, variables, expressions, memory, or a stopped-state snapshot."); }
	FString GetCategory() const override { return TEXT("core"); }
	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder().StringEnum(TEXT("action"), {TEXT("threads"), TEXT("stack"), TEXT("scopes"), TEXT("variables"), TEXT("evaluate"), TEXT("read_memory"), TEXT("snapshot")}, TEXT("Inspection operation."))
			.String(TEXT("sessionId"), TEXT("Debug session identifier."))
			.String(TEXT("expression"), TEXT("Expression to evaluate."))
			.Number(TEXT("threadId"), TEXT("DAP thread ID."))
			.Number(TEXT("frameId"), TEXT("DAP frame ID."))
			.Bool(TEXT("unsafe"), TEXT("Explicit unsafe-operation permission."))
			.Required({TEXT("action")}).Build();
	}
};

class FMcpTool_DebugObserve : public FMcpToolDefinition
{
public:
	FString GetName() const override { return TEXT("debug_observe"); }
	FString GetDescription() const override { return TEXT("Query correlated events and diagnostics, capture probes, run tests and traces, and create artifact bundles."); }
	FString GetCategory() const override { return TEXT("core"); }
	TSharedPtr<FJsonObject> BuildInputSchema() const override
	{
		return FMcpSchemaBuilder().StringEnum(TEXT("action"), {TEXT("query_events"), TEXT("blueprint_diagnostics"), TEXT("probe_snapshot"), TEXT("start_recording"), TEXT("stop_recording"), TEXT("run_tests"), TEXT("test_status"), TEXT("cancel_test"), TEXT("start_trace"), TEXT("stop_trace"), TEXT("trace_status"), TEXT("create_bundle")}, TEXT("Observability operation."))
			.String(TEXT("sessionId"), TEXT("Debug session identifier."))
			.String(TEXT("jobId"), TEXT("Asynchronous job identifier."))
			.Number(TEXT("after"), TEXT("Event cursor."))
			.Number(TEXT("limit"), TEXT("Maximum events."))
			.String(TEXT("regex"), TEXT("Event message filter."))
			.Required({TEXT("action")}).Build();
	}
};
}

MCP_REGISTER_TOOL(FMcpTool_DebugSession);
MCP_REGISTER_TOOL(FMcpTool_DebugBreakpoint);
MCP_REGISTER_TOOL(FMcpTool_DebugInspect);
MCP_REGISTER_TOOL(FMcpTool_DebugObserve);
