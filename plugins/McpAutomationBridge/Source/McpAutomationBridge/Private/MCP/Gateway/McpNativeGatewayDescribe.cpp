// McpNativeGatewayDescribe.cpp — capability describe for the unreal gateway.
//
// Three levels, mirroring the TypeScript discovery reference exactly:
//   1. describe { tool }                -> parent summary + bounded action list
//   2. describe { tool, action }        -> that capability's EXACT contract
//   3. describe { tool, action, param } -> exactly one parameter's schema
//
// Level 2 returns the action-specific schema, not a tool union, so no response
// dumps unrelated parameters. Sourced only from the generated capability store:
// when it is unavailable the caller receives a typed startup error rather than
// substituted metadata.

#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

namespace
{
TSharedPtr<FJsonObject> SchemaProperties(const TSharedPtr<FJsonObject>& Schema)
{
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Properties) && Properties)
	{
		return *Properties;
	}
	return MakeShared<FJsonObject>();
}

bool IsRequired(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
{
	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("required"), Required) || !Required) return false;
	for (const TSharedPtr<FJsonValue>& Entry : *Required)
	{
		FString Value;
		if (Entry.IsValid() && Entry->TryGetString(Value) && Value.Equals(Name, ESearchCase::CaseSensitive)) return true;
	}
	return false;
}

TArray<FString> ParameterNames(const TSharedPtr<FJsonObject>& Schema)
{
	TArray<FString> Names;
	for (const auto& Pair : SchemaProperties(Schema)->Values)
	{
		// Both key types dereference to a null-terminated TCHAR buffer, so a view
		// compares without allocating on every supported engine version.
		const FStringView KeyView(*Pair.Key);
		if (KeyView.Equals(TEXT("action"), ESearchCase::CaseSensitive)) continue;
		if (KeyView.Equals(TEXT("subAction"), ESearchCase::CaseSensitive)) continue;
		Names.Add(FString(*Pair.Key));
	}
	Names.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
	return Names;
}

TSharedPtr<FJsonObject> ParameterSchema(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
{
	const TSharedPtr<FJsonObject>* Property = nullptr;
	if (SchemaProperties(Schema)->TryGetObjectField(Name, Property) && Property) return *Property;
	return MakeShared<FJsonObject>();
}

TSharedPtr<FJsonObject> ParameterView(const TSharedPtr<FJsonObject>& Schema, const FString& Name)
{
	const TSharedPtr<FJsonObject> Body = ParameterSchema(Schema, Name);
	auto View = MakeShared<FJsonObject>();
	View->SetStringField(TEXT("name"), Name);
	View->SetBoolField(TEXT("required"), IsRequired(Schema, Name));
	FString Type;
	View->SetStringField(TEXT("type"), Body->TryGetStringField(TEXT("type"), Type) ? Type : TEXT("unknown"));
	FString Description;
	if (Body->TryGetStringField(TEXT("description"), Description))
	{
		View->SetStringField(TEXT("description"), Description);
	}
	const TArray<TSharedPtr<FJsonValue>>* EnumValues = nullptr;
	if (Body->TryGetArrayField(TEXT("enum"), EnumValues) && EnumValues)
	{
		View->SetArrayField(TEXT("enum"), *EnumValues);
	}
	return View;
}

TArray<FString> DistinctSortedOf(const TArray<const FMcpCapabilityRecord*>& Records,
	TFunctionRef<const FString&(const FMcpCapabilityRecord&)> Project)
{
	TArray<FString> Values;
	for (const FMcpCapabilityRecord* Record : Records)
	{
		const FString& Value = Project(*Record);
		const bool bSeen = Values.ContainsByPredicate(
			[&Value](const FString& Existing) { return Existing.Equals(Value, ESearchCase::CaseSensitive); });
		if (!bSeen) Values.Add(Value);
	}
	Values.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
	return Values;
}

void SetObjectOrEmpty(const TSharedPtr<FJsonObject>& Out, const TCHAR* Field, const TSharedPtr<FJsonObject>& Value)
{
	Out->SetObjectField(Field, Value.IsValid() ? Value : MakeShared<FJsonObject>());
}

TSharedPtr<FJsonObject> ToolSummary(
	const FString& Tool, const TArray<const FMcpCapabilityRecord*>& Siblings,
	const TArray<FString>& Actions, const FString& Query, int32 Limit, int32 Offset,
	const FString& Revision)
{
	TArray<FString> Filtered;
	if (Query.IsEmpty())
	{
		Filtered = Actions;
	}
	else
	{
		for (const FString& Action : Actions)
		{
			if (Action.ToLower().Contains(Query, ESearchCase::CaseSensitive)) Filtered.Add(Action);
		}
	}
	const int32 Total = Filtered.Num();
	TArray<FString> Paged;
	for (int32 Index = Offset; Index < Total && Paged.Num() < Limit; ++Index) Paged.Add(Filtered[Index]);

	auto Out = MakeShared<FJsonObject>();
	Out->SetNumberField(TEXT("actionCount"), Total);
	Out->SetBoolField(TEXT("actionHasMore"), Offset + Paged.Num() < Total);
	Out->SetNumberField(TEXT("actionLimit"), Limit);
	Out->SetNumberField(TEXT("actionOffset"), Offset);
	Out->SetArrayField(TEXT("actions"), GatewayStringArray(Paged));
	Out->SetNumberField(TEXT("capabilityCount"), Siblings.Num());
	Out->SetStringField(TEXT("catalogRevision"), Revision);
	Out->SetArrayField(TEXT("domains"), GatewayStringArray(
		DistinctSortedOf(Siblings, [](const FMcpCapabilityRecord& R) -> const FString& { return R.Domain; })));
	const FString First = Paged.Num() > 0 ? Paged[0] : (Actions.Num() > 0 ? Actions[0] : FString());
	Out->SetObjectField(TEXT("drillDown"), GatewayBuildNextCall(TEXT("describe"), Tool, First, FString()));
	Out->SetArrayField(TEXT("families"), GatewayStringArray(
		DistinctSortedOf(Siblings, [](const FMcpCapabilityRecord& R) -> const FString& { return R.Family; })));
	Out->SetStringField(TEXT("message"),
		TEXT("Tool summary. Drill into an action to receive that capability's exact contract."));
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	Out->SetStringField(TEXT("scope"), TEXT("tool"));
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("tool"), Tool);
	return Out;
}

// The exact `consent` sibling execute demands for this capability, so a grant is
// discoverable BEFORE the first refusal — the gateway tool description points
// clients at `describe.consentGrant`. Absent when policy.consent is "none".
// Must stay identical to the TypeScript capabilityConsentGrant: the two
// surfaces answer the same discovery question and a client may follow either.
TSharedPtr<FJsonObject> ConsentGrant(const FMcpCapabilityRecord& Record)
{
	FString Consent;
	if (!Record.Policy.IsValid()) return nullptr;
	if (!Record.Policy->TryGetStringField(TEXT("consent"), Consent)) return nullptr;
	if (Consent.Equals(TEXT("none"), ESearchCase::IgnoreCase)) return nullptr;
	const bool bElevated = Consent.Equals(TEXT("elevated"), ESearchCase::IgnoreCase);
	auto Grant = MakeShared<FJsonObject>();
	Grant->SetStringField(TEXT("capability"), Record.Id);
	Grant->SetStringField(TEXT("acknowledge"), bElevated ? TEXT("elevated") : TEXT("explicit"));
	return Grant;
}

TSharedPtr<FJsonObject> CapabilityContract(
	const FMcpCapabilityRecord& Record, const FString& Tool, const FString& Revision, bool bAvailable)
{
	auto Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("action"), Record.DispatchAction);
	SetObjectOrEmpty(Out, TEXT("availability"), Record.Availability);
	Out->SetBoolField(TEXT("available"), bAvailable);
	SetObjectOrEmpty(Out, TEXT("behavior"), Record.Behavior);
	Out->SetStringField(TEXT("capability"), Record.Id);
	Out->SetStringField(TEXT("catalogRevision"), Revision);
	if (const TSharedPtr<FJsonObject> Grant = ConsentGrant(Record)) Out->SetObjectField(TEXT("consentGrant"), Grant);
	SetObjectOrEmpty(Out, TEXT("cost"), Record.Cost);
	SetObjectOrEmpty(Out, TEXT("deprecation"), Record.Deprecation);
	Out->SetStringField(TEXT("domain"), Record.Domain);
	Out->SetStringField(TEXT("effect"), Record.Effect);
	Out->SetNumberField(TEXT("exampleCount"), Record.ExampleCount);
	Out->SetStringField(TEXT("family"), Record.Family);
	SetObjectOrEmpty(Out, TEXT("hashes"), Record.Hashes);
	SetObjectOrEmpty(Out, TEXT("inputSchema"), Record.InputSchema);
	Out->SetStringField(TEXT("message"),
		TEXT("Exact capability contract. Every parameter below is action-specific, not a tool union."));
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	SetObjectOrEmpty(Out, TEXT("outputSchema"), Record.OutputSchema);
	TArray<TSharedPtr<FJsonValue>> Parameters;
	for (const FString& Name : ParameterNames(Record.InputSchema))
	{
		Parameters.Add(MakeShared<FJsonValueObject>(ParameterView(Record.InputSchema, Name)));
	}
	Out->SetArrayField(TEXT("parameters"), Parameters);
	Out->SetStringField(TEXT("parent"), Record.Parent);
	SetObjectOrEmpty(Out, TEXT("policy"), Record.Policy);
	Out->SetStringField(TEXT("scope"), TEXT("capability"));
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("summary"), Record.Summary);
	Out->SetStringField(TEXT("tool"), Tool);
	Out->SetArrayField(TEXT("whenNotToUse"), GatewayStringArray(Record.WhenNotToUse));
	Out->SetArrayField(TEXT("whenToUse"), GatewayStringArray(Record.WhenToUse));
	return Out;
}
}

TSharedPtr<FJsonObject> McpGatewayDescribeCapability(
	const FMcpDiscoveryQuery& Input, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled)
{
	if (!Store.IsReady()) return McpGatewayCatalogUnavailable(TEXT("describe"), Store);

	const FString& Revision = Store.GetCatalogRevision();
	const int32 Limit = FMath::Clamp(Input.Limit, 1, McpDescribeMaxLimit);
	const int32 Offset = FMath::Max(0, Input.Offset);
	const FString Query = Input.Query.TrimStartAndEnd().ToLower();

	const TArray<FString> Parents = Store.GetParents();
	if (!Parents.ContainsByPredicate([&](const FString& P) { return P.Equals(Input.Tool, ESearchCase::CaseSensitive); }))
	{
		TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_TOOL"),
			TEXT("Unknown tool. Call search to retrieve canonical capability names."));
		Error->SetStringField(TEXT("catalogRevision"), Revision);
		Error->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
		Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Input.Tool, Parents, 3)));
		return Error;
	}

	const TArray<const FMcpCapabilityRecord*> Siblings = Store.GetRecordsForParent(Input.Tool);
	// PUBLIC action names, not internal DispatchAction: all 50 manage_audio
	// capabilities share one DispatchAction, so this listed 1 action for 50.
	const TArray<FString> Actions = McpDistinctSortedComputed(
		Siblings, [](const FMcpCapabilityRecord& R) -> FString { return McpCapabilityPublicAction(R); });

	if (!Input.bHasAction)
	{
		return ToolSummary(Input.Tool, Siblings, Actions, Query, Limit, Offset, Revision);
	}

	const FMcpCapabilityRecord* Record = Store.FindByParentAction(Input.Tool, Input.Action);
	if (!Record)
	{
		TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action '%s' for %s."), *Input.Action, *Input.Tool));
		Error->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
		Error->SetStringField(TEXT("catalogRevision"), Revision);
		Error->SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), Input.Tool, FString(), FString()));
		Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Input.Action, Actions, 3)));
		Error->SetStringField(TEXT("tool"), Input.Tool);
		return Error;
	}

	const TArray<FString> Names = ParameterNames(Record->InputSchema);
	if (Input.bHasParam)
	{
		if (!Names.ContainsByPredicate([&](const FString& N) { return N.Equals(Input.Param, ESearchCase::CaseSensitive); }))
		{
			TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_PARAM"),
				FString::Printf(TEXT("Unknown parameter '%s' for %s."), *Input.Param, *Record->Id));
			Error->SetArrayField(TEXT("availableParameters"), GatewayStringArray(Names));
			Error->SetStringField(TEXT("capability"), Record->Id);
			Error->SetStringField(TEXT("catalogRevision"), Revision);
			Error->SetObjectField(TEXT("nextCall"),
				GatewayBuildNextCall(TEXT("describe"), Input.Tool, Input.Action, FString()));
			Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(GatewayClosestMatches(Input.Param, Names, 3)));
			return Error;
		}
		auto Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("capability"), Record->Id);
		Out->SetStringField(TEXT("catalogRevision"), Revision);
		Out->SetStringField(TEXT("message"), TEXT("Exact per-action parameter schema. Pass it under params on execute."));
		Out->SetStringField(TEXT("operation"), TEXT("describe"));
		Out->SetStringField(TEXT("param"), Input.Param);
		Out->SetBoolField(TEXT("required"), IsRequired(Record->InputSchema, Input.Param));
		Out->SetObjectField(TEXT("schema"), ParameterSchema(Record->InputSchema, Input.Param));
		Out->SetStringField(TEXT("scope"), TEXT("capability"));
		Out->SetBoolField(TEXT("success"), true);
		return Out;
	}

	const bool bAvailable =
		!Record->DeprecationStatus.Equals(TEXT("removed"), ESearchCase::CaseSensitive) &&
		IsToolEnabled(Record->Parent);
	return CapabilityContract(*Record, Input.Tool, Revision, bAvailable);
}
