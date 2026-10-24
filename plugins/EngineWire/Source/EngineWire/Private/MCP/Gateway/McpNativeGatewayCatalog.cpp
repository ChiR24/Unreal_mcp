// McpNativeGatewayCatalog.cpp — see header. The editor-state execute gate came out of McpNativeGatewayValidation.cpp,
// and the views out of the search and describe files, so each stays under the plugin's 250 pure-line ceiling.
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Editor.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

namespace
{
bool AllowsEditState(const TArray<TSharedPtr<FJsonValue>>& States, TArray<FString>& OutNames)
{
	bool bAllowsEdit = false;
	for (const TSharedPtr<FJsonValue>& State : States)
	{
		const FString Name = State.IsValid() ? State->AsString() : FString();
		bAllowsEdit |= Name.Equals(TEXT("edit"), ESearchCase::IgnoreCase);
		OutNames.Add(Name);
	}
	return bAllowsEdit;
}
}

bool McpCheckEditorStateGate(const FMcpCapabilityRecord& Record, FString& OutMessage)
{
	// Editor-state gate (dogfood #91): a capability whose editorStates exclude
	// "edit" needs a running world; refuse it up front in plain edit mode
	// instead of failing deep in a handler.
	const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
	if (!Record.Availability.IsValid() ||
		!Record.Availability->TryGetArrayField(TEXT("editorStates"), States) || !States || States->Num() == 0)
	{
		return true;
	}
	TArray<FString> Names;
	if (AllowsEditState(*States, Names) || (GEditor && GEditor->PlayWorld != nullptr))
	{
		return true;
	}
	OutMessage = FString::Printf(
		TEXT("'%s' needs a running world (editor states: %s); start Play In Editor (control_editor play) first."),
		*Record.Id, *FString::Join(Names, TEXT(", ")));
	return false;
}

TSharedPtr<FJsonObject> McpCapabilityAvailabilityView(const FMcpCapabilityRecord& Record, bool bEnabled)
{
	auto View = MakeShared<FJsonObject>();
	View->SetStringField(TEXT("status"), bEnabled ? TEXT("available") : TEXT("disabled"));
	TArray<FString> Reasons;
	if (!bEnabled) Reasons.Add(TEXT("parent_tool_disabled"));
	View->SetArrayField(TEXT("reasons"), GatewayStringArray(Reasons));
	// Declared requirements are data, not decided here: discovery has no live editor state to probe.
	const TCHAR* const Declared[] = {TEXT("requiredPlugins"), TEXT("editorStates")};
	for (const TCHAR* Field : Declared)
	{
		TArray<FString> Values;
		const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
		if (Record.Availability.IsValid() && Record.Availability->TryGetArrayField(Field, Listed) && Listed)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Listed)
			{
				if (Value.IsValid() && Value->Type == EJson::String) Values.Add(Value->AsString());
			}
		}
		Values.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });
		View->SetArrayField(Field, GatewayStringArray(Values));
	}
	return View;
}

TSharedPtr<FJsonObject> McpSearchRow(const FMcpCapabilityRecord& Record, bool bEnabled)
{
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("capability"), Record.Id);
	Row->SetStringField(TEXT("parentTool"), Record.Parent);
	Row->SetStringField(TEXT("action"), McpCapabilityPublicAction(Record));
	Row->SetStringField(TEXT("domain"), Record.Domain);
	Row->SetStringField(TEXT("family"), Record.Family);
	Row->SetStringField(TEXT("summary"), Record.Summary);
	Row->SetStringField(TEXT("effect"), Record.Effect);
	Row->SetObjectField(TEXT("availability"), McpCapabilityAvailabilityView(Record, bEnabled));
	Row->SetObjectField(TEXT("policy"), Record.Policy.IsValid() ? Record.Policy : MakeShared<FJsonObject>());
	TArray<FString> Outputs;
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Record.OutputSchema.IsValid() && Record.OutputSchema->TryGetObjectField(TEXT("properties"), Properties) && Properties)
	{
		for (const auto& Field : (*Properties)->Values) Outputs.Emplace(*Field.Key);  // 5.8 keys are not FString
	}
	Row->SetArrayField(TEXT("outputs"), GatewayStringArray(Outputs));
	Row->SetBoolField(TEXT("runnable"), bEnabled);
	TSharedPtr<FJsonObject> Next;
	if (bEnabled)
	{
		Next = GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString());
		Next->SetStringField(TEXT("capability"), Record.Id);
	}
	else
	{
		Next = GatewayEnableToolNextCall(Record.Parent);
	}
	Row->SetObjectField(TEXT("nextCall"), Next);
	return Row;
}

TSharedPtr<FJsonObject> McpSearchFilters(const FMcpDiscoveryQuery& Input)
{
	auto Filters = MakeShared<FJsonObject>();
	if (Input.bHasDomain) Filters->SetStringField(TEXT("domain"), Input.Domain);
	if (Input.bHasFamily) Filters->SetStringField(TEXT("family"), Input.Family);
	if (Input.bHasTool) Filters->SetStringField(TEXT("tool"), Input.Tool);
	if (Input.bHasAction) Filters->SetStringField(TEXT("action"), Input.Action);
	if (Input.bHasCapability) Filters->SetStringField(TEXT("capability"), Input.Capability);
	if (Input.bHasEffect) Filters->SetStringField(TEXT("effect"), Input.Effect);
	return Filters;
}
