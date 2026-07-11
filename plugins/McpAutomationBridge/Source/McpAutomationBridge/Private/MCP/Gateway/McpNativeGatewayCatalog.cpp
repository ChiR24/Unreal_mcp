// McpNativeGatewayCatalog.cpp — search & describe operations for the unreal gateway

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Protocol/McpJsonRpc.h"

namespace
{
TSharedPtr<FJsonObject> GetSchemaProperties(FMcpToolDefinition* Tool)
{
	if (!Tool) return nullptr;
	TSharedPtr<FJsonObject> Schema = Tool->BuildInputSchema();
	const TSharedPtr<FJsonObject>* Props = nullptr;
	if (Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Props) && Props)
	{
		return *Props;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> BuildDescriptor(FMcpToolDefinition* Tool, const FMcpDynamicToolManager& ToolManager)
{
	auto Desc = MakeShared<FJsonObject>();
	Desc->SetStringField(TEXT("name"), Tool->GetName());
	Desc->SetStringField(TEXT("category"), Tool->GetCategory());
	Desc->SetStringField(TEXT("description"), Tool->GetDescription());
	Desc->SetArrayField(TEXT("actions"), GatewayStringArray(GatewayGetActionValues(Tool)));
	Desc->SetArrayField(TEXT("parameterNames"), GatewayStringArray(GatewayGetParameterNames(Tool)));
	Desc->SetBoolField(TEXT("enabled"), ToolManager.IsToolEnabled(Tool->GetName()));
	return Desc;
}
}

TArray<FString> GatewayGetActionValues(FMcpToolDefinition* Tool)
{
	TArray<FString> Actions;
	const TSharedPtr<FJsonObject> Props = GetSchemaProperties(Tool);
	const TSharedPtr<FJsonObject>* ActionObj = nullptr;
	if (Props.IsValid() && Props->TryGetObjectField(TEXT("action"), ActionObj) && ActionObj)
	{
		const TArray<TSharedPtr<FJsonValue>>* EnumArr = nullptr;
		if ((*ActionObj)->TryGetArrayField(TEXT("enum"), EnumArr) && EnumArr)
		{
			for (const TSharedPtr<FJsonValue>& V : *EnumArr)
			{
				FString S;
				if (V->TryGetString(S))
				{
					Actions.AddUnique(S);
				}
			}
		}
	}
	Actions.Sort();
	return Actions;
}

TArray<FString> GatewayGetParameterNames(FMcpToolDefinition* Tool)
{
	TArray<FString> Names;
	const TSharedPtr<FJsonObject> Props = GetSchemaProperties(Tool);
	if (Props.IsValid())
	{
		for (const auto& Pair : Props->Values)
		{
			const FString Name(Pair.Key);
			if (Name != TEXT("action") && Name != TEXT("subAction") && Name != TEXT("params"))
			{
				Names.Add(Name);
			}
		}
	}
	Names.Sort();
	return Names;
}

TSharedPtr<FJsonObject> SearchGatewayCatalog(
	const FString& Query, int32 Limit, int32 Offset,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager)
{
	const FString Q = Query.ToLower();

	TArray<TSharedPtr<FJsonObject>> Matches;
	for (const FString& Name : Registry.GetToolNames())
	{
		FMcpToolDefinition* Tool = Registry.FindTool(Name);
		if (!Tool) continue;

		if (!Q.IsEmpty())
		{
			FString Searchable = Tool->GetName() + TEXT(" ") + Tool->GetCategory() + TEXT(" ")
				+ Tool->GetDescription();
			for (const FString& A : GatewayGetActionValues(Tool))
			{
				Searchable += TEXT(" ") + A;
			}
			if (!Searchable.ToLower().Contains(Q)) continue;
		}
		Matches.Add(BuildDescriptor(Tool, ToolManager));
	}

	const int32 Total = Matches.Num();
	TArray<TSharedPtr<FJsonObject>> Results;
	for (int32 i = Offset; i < Total && Results.Num() < Limit; ++i)
	{
		Results.Add(Matches[i]);
	}

	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("search"));
	Out->SetStringField(TEXT("query"), Query);
	TArray<TSharedPtr<FJsonValue>> ResultValues;
	ResultValues.Reserve(Results.Num());
	for (const TSharedPtr<FJsonObject>& R : Results)
	{
		ResultValues.Add(MakeShared<FJsonValueObject>(R));
	}
	Out->SetArrayField(TEXT("results"), ResultValues);
	Out->SetNumberField(TEXT("total"), Total);
	Out->SetNumberField(TEXT("offset"), Offset);
	Out->SetNumberField(TEXT("limit"), Limit);
	Out->SetBoolField(TEXT("hasMore"), Offset + Results.Num() < Total);
	Out->SetStringField(TEXT("message"),
		TEXT("Search results are compact. Call describe with an exact tool and action before execute."));
	return Out;
}

TSharedPtr<FJsonObject> DescribeGatewayCapability(
	const FString& ToolName, const FString& Action,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager)
{
	FMcpToolDefinition* Tool = Registry.FindTool(ToolName);
	if (!Tool)
	{
		return GatewayError(TEXT("describe"), TEXT("UNKNOWN_TOOL"),
			TEXT("Unknown tool. Call search to retrieve canonical tool names."));
	}

	const TArray<FString> Actions = GatewayGetActionValues(Tool);
	if (!Action.IsEmpty() && !Actions.Contains(Action))
	{
		TSharedPtr<FJsonObject> Err = GatewayError(TEXT("describe"), TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action '%s' for %s."), *Action, *ToolName));
		Err->SetStringField(TEXT("tool"), ToolName);
		Err->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
		return Err;
	}

	TArray<FString> OutActions = Action.IsEmpty() ? Actions : TArray<FString>{ Action };

	auto Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("tool"), ToolName);
	Out->SetStringField(TEXT("action"), Action);
	Out->SetStringField(TEXT("category"), Tool->GetCategory());
	Out->SetStringField(TEXT("description"), Tool->GetDescription());
	Out->SetBoolField(TEXT("enabled"), ToolManager.IsToolEnabled(ToolName));
	Out->SetArrayField(TEXT("actions"), GatewayStringArray(OutActions));
	Out->SetArrayField(TEXT("parameterNames"), GatewayStringArray(GatewayGetParameterNames(Tool)));
	Out->SetObjectField(TEXT("inputSchema"), Tool->BuildInputSchema());
	Out->SetBoolField(TEXT("perActionSchemas"), false);
	Out->SetStringField(TEXT("message"),
		TEXT("The canonical catalog does not register per-action parameter schemas. "
			"parameterNames is the union across all actions — pass only parameters "
			"relevant to the selected action. Use the exact casing shown in inputSchema."));
	return Out;
}
