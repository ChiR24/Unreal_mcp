// McpNativeGatewayCatalog.cpp — search & describe operations for the unreal gateway

#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayManifest.h"
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
	// Prefer the neutral generated manifest (single source of truth with the TS gateway).
	if (TSharedPtr<FJsonObject> ManifestResult = GatewaySearchFromManifest(Query, Limit, Offset, ToolManager))
	{
		return ManifestResult;
	}

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

int32 GatewayLevenshtein(const FString& A, const FString& B)
{
	const int32 M = A.Len();
	const int32 N = B.Len();
	if (M == 0) return N;
	if (N == 0) return M;
	TArray<int32> Prev;
	Prev.SetNumUninitialized(N + 1);
	for (int32 j = 0; j <= N; ++j) Prev[j] = j;
	TArray<int32> Curr;
	Curr.SetNumUninitialized(N + 1);
	for (int32 i = 1; i <= M; ++i)
	{
		Curr[0] = i;
		for (int32 j = 1; j <= N; ++j)
		{
			const int32 Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
			Curr[j] = FMath::Min3(Prev[j] + 1, Curr[j - 1] + 1, Prev[j - 1] + Cost);
		}
		Swap(Prev, Curr);
	}
	return Prev[N];
}

TArray<FString> GatewayClosestMatches(const FString& Target, const TArray<FString>& Candidates, int32 Limit)
{
	if (Limit <= 0) return {};
	const FString T = Target.TrimStartAndEnd().ToLower();
	if (T.IsEmpty()) return Candidates;
	struct FScored { FString Name; int32 Score; };
	TArray<FScored> Scored;
	Scored.Reserve(Candidates.Num());
	for (const FString& C : Candidates)
	{
		const FString Lower = C.ToLower();
		int32 Score = GatewayLevenshtein(Lower, T);
		if (Lower.Contains(T) || T.Contains(Lower)) Score -= 4;
		Scored.Add({ C, Score });
	}
	Scored.Sort([](const FScored& L, const FScored& R) { return L.Score < R.Score; });
	TArray<FString> Out;
	for (int32 i = 0; i < Scored.Num() && i < Limit; ++i) Out.Add(Scored[i].Name);
	return Out;
}

TSharedPtr<FJsonObject> GatewayBuildNextCall(const FString& Operation, const FString& Tool, const FString& Action, const FString& Param)
{
	auto Next = MakeShared<FJsonObject>();
	Next->SetStringField(TEXT("operation"), Operation);
	if (!Tool.IsEmpty()) Next->SetStringField(TEXT("tool"), Tool);
	if (!Action.IsEmpty()) Next->SetStringField(TEXT("action"), Action);
	if (!Param.IsEmpty()) Next->SetStringField(TEXT("param"), Param);
	return Next;
}
