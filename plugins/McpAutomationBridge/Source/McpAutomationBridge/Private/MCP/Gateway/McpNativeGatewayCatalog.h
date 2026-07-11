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

/** Exact action enum for a canonical tool, read from its input schema 'action' field. */
TArray<FString> GatewayGetActionValues(FMcpToolDefinition* Tool);

/** Declared parameter field names (schema properties minus action/subAction/params). */
TArray<FString> GatewayGetParameterNames(FMcpToolDefinition* Tool);

/** Bounded, compact search over the canonical tool catalog. */
TSharedPtr<FJsonObject> SearchGatewayCatalog(
	const FString& Query, int32 Limit, int32 Offset,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager);

/** Exact contract for one canonical tool (actions, parameter names, schema, per-action note). */
TSharedPtr<FJsonObject> DescribeGatewayCapability(
	const FString& ToolName, const FString& Action,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager);
