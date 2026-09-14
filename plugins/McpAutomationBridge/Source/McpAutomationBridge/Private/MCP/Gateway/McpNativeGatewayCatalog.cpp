// McpNativeGatewayCatalog.cpp â€” the editor-state execute gate, extracted from
// McpNativeGatewayValidation.cpp so that file stays under the plugin's 250
// pure-line ceiling.
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Editor.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

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
