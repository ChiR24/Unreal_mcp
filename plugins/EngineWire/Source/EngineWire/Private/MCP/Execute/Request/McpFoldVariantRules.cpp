// McpFoldVariantRules.cpp — see header for the contract.

#include "MCP/Execute/Request/McpFoldVariantRules.h"
#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewaySchemaKeywords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

namespace
{
TArray<FString> RequirementNames(const TSharedPtr<FJsonObject>& Requirement, const TCHAR* Field)
{
	TArray<FString> Names;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Requirement.IsValid() && Requirement->TryGetArrayField(Field, Values) && Values)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (Value.IsValid() && Value->Type == EJson::String) Names.Add(Value->AsString());
		}
	}
	return Names;
}

TSharedPtr<FJsonObject> RequirementOf(const FMcpCapabilityRecord& Record, const FString& Action)
{
	const TSharedPtr<FJsonObject>* Requirement = nullptr;
	return Record.DispatchByRequires.IsValid() && Record.DispatchByRequires->TryGetObjectField(Action, Requirement) && Requirement
		? *Requirement : nullptr;
}
}

// One receipt warning per parameter a call sent (the caller's names, before defaults) that only other
// variants read: inspect_graph info=node with pinName answered every pin.
TArray<FString> McpUnreadVariantParams(
	const FMcpCapabilityRecord& Record, const TArray<FString>& Sent, const TSharedPtr<FJsonObject>& Params)
{
	TArray<FString> Out;
	FString Selected;
	if (Record.DispatchByDeclaredBy.Num() == 0 || !Params.IsValid() ||
		!Params->TryGetStringField(Record.DispatchBySelector, Selected))
	{
		return Out;
	}
	for (const FString& Name : Sent)
	{
		const TArray<FString>* Owners = Record.DispatchByDeclaredBy.Find(Name);
		if (Owners && !Owners->Contains(Selected))
		{
			Out.Add(FString::Printf(TEXT("%s is read only when %s is %s; this %s=%s call did not use it."),
				*Name, *Record.DispatchBySelector, *FString::Join(*Owners, TEXT(" or ")),
				*Record.DispatchBySelector, *Selected));
		}
	}
	return Out;
}

bool McpCheckVariantRequirements(
	const FMcpCapabilityRecord& Record, const FString& DispatchTarget,
	const TSharedPtr<FJsonObject>& Params, FMcpSchemaViolationDetail& OutViolation)
{
	const TSharedPtr<FJsonObject> Requirement = RequirementOf(Record, DispatchTarget);
	if (!Requirement.IsValid() || !Params.IsValid())
	{
		return true;
	}
	const TSharedPtr<FJsonValue> Selected = Params->TryGetField(Record.DispatchBySelector);
	const FString When = Selected.IsValid() && Selected->Type == EJson::String
		? FString::Printf(TEXT("when %s is %s"), *Record.DispatchBySelector, *Selected->AsString())
		: FString::Printf(TEXT("when action is %s"), *DispatchTarget);
	TArray<FString> Missing = RequirementNames(Requirement, TEXT("required"));
	Missing.RemoveAll([&Params](const FString& Name) { return Params->HasField(Name); });
	if (Missing.Num() > 0)
	{
		OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::MissingRequired, TEXT("/") + Missing[0],
			Missing.Num() == 1
				? FString::Printf(TEXT("Missing required parameter '%s' %s"), *Missing[0], *When)
				: FString::Printf(TEXT("Missing required parameters %s: %s"), *When, *FString::Join(Missing, TEXT(", "))));
		return false;
	}
	const TArray<FString> Group = RequirementNames(Requirement, TEXT("oneOf"));
	if (Group.Num() == 0 || Group.ContainsByPredicate([&Params](const FString& Name) { return Params->HasField(Name); }))
	{
		return true;
	}
	OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::RequiredOneOf, TEXT("/requiredOneOf"),
		FString::Printf(TEXT("At least one of [%s] must be provided %s"), *FString::Join(Group, TEXT(", ")), *When));
	return false;
}

TSharedPtr<FJsonObject> McpVariantRequired(const FMcpCapabilityRecord& Record)
{
	if (!Record.DispatchByRequires.IsValid())
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Entry : Record.DispatchByActions)
	{
		const TSharedPtr<FJsonObject> Requirement = RequirementOf(Record, Entry.Value);
		if (!Requirement.IsValid()) continue;
		TArray<FString> Names = RequirementNames(Requirement, TEXT("required"));
		const TArray<FString> Group = RequirementNames(Requirement, TEXT("oneOf"));
		// A one-name group the variant also requires ("blueprintPath", "blueprintPath") is listed once.
		if (Group.Num() > 0) Names.AddUnique(FString::Join(Group, TEXT("|")));
		Out->SetArrayField(Entry.Key, GatewayStringArray(Names));
	}
	return Out->Values.Num() > 0 ? Out : nullptr;
}

void McpSetVariantRequirementViews(
	const TSharedPtr<FJsonObject>& View, const FMcpCapabilityRecord& Record, const FString& Name)
{
	const TSharedPtr<FJsonObject> Required = McpVariantRequired(Record);
	if (!Required.IsValid())
	{
		return;
	}
	if (Name.Equals(Record.DispatchBySelector, ESearchCase::CaseSensitive))
	{
		View->SetObjectField(TEXT("variantRequired"), Required);
		return;
	}
	TArray<FString> RequiredFor;
	for (const TPair<FString, FString>& Entry : Record.DispatchByActions)
	{
		const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
		if (Required->TryGetArrayField(Entry.Key, Names) && Names && Names->ContainsByPredicate(
			[&Name](const TSharedPtr<FJsonValue>& Value) { return Value.IsValid() && Value->AsString().Equals(Name, ESearchCase::CaseSensitive); }))
		{
			RequiredFor.Add(Entry.Key);
		}
	}
	if (RequiredFor.Num() > 0)
	{
		View->SetArrayField(TEXT("requiredFor"), GatewayStringArray(RequiredFor));
	}
}
