// McpNativeGatewayCatalog.h — search & describe operations for the unreal gateway

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FMcpToolRegistry;
class FMcpDynamicToolManager;
class FMcpToolDefinition;

/** Build a gateway error envelope (success=false) reused by search/describe/execute. */
inline TSharedPtr<FJsonObject> GatewayError(const FString& Operation, const FString& ErrorCode, const FString& Message)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("operation"), Operation);
	Obj->SetStringField(TEXT("errorCode"), ErrorCode);
	Obj->SetStringField(TEXT("error"), Message);
	Obj->SetStringField(TEXT("message"), Message);
	return Obj;
}

/** Wrap a string array as a JSON array value. */
inline TArray<TSharedPtr<FJsonValue>> GatewayStringArray(const TArray<FString>& Strings)
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	Arr.Reserve(Strings.Num());
	for (const FString& S : Strings)
	{
		Arr.Add(MakeShared<FJsonValueString>(S));
	}
	return Arr;
}

/** Edit-distance for closest-match suggestions. */
int32 GatewayLevenshtein(const FString& A, const FString& B);

/** Ranked closest candidates to Target by substring boost + edit distance. */
TArray<FString> GatewayClosestMatches(const FString& Target, const TArray<FString>& Candidates, int32 Limit = 3);

/** Build a directly-invokable gateway request payload (omitted parts stay absent). */
TSharedPtr<FJsonObject> GatewayBuildNextCall(const FString& Operation, const FString& Tool, const FString& Action, const FString& Param);

/** Exact action enum for a canonical tool, read from its input schema 'action' field. */
TArray<FString> GatewayGetActionValues(FMcpToolDefinition* Tool);

/** Declared parameter field names (schema properties minus action/subAction/params). */
TArray<FString> GatewayGetParameterNames(FMcpToolDefinition* Tool);

/** Bounded, compact search over the canonical tool catalog. */
TSharedPtr<FJsonObject> SearchGatewayCatalog(
	const FString& Query, int32 Limit, int32 Offset,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager);

/** Progressive contract for one canonical tool: tool summary, action parameter catalog, or a single parameter schema. */
TSharedPtr<FJsonObject> DescribeGatewayCapability(
	const FString& ToolName, const FString& Action,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	const FString& Param = FString(), const FString& Query = FString(),
	int32 Limit = 20, int32 Offset = 0);
