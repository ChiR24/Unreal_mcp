// McpNativeGatewayCatalog.cpp — see header. The availability execute gate came out of McpNativeGatewayValidation.cpp,
// and the views out of the search and describe files, so each stays under the plugin's 250 pure-line ceiling.
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Editor.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "Runtime/Launch/Resources/Version.h"

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

FString McpRecordMinEngineVersion(const FMcpCapabilityRecord& Record)
{
	FString Version;
	if (Record.Availability.IsValid())
	{
		Record.Availability->TryGetStringField(TEXT("minEngineVersion"), Version);
	}
	return Version;
}

bool McpEngineVersionAtLeast(const FString& Version, int32 Major, int32 Minor)
{
	FString NeedMajor;
	FString NeedMinor;
	if (!Version.Split(TEXT("."), &NeedMajor, &NeedMinor) || NeedMajor.IsEmpty() || NeedMinor.IsEmpty() ||
		!NeedMajor.IsNumeric() || !NeedMinor.IsNumeric())
	{
		return false;
	}
	const int32 MajorNeeded = FCString::Atoi(*NeedMajor);
	return Major != MajorNeeded ? Major > MajorNeeded : Minor >= FCString::Atoi(*NeedMinor);
}

bool McpRunsOnThisEngine(const FMcpCapabilityRecord& Record)
{
	const FString Version = McpRecordMinEngineVersion(Record);
	return Version.IsEmpty() || McpEngineVersionAtLeast(Version, ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
}

bool McpCheckAvailabilityGate(const FMcpCapabilityRecord& Record, FString& OutCode, FString& OutMessage)
{
	// A handler compiled out of this engine's build would only answer that it cannot run here.
	if (!McpRunsOnThisEngine(Record))
	{
		OutCode = TEXT("ENGINE_VERSION_UNSUPPORTED");
		OutMessage = FString::Printf(TEXT("'%s' needs Unreal Engine %s or later; this editor is %d.%d."),
			*Record.Id, *McpRecordMinEngineVersion(Record), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
		return false;
	}
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
	OutCode = TEXT("EDITOR_STATE_MISMATCH");
	OutMessage = FString::Printf(
		TEXT("'%s' needs a running world (editor states: %s); start Play In Editor (control_editor play) first."),
		*Record.Id, *FString::Join(Names, TEXT(", ")));
	return false;
}

TSharedPtr<FJsonObject> McpCapabilityAvailabilityView(const FMcpCapabilityRecord& Record, bool bEnabled)
{
	auto View = MakeShared<FJsonObject>();
	const FString MinEngine = McpRecordMinEngineVersion(Record);
	const bool bRunsHere = McpRunsOnThisEngine(Record);
	View->SetStringField(TEXT("status"), !bEnabled ? TEXT("disabled") : bRunsHere ? TEXT("available") : TEXT("unavailable"));
	TArray<FString> Reasons;
	if (!bEnabled) Reasons.Add(TEXT("parent_tool_disabled"));
	if (!bRunsHere) Reasons.Add(TEXT("engine_version"));
	View->SetArrayField(TEXT("reasons"), GatewayStringArray(Reasons));
	if (!MinEngine.IsEmpty()) View->SetStringField(TEXT("minEngineVersion"), MinEngine);
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
	Row->SetBoolField(TEXT("runnable"), bEnabled && McpRunsOnThisEngine(Record));
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
