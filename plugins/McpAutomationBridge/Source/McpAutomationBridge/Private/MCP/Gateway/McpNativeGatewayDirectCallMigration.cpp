#include "MCP/Gateway/McpNativeGatewayDirectCallMigration.h"

#include "Dom/JsonValue.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

namespace
{
constexpr int32 McpMigrationMaxSuggestions = 3;  // mirrors TS MAX_SUGGESTIONS

// Merge a legacy call's nested `params` into the action parameters (top-level
// keys win, exactly as legacy dispatch did), then drop the routing/control
// fields on a fresh object so the caller's Arguments is never mutated.
TSharedPtr<FJsonObject> MigratedParams(const TSharedPtr<FJsonObject>& Arguments)
{
	TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
	if (Arguments.IsValid())
	{
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Arguments->TryGetObjectField(TEXT("params"), Nested) && Nested && Nested->IsValid())
		{
			Merged->Values = (*Nested)->Values;
		}
		for (const auto& Field : Arguments->Values)
		{
			Merged->Values.Add(Field.Key, Field.Value);
		}
	}
	Merged->RemoveField(TEXT("action"));
	Merged->RemoveField(TEXT("subAction"));
	Merged->RemoveField(TEXT("params"));
	Merged->RemoveField(TEXT("operation"));
	return Merged;
}

// Flat migration receipt mirroring the TS DirectCallMigrationResult:
// success/operation/errorCode/tool/message/nextCall as top-level keys, never a
// wrapper (operation is copied from nextCall below).
TSharedPtr<FJsonObject> MigrationReceipt(
	const FString& ToolName, const FString& Message, const TSharedPtr<FJsonObject>& NextCall)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("errorCode"), TEXT("DIRECT_TOOL_CALL_REMOVED"));
	Result->SetStringField(TEXT("tool"), ToolName);
	Result->SetStringField(TEXT("message"), Message);
	// The `unreal` output schema (and the TS DirectCallMigrationResult) require a
	// top-level `operation` beside success:false; copy it from the branch's
	// nextCall so the flat receipt and its executable next step never disagree.
	FString Operation;
	if (NextCall.IsValid() && NextCall->TryGetStringField(TEXT("operation"), Operation))
	{
		Result->SetStringField(TEXT("operation"), Operation);
	}
	Result->SetObjectField(TEXT("nextCall"), NextCall);
	return Result;
}

// Mirror of TS getString(args, key): accept only a JSON string, TrimStartAndEnd,
// and reject an empty-after-trim value so it can fall through. Sets OutValue to
// the trimmed value on a hit and leaves it untouched on a miss.
bool GetTrimmedStringArg(
	const TSharedPtr<FJsonObject>& Arguments, const TCHAR* Key, FString& OutValue)
{
	if (!Arguments.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonValue> Value = Arguments->TryGetField(Key);
	if (!Value.IsValid() || Value->Type != EJson::String)
	{
		return false;
	}
	const FString Trimmed = Value->AsString().TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}
	OutValue = Trimmed;
	return true;
}
}

TSharedPtr<FJsonObject> McpBuildDirectCallMigration(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TArray<FString>& ParentNames)
{
	// Unknown parent: steer to search and offer the closest real tool names.
	if (!ParentNames.Contains(ToolName))
	{
		TSharedPtr<FJsonObject> NextCall = MakeShared<FJsonObject>();
		NextCall->SetStringField(TEXT("operation"), TEXT("search"));
		const FString Message = FString::Printf(
			TEXT("Direct tool calls are removed. '%s' is not a known tool; call the 'unreal' gateway with operation 'search' to discover the right capability."),
			*ToolName);
		TSharedPtr<FJsonObject> Result = MigrationReceipt(ToolName, Message, NextCall);
		TArray<TSharedPtr<FJsonValue>> Suggestions;
		for (const FString& Name : GatewayClosestMatches(ToolName, ParentNames, McpMigrationMaxSuggestions))
		{
			Suggestions.Add(MakeShared<FJsonValueString>(Name));
		}
		Result->SetArrayField(TEXT("suggestions"), Suggestions);
		return Result;
	}

	// Action selection mirrors TS getString(args,'action') ?? getString(args,'subAction'):
	// a string-typed, trimmed, non-empty action wins; an empty/whitespace/non-string
	// action falls through to subAction; otherwise there is no action.
	FString Action;
	const bool bHasAction =
		GetTrimmedStringArg(Arguments, TEXT("action"), Action) ||
		GetTrimmedStringArg(Arguments, TEXT("subAction"), Action);

	// Known parent, no action: steer to describe so the caller picks an action.
	if (!bHasAction)
	{
		TSharedPtr<FJsonObject> NextCall = MakeShared<FJsonObject>();
		NextCall->SetStringField(TEXT("operation"), TEXT("describe"));
		NextCall->SetStringField(TEXT("tool"), ToolName);
		const FString Message = FString::Printf(
			TEXT("Direct tool calls are removed. Call the 'unreal' gateway: describe '%s' to choose an action, then execute it."),
			*ToolName);
		return MigrationReceipt(ToolName, Message, NextCall);
	}

	// Known parent with action/subAction: hand back a runnable execute call.
	TSharedPtr<FJsonObject> NextCall = MakeShared<FJsonObject>();
	NextCall->SetStringField(TEXT("operation"), TEXT("execute"));
	NextCall->SetStringField(TEXT("tool"), ToolName);
	NextCall->SetStringField(TEXT("action"), Action);
	NextCall->SetObjectField(TEXT("params"), MigratedParams(Arguments));
	const FString Message = FString::Printf(
		TEXT("Direct tool calls are removed. Call the 'unreal' gateway with operation 'execute' to run '%s.%s'."),
		*ToolName, *Action);
	return MigrationReceipt(ToolName, Message, NextCall);
}
