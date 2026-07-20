// McpNativeGatewayCatalog.cpp — shared gateway envelope + guidance primitives.
//
// Discovery lives in McpNativeGatewaySearch/Describe and is sourced from the
// generated capability store; shared error/guidance primitives live in
// McpNativeGatewayGuidance. What remains here is the registry-schema reader pair
// the execute validation path still needs.

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
	Actions.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
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
	Names.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
	return Names;
}
