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

// A required parameter's type, or its values joined by '|'.
FString SearchSigType(const TSharedPtr<FJsonObject>& Properties, const FString& Name)
{
	const TSharedPtr<FJsonObject>* Schema = nullptr;
	if (!Properties.IsValid() || !Properties->TryGetObjectField(Name, Schema) || !Schema) return TEXT("unknown");
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if ((*Schema)->TryGetArrayField(TEXT("enum"), Values) && Values)
	{
		TArray<FString> Names;
		for (const TSharedPtr<FJsonValue>& Value : *Values) Names.Add(Value.IsValid() ? Value->AsString() : FString());
		return FString::Join(Names, TEXT("|"));
	}
	FString Type;
	return (*Schema)->TryGetStringField(TEXT("type"), Type) ? Type : FString(TEXT("unknown"));
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

TSharedPtr<FJsonObject> McpSearchSignature(const FMcpCapabilityRecord& Record)
{
	auto IsControl = [](const FString& Name) { return Name.Equals(TEXT("action"), ESearchCase::CaseSensitive) || Name.Equals(TEXT("subAction"), ESearchCase::CaseSensitive); };
	const TSharedPtr<FJsonObject> Input = Record.InputSchema.IsValid() ? Record.InputSchema : MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* PropsPtr = nullptr;
	const TSharedPtr<FJsonObject> Properties = Input->TryGetObjectField(TEXT("properties"), PropsPtr) && PropsPtr ? *PropsPtr : nullptr;
	auto Required = MakeShared<FJsonObject>();
	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	if (Input->TryGetArrayField(TEXT("required"), Names) && Names)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Names)
		{
			const FString Name = Value.IsValid() ? Value->AsString() : FString();
			if (!IsControl(Name) && !Name.Equals(Record.DispatchBySelector, ESearchCase::CaseSensitive)) Required->SetStringField(Name, SearchSigType(Properties, Name));
		}
	}
	auto Signature = MakeShared<FJsonObject>();
	Signature->SetObjectField(TEXT("required"), Required);
	const TArray<TSharedPtr<FJsonValue>>* OneOf = nullptr;
	if (Input->TryGetArrayField(TEXT("requiredOneOf"), OneOf) && OneOf) Signature->SetArrayField(TEXT("oneOf"), *OneOf);
	const TSharedPtr<FJsonObject> VariantRequired = McpVariantRequired(Record);
	if (VariantRequired.IsValid()) Signature->SetObjectField(TEXT("variantRequired"), VariantRequired);
	if (!Record.DispatchBySelector.IsEmpty())
	{
		auto Selector = MakeShared<FJsonObject>();
		Selector->SetStringField(TEXT("name"), Record.DispatchBySelector);
		const TSharedPtr<FJsonObject>* Schema = nullptr;
		const bool bSchema = Properties.IsValid() && Properties->TryGetObjectField(Record.DispatchBySelector, Schema) && Schema;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		Selector->SetArrayField(TEXT("values"), bSchema && (*Schema)->TryGetArrayField(TEXT("enum"), Values) && Values
			? *Values : TArray<TSharedPtr<FJsonValue>>());
		if (bSchema && (*Schema)->HasField(TEXT("default"))) Selector->SetField(TEXT("default"), (*Schema)->TryGetField(TEXT("default")));
		Signature->SetObjectField(TEXT("selector"), Selector);
	}
	// The example's own call: the pins its action name implies, then its params, without the gateway-level controls.
	auto Example = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ExampleInput = nullptr;
	if (Record.Examples.Num() > 0 && Record.Examples[0].IsValid() && Record.Examples[0]->Type == EJson::Object &&
		Record.Examples[0]->AsObject()->TryGetObjectField(TEXT("input"), ExampleInput) && ExampleInput)
	{
		FString Action;
		(*ExampleInput)->TryGetStringField(TEXT("action"), Action);
		const FMcpLegacyPair* Pinned = Record.LegacyPairs.FindByPredicate(
			[&Action](const FMcpLegacyPair& Pair) { return Pair.IsFolded() && Pair.Action.Equals(Action, ESearchCase::CaseSensitive); });
		if (Pinned) Example->Values = Pinned->FoldedPins->Values;
		for (const auto& Field : (*ExampleInput)->Values)
		{
			const FString Key(Field.Key.Len(), *Field.Key);  // 5.8 keys are not FString
			if (!IsControl(Key)) Example->SetField(Key, Field.Value);
		}
	}
	Signature->SetObjectField(TEXT("example"), Example);
	return Signature;
}
