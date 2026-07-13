// McpNativeGatewayDescribe.cpp — registry-fallback describe for the unreal gateway.
// Mirrors GatewayDescribeFromManifest (generated McpNativeGatewayManifest.h); used only
// when the neutral generated manifest fails to load. Progressive drill-down:
// tool summary -> action parameter catalog -> single param schema, with guided errors.

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayManifest.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Protocol/McpJsonRpc.h"

TSharedPtr<FJsonObject> DescribeGatewayCapability(
	const FString& ToolName, const FString& Action,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	const FString& Param, const FString& Query, int32 Limit, int32 Offset)
{
	// Prefer the neutral generated manifest (single source of truth with the TS gateway).
	if (TSharedPtr<FJsonObject> ManifestResult = GatewayDescribeFromManifest(ToolName, Action, ToolManager, Param, Query, Limit, Offset))
	{
		return ManifestResult;
	}

	FMcpToolDefinition* Tool = Registry.FindTool(ToolName);
	if (!Tool)
	{
		// UNKNOWN_TOOL guidance is produced by the manifest path; the registry fallback runs
		// only when the generated manifest fails to load, so it returns the canonical envelope.
		return GatewayError(TEXT("describe"), TEXT("UNKNOWN_TOOL"),
			TEXT("Unknown tool. Call search to retrieve canonical tool names."));
	}

	const TArray<FString> Actions = GatewayGetActionValues(Tool);
	if (!Action.IsEmpty() && !Actions.Contains(Action))
	{
		TArray<FString> Suggestions = GatewayClosestMatches(Action, Actions, 3);
		TSharedPtr<FJsonObject> Err = GatewayError(TEXT("describe"), TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action '%s' for %s."), *Action, *ToolName));
		Err->SetStringField(TEXT("tool"), ToolName);
		Err->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
		Err->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
		Err->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), ToolName, Suggestions.Num() > 0 ? Suggestions[0] : FString(), FString()));
		return Err;
	}

	const TArray<FString> ParamNames = GatewayGetParameterNames(Tool);
	if (!Param.IsEmpty())
	{
		if (!ParamNames.Contains(Param))
		{
			TArray<FString> Suggestions = GatewayClosestMatches(Param, ParamNames, 3);
			TSharedPtr<FJsonObject> Err = GatewayError(TEXT("describe"), TEXT("UNKNOWN_PARAM"),
				FString::Printf(TEXT("Unknown parameter '%s' for %s."), *Param, *ToolName));
			Err->SetStringField(TEXT("tool"), ToolName);
			if (!Action.IsEmpty()) Err->SetStringField(TEXT("action"), Action);
			Err->SetArrayField(TEXT("availableParameters"), GatewayStringArray(ParamNames));
			Err->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
			Err->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), ToolName, Action, Suggestions.Num() > 0 ? Suggestions[0] : FString()));
			return Err;
		}
		TSharedPtr<FJsonObject> InputSchema = Tool->BuildInputSchema();
		const TSharedPtr<FJsonObject>* Props = nullptr;
		TSharedPtr<FJsonObject> ParamSchema = MakeShared<FJsonObject>();
		if (InputSchema.IsValid() && InputSchema->TryGetObjectField(TEXT("properties"), Props) && Props)
		{
			const TSharedPtr<FJsonObject>* P = nullptr;
			if ((*Props)->TryGetObjectField(Param, P) && P) ParamSchema = *P;
		}
		const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
		bool bRequired = false;
		if (InputSchema.IsValid() && InputSchema->TryGetArrayField(TEXT("required"), Required) && Required)
		{
			for (const TSharedPtr<FJsonValue>& V : *Required)
			{
				FString S; if (V->TryGetString(S) && S == Param) { bRequired = true; break; }
			}
		}
		auto Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("success"), true);
		Out->SetStringField(TEXT("operation"), TEXT("describe"));
		Out->SetStringField(TEXT("tool"), ToolName);
		if (!Action.IsEmpty()) Out->SetStringField(TEXT("action"), Action);
		Out->SetStringField(TEXT("param"), Param);
		Out->SetBoolField(TEXT("required"), bRequired);
		Out->SetObjectField(TEXT("schema"), ParamSchema);
		Out->SetStringField(TEXT("scope"), TEXT("union"));
		Out->SetBoolField(TEXT("perActionSchemas"), false);
		Out->SetStringField(TEXT("message"),
			TEXT("This parameter belongs to the tool-union catalog (per-action parameter mappings do not exist). Pass it only when relevant to the selected action, using the exact casing shown."));
		return Out;
	}

	const FString Q = Query.ToLower();
	if (Action.IsEmpty())
	{
		TArray<FString> Filtered = Actions;
		if (!Q.IsEmpty()) { Filtered.Empty(); for (const FString& A : Actions) if (A.ToLower().Contains(Q)) Filtered.Add(A); }
		const int32 Total = Filtered.Num();
		TArray<FString> Paged;
		for (int32 i = Offset; i < Total && Paged.Num() < Limit; ++i) Paged.Add(Filtered[i]);
		auto Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("success"), true);
		Out->SetStringField(TEXT("operation"), TEXT("describe"));
		Out->SetStringField(TEXT("tool"), ToolName);
		Out->SetStringField(TEXT("category"), Tool->GetCategory());
		Out->SetStringField(TEXT("description"), Tool->GetDescription());
		Out->SetBoolField(TEXT("enabled"), ToolManager.IsToolEnabled(ToolName));
		Out->SetArrayField(TEXT("actions"), GatewayStringArray(Paged));
		Out->SetNumberField(TEXT("actionCount"), Total);
		Out->SetNumberField(TEXT("actionOffset"), Offset);
		Out->SetNumberField(TEXT("actionLimit"), Limit);
		Out->SetBoolField(TEXT("actionHasMore"), Offset + Paged.Num() < Total);
		Out->SetStringField(TEXT("scope"), TEXT("tool"));
		Out->SetBoolField(TEXT("perActionSchemas"), false);
		Out->SetObjectField(TEXT("drillDown"), GatewayBuildNextCall(TEXT("describe"), ToolName, Paged.Num() > 0 ? Paged[0] : (Actions.Num() > 0 ? Actions[0] : FString()), FString()));
		Out->SetStringField(TEXT("message"),
			TEXT("Tool summary. Drill into an action (drillDown) to list its parameter catalog - parameters are the tool-union, not action-specific."));
		return Out;
	}

	TArray<FString> Filtered = ParamNames;
	if (!Q.IsEmpty()) { Filtered.Empty(); for (const FString& N : ParamNames) if (N.ToLower().Contains(Q)) Filtered.Add(N); }
	const int32 Total = Filtered.Num();
	TArray<TSharedPtr<FJsonValue>> Params;
	for (int32 i = Offset; i < Total && Params.Num() < Limit; ++i)
	{
		const FString& N = Filtered[i];
		const TSharedPtr<FJsonObject>* Props = nullptr;
		TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
		if (TSharedPtr<FJsonObject> InputSchema = Tool->BuildInputSchema())
		{
			if (InputSchema->TryGetObjectField(TEXT("properties"), Props) && Props)
			{
				const TSharedPtr<FJsonObject>* P = nullptr;
				if ((*Props)->TryGetObjectField(N, P) && P) Schema = *P;
			}
		}
		auto Sum = MakeShared<FJsonObject>();
		Sum->SetStringField(TEXT("name"), N);
		FString Type; if (Schema->TryGetStringField(TEXT("type"), Type)) Sum->SetStringField(TEXT("type"), Type); else Sum->SetStringField(TEXT("type"), TEXT("unknown"));
		FString Desc; if (Schema->TryGetStringField(TEXT("description"), Desc)) Sum->SetStringField(TEXT("description"), Desc);
		Params.Add(MakeShared<FJsonValueObject>(Sum));
	}
	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("tool"), ToolName);
	Out->SetStringField(TEXT("action"), Action);
	Out->SetStringField(TEXT("category"), Tool->GetCategory());
	Out->SetStringField(TEXT("description"), Tool->GetDescription());
	Out->SetBoolField(TEXT("enabled"), ToolManager.IsToolEnabled(ToolName));
	Out->SetArrayField(TEXT("actions"), GatewayStringArray(TArray<FString>{ Action }));
	Out->SetArrayField(TEXT("parameters"), Params);
	Out->SetNumberField(TEXT("parameterCount"), Total);
	Out->SetNumberField(TEXT("parameterOffset"), Offset);
	Out->SetNumberField(TEXT("parameterLimit"), Limit);
	Out->SetBoolField(TEXT("parameterHasMore"), Offset + Params.Num() < Total);
	Out->SetStringField(TEXT("scope"), TEXT("union"));
	Out->SetBoolField(TEXT("perActionSchemas"), false);
	Out->SetObjectField(TEXT("drillDown"), GatewayBuildNextCall(TEXT("describe"), ToolName, Action, Filtered.Num() > 0 ? Filtered[0] : (ParamNames.Num() > 0 ? ParamNames[0] : FString())));
	Out->SetStringField(TEXT("message"),
		TEXT("parameterNames is the union catalog for this parent tool (per-action schemas do not exist). Drill into a single param (drillDown) for its full schema."));
	return Out;
}
