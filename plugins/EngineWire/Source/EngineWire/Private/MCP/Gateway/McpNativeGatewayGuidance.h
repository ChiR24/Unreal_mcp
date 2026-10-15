// McpNativeGatewayGuidance.h — gateway error envelopes and closest-match guidance
//
// Depends on nothing but CoreMinimal + JSON, so guided self-correction is shared
// by discovery and execute without dragging in the tool registry.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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

/** Ranked closest candidates to Target by substring boost, shared prefix, then name. */
TArray<FString> GatewayClosestMatches(const FString& Target, const TArray<FString>& Candidates, int32 Limit = 3);

/** Build a directly-invokable gateway request payload (omitted parts stay absent). */
TSharedPtr<FJsonObject> GatewayBuildNextCall(const FString& Operation, const FString& Tool, const FString& Action, const FString& Param);

/** What an UNKNOWN_ACTION hands back: suggestions, the call to make next, and a message hint. */
struct FMcpUnknownActionGuide
{
	TArray<FString> Suggestions;
	TSharedPtr<FJsonObject> NextCall;
	/** " 'x' is a <tool> action." when another tool owns the name, else empty. */
	FString Hint;
};

/**
 * Guidance for an action a tool does not have. The one OTHER tool that owns the
 * exact name wins (manage_blueprint.set_blueprint_variables -> control_actor).
 * When no suggestion shares the action's verb, even with a one-letter typo,
 * the name is searched instead: save_asset used to be sent to move_asset,
 * while saving is control_editor.save_all. Otherwise the closest action of this
 * tool. Mirrors guideUnknownAction in gateway-guidance.ts.
 */
FMcpUnknownActionGuide GatewayGuideUnknownAction(const FString& Tool, const FString& Action,
	const TArray<FString>& Actions, const TArray<FString>& OwningTools);

/** nextCall envelope pointing a disabled-capability caller at configure. */
TSharedPtr<FJsonObject> GatewayDisabledCapabilityGuidance(const FString& ParentTool);

/** nextCall envelope pointing a schema-refused caller at describe. */
TSharedPtr<FJsonObject> GatewaySchemaGuidance(
	const FString& ParentTool, const FString& Action, const FString& Pointer);
