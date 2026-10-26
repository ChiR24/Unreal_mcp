#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Core/Security/McpConsentLedger.h"
// McpNativeGatewayDescribe.cpp — capability describe for the unreal gateway.
//
// The levels below the browse listings (McpNativeGatewayDescribeOverview.cpp), the same replies as the TypeScript
// discovery reference (gateway-describe.ts, gateway-describe-capability.ts):
//   1. describe { tool }                -> parent summary + bounded action list
//   2. describe { tool, action }        -> that capability's EXACT contract
//   3. describe { tool, action, param } -> exactly one parameter's schema
//
// Level 2 returns the action-specific schema, not a tool union, so no response
// dumps unrelated parameters. Sourced only from the generated capability store:
// when it is unavailable the caller receives a typed startup error rather than
// substituted metadata.

#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Execute/Request/McpFoldVariantRules.h"

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
		if (Entry.IsValid() && McpHandlerUtils::TryGetJsonValueString(Entry, Value) && Value.Equals(Name, ESearchCase::CaseSensitive)) return true;
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

// The variants of a folded family that read this parameter (none when all of them
// do): the union schema lists every variant's parameters, so a caller adding a
// slider could not tell minValue from isMarquee.
void SetVariants(const TSharedPtr<FJsonObject>& Out, const FMcpCapabilityRecord& Record, const FString& Name)
{
	if (const TArray<FString>* Variants = Record.DispatchByDeclaredBy.Find(Name))
	{
		Out->SetArrayField(TEXT("variants"), GatewayStringArray(*Variants));
	}
	McpSetVariantRequirementViews(Out, Record, Name);
}


void SetObjectOrEmpty(const TSharedPtr<FJsonObject>& Out, const TCHAR* Field, const TSharedPtr<FJsonObject>& Value)
{
	Out->SetObjectField(Field, Value.IsValid() ? Value : MakeShared<FJsonObject>());
}

// The name the caller described the capability by, when it is not the one it answers to (an old folded name).
void SetMigratedFrom(const TSharedPtr<FJsonObject>& Out, const FMcpDiscoveryQuery& Input, const FMcpCapabilityRecord& Record)
{
	if (Input.Action.Equals(McpCapabilityPublicAction(Record), ESearchCase::CaseSensitive)) return;
	auto From = MakeShared<FJsonObject>();
	From->SetStringField(TEXT("tool"), Input.Tool);
	From->SetStringField(TEXT("action"), Input.Action);
	Out->SetObjectField(TEXT("migratedFrom"), From);
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
	// Single-use binding: each describe mints a fresh nonce for this capability in
	// the consent ledger, and the prequeue gate burns it on first acceptance, so a
	// replayed grant is CONSENT_REUSED and one whose nonce this server did not mint
	// for the capability (made up, missing, too old) is CONSENT_NOT_ISSUED.
	Grant->SetStringField(TEXT("nonce"), McpCapabilityAuthorization::FMcpConsentLedger::Get().Mint(Record.Id));
	return Grant;
}

// capabilityContract in gateway-capability-view.ts, field for field.
TSharedPtr<FJsonObject> CapabilityContract(
	const FMcpCapabilityRecord& Record, const FString& Tool, const FString& Revision, bool bAvailable)
{
	auto Out = MakeShared<FJsonObject>();
	Out->SetStringField(TEXT("action"), McpCapabilityPublicAction(Record));
	Out->SetArrayField(TEXT("aliases"), GatewayStringArray(Record.Aliases));
	Out->SetObjectField(TEXT("availability"), McpCapabilityAvailabilityView(Record, bAvailable));
	SetObjectOrEmpty(Out, TEXT("behavior"), Record.Behavior);
	Out->SetStringField(TEXT("capability"), Record.Id);
	Out->SetStringField(TEXT("catalogRevision"), Revision);
	if (const TSharedPtr<FJsonObject> Grant = ConsentGrant(Record)) Out->SetObjectField(TEXT("consentGrant"), Grant);
	SetObjectOrEmpty(Out, TEXT("cost"), Record.Cost);
	Out->SetStringField(TEXT("domain"), Record.Domain);
	Out->SetNumberField(TEXT("exampleCount"), Record.Examples.Num());
	if (Record.Examples.Num() > 0)
	{
		Out->SetArrayField(TEXT("examples"), Record.Examples);
	}
	Out->SetStringField(TEXT("family"), Record.Family);
	SetObjectOrEmpty(Out, TEXT("hashes"), Record.Hashes);
	SetObjectOrEmpty(Out, TEXT("inputSchema"), McpStripActionFromInputSchema(Record.InputSchema));
	TArray<TSharedPtr<FJsonValue>> LegacyIds;
	for (const FMcpLegacyPair& Pair : Record.LegacyPairs)
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("tool"), Pair.Tool);
		Entry->SetStringField(TEXT("action"), Pair.Action);
		if (Pair.IsFolded()) Entry->SetObjectField(TEXT("folded"), Pair.FoldedPins);
		LegacyIds.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Out->SetArrayField(TEXT("legacyIds"), LegacyIds);
	Out->SetStringField(TEXT("message"),
		TEXT("Exact capability contract. inputSchema declares every parameter this action accepts."));
	// The execute call to send with params filled in, as the tool description promises (capabilityNextCall in TS);
	// a disabled tool is turned back on first.
	TSharedPtr<FJsonObject> Next;
	if (bAvailable)
	{
		Next = GatewayBuildNextCall(TEXT("execute"), Tool, McpCapabilityPublicAction(Record), FString());
		Next->SetStringField(TEXT("capability"), Record.Id);
		Next->SetObjectField(TEXT("params"), MakeShared<FJsonObject>());
	}
	else
	{
		Next = GatewayEnableToolNextCall(Record.Parent);
	}
	Out->SetObjectField(TEXT("nextCall"), Next);
	Out->SetStringField(TEXT("operation"), TEXT("describe"));
	SetObjectOrEmpty(Out, TEXT("outputSchema"), Record.OutputSchema);
	TArray<TSharedPtr<FJsonValue>> Parameters;
	for (const FString& Name : ParameterNames(Record.InputSchema))
	{
		const TSharedPtr<FJsonObject> View = ParameterView(Record.InputSchema, Name);
		SetVariants(View, Record, Name);
		Parameters.Add(MakeShared<FJsonValueObject>(View));
	}
	Out->SetArrayField(TEXT("parameters"), Parameters);
	Out->SetStringField(TEXT("parentTool"), Record.Parent);
	SetObjectOrEmpty(Out, TEXT("policy"), Record.Policy);
	Out->SetBoolField(TEXT("runnable"), bAvailable);
	Out->SetStringField(TEXT("scope"), TEXT("capability"));
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("summary"), Record.Summary);
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
	if (Input.Tool.IsEmpty()) return McpGatewayDescribeBrowse(Input, Store, IsToolEnabled);
	const TArray<FString> Parents = Store.GetParents();
	if (!Parents.ContainsByPredicate([&](const FString& P) { return P.Equals(Input.Tool, ESearchCase::CaseSensitive); }))
	{
		TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_TOOL"),
			TEXT("Unknown tool. Call search to retrieve canonical tool names."));
		const TArray<FString> Suggestions = GatewayClosestMatches(Input.Tool, Parents, 3);
		Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
		Error->SetObjectField(TEXT("nextCall"), Suggestions.Num() > 0
			? GatewayBuildNextCall(TEXT("describe"), Suggestions[0], FString(), FString())
			: GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
		return Error;
	}

	const TArray<const FMcpCapabilityRecord*> Siblings = Store.GetRecordsForParent(Input.Tool);
	// PUBLIC action names, not internal DispatchAction: all 50 manage_audio
	// capabilities share one DispatchAction, so this listed 1 action for 50.
	const TArray<FString> Actions = McpDistinctSorted(
		Siblings, [](const FMcpCapabilityRecord& R) -> FString { return McpCapabilityPublicAction(R); });

	if (!Input.bHasAction)
	{
		return McpGatewayToolSummary(Input.Tool, Actions, Input, IsToolEnabled(Input.Tool));
	}

	const FMcpCapabilityRecord* Record = Store.FindByParentAction(Input.Tool, Input.Action);
	if (!Record)
	{
		const FMcpUnknownActionGuide Guide = GatewayGuideUnknownAction(
			Input.Tool, Input.Action, Actions, Store.GetParentsWithAction(Input.Action));
		TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action '%s' for %s.%s"), *Input.Action, *Input.Tool, *Guide.Hint));
		Error->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
		Error->SetObjectField(TEXT("nextCall"), Guide.NextCall);
		Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(Guide.Suggestions));
		Error->SetStringField(TEXT("tool"), Input.Tool);
		return Error;
	}

	const TArray<FString> Names = ParameterNames(Record->InputSchema);
	if (Input.bHasParam)
	{
		if (!Names.ContainsByPredicate([&](const FString& N) { return N.Equals(Input.Param, ESearchCase::CaseSensitive); }))
		{
			TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_PARAM"),
				FString::Printf(TEXT("Unknown parameter '%s' for capability '%s'."), *Input.Param, *Record->Id));
			const TArray<FString> Suggestions = GatewayClosestMatches(Input.Param, Names, 3);
			Error->SetArrayField(TEXT("availableParameters"), GatewayStringArray(Names));
			Error->SetStringField(TEXT("capability"), Record->Id);
			const TSharedPtr<FJsonObject> Next = GatewayBuildNextCall(TEXT("describe"), FString(), FString(),
				Suggestions.Num() > 0 ? Suggestions[0] : FString());
			Next->SetStringField(TEXT("capability"), Record->Id);
			Error->SetObjectField(TEXT("nextCall"), Next);
			Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
			return Error;
		}
		auto Out = MakeShared<FJsonObject>();
		Out->SetStringField(TEXT("action"), McpCapabilityPublicAction(*Record));
		Out->SetStringField(TEXT("capability"), Record->Id);
		Out->SetStringField(TEXT("catalogRevision"), Revision);
		// Same capability, same consent requirement. Omitting the grant here made
		// the compact per-parameter describe useless for any consented write: the
		// caller still had to pay for a full-size describe just to mint a nonce.
		if (const TSharedPtr<FJsonObject> Grant = ConsentGrant(*Record)) Out->SetObjectField(TEXT("consentGrant"), Grant);
		Out->SetStringField(TEXT("message"), TEXT("Single parameter schema for this capability. Pass it with the exact casing shown."));
		Out->SetStringField(TEXT("operation"), TEXT("describe"));
		Out->SetStringField(TEXT("param"), Input.Param);
		Out->SetStringField(TEXT("parentTool"), Record->Parent);
		Out->SetBoolField(TEXT("required"), IsRequired(Record->InputSchema, Input.Param));
		Out->SetObjectField(TEXT("schema"), ParameterSchema(Record->InputSchema, Input.Param));
		Out->SetStringField(TEXT("scope"), TEXT("parameter"));
		Out->SetBoolField(TEXT("success"), true);
		SetVariants(Out, *Record, Input.Param);
		SetMigratedFrom(Out, Input, *Record);
		return Out;
	}

	TSharedPtr<FJsonObject> Contract = CapabilityContract(*Record, Input.Tool, Revision, IsToolEnabled(Record->Parent));
	SetMigratedFrom(Contract, Input, *Record);
	return Contract;
}
