// McpNativeGatewayCatalog.cpp — see header. The availability execute gate came out of McpNativeGatewayValidation.cpp,
// and the views out of the search and describe files, so each stays under the plugin's 250 pure-line ceiling.
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "Core/Compatibility/McpCompiledFeatures.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/Reflect/McpEngineRoles.h"
#include "Editor.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "Modules/ModuleManager.h"

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

bool AllowsPlayState(const TArray<TSharedPtr<FJsonValue>>& States)
{
	for (const TSharedPtr<FJsonValue>& State : States)
	{
		const FString Name = State.IsValid() ? State->AsString() : FString();
		if (Name.Equals(TEXT("pie"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("simulate"), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}
}

FString McpMissingEngineRole(const FMcpCapabilityRecord& Record, FString* OutWhy)
{
	const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
	if (Record.Availability.IsValid() && Record.Availability->TryGetArrayField(TEXT("roles"), Listed) && Listed)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Listed)
		{
			const FString Name = Value.IsValid() ? Value->AsString() : FString();
			if (Name.IsEmpty() || !McpEngineRoles::IsKnownResolved(FName(*Name), OutWhy))
			{
				return Name.IsEmpty() ? FString(TEXT("(unnamed)")) : Name; // fail closed on a malformed entry
			}
		}
	}
	return FString();
}

FString McpMissingCompiledFeature(const FMcpCapabilityRecord& Record)
{
	const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
	if (Record.Availability.IsValid() && Record.Availability->TryGetArrayField(TEXT("compiled"), Listed) && Listed)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Listed)
		{
			const FString Name = Value.IsValid() ? Value->AsString() : FString();
			if (!McpCompiledFeatures::Has(Name))
			{
				return Name.IsEmpty() ? FString(TEXT("(unnamed)")) : Name; // fail closed on a malformed entry
			}
		}
	}
	return FString();
}

bool McpRunsOnThisEngine(const FMcpCapabilityRecord& Record)
{
	return McpMissingCompiledFeature(Record).IsEmpty() && McpMissingEngineRole(Record).IsEmpty();
}

bool McpCheckAvailabilityGate(const FMcpCapabilityRecord& Record, FString& OutCode, FString& OutMessage)
{
	// A handler compiled out of this build would only answer that it cannot run here.
	const FString Missing = McpMissingCompiledFeature(Record);
	if (!Missing.IsEmpty())
	{
		OutCode = TEXT("FEATURE_UNAVAILABLE");
		OutMessage = FString::Printf(TEXT("'%s' needs %s, which this build of EngineWire left out: this engine (or its "
			"package) lacks what it is built on."), *Record.Id, *Missing);
		return false;
	}
	// A handler that reaches the engine by reflection needs what plays each of its roles on this engine.
	FString RoleWhy;
	const FString Role = McpMissingEngineRole(Record, &RoleWhy);
	if (!Role.IsEmpty())
	{
		OutCode = TEXT("FEATURE_UNAVAILABLE");
		OutMessage = FString::Printf(TEXT("'%s' reaches the engine role %s, which nothing plays on this engine (%s). "
			"inspect describe_engine_roles lists every role and what was searched."), *Record.Id, *Role, *RoleWhy);
		return false;
	}
	// While Live Coding compiles, the engine marks no package dirty (MarkPackageDirty skips while a reload is active):
	// moves, spawns and mesh swaps made then read "nothing unsaved", save_all wrote nothing, and a level load dropped them.
	if (IsReloadActive() && !Record.Effect.Equals(TEXT("read"), ESearchCase::IgnoreCase))
	{
		OutCode = TEXT("LIVE_CODING_ACTIVE");
		OutMessage = FString::Printf(TEXT("'%s' changes the editor, and a Live Coding compile is running: until it ends the engine "
			"marks nothing unsaved, so the change would be lost. Call again once the log says \"Live coding succeeded\"."), *Record.Id);
		return false;
	}
	// Editor-state gate: a capability whose editorStates exclude
	// "edit" needs a running world; refuse it up front in plain edit mode
	// instead of failing deep in a handler.
	const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
	if (!Record.Availability.IsValid() ||
		!Record.Availability->TryGetArrayField(TEXT("editorStates"), States) || !States || States->Num() == 0)
	{
		return true;
	}
	TArray<FString> Names;
	const bool bAllowsEdit = AllowsEditState(*States, Names);
	const bool bPlaying = GEditor && GEditor->PlayWorld != nullptr;
	// The other direction: a change whose record declares no play state is refused while Play runs, as its contract
	// says, instead of landing on assets the running game is using. Reads stay open during Play.
	if (bPlaying && !Record.Effect.Equals(TEXT("read"), ESearchCase::IgnoreCase) && !AllowsPlayState(*States))
	{
		OutCode = TEXT("EDITOR_STATE_MISMATCH");
		OutMessage = FString::Printf(
			TEXT("'%s' changes the editor and runs only outside Play In Editor (editor states: %s); call again once Play "
				"has ended (a Play the user started is theirs to stop)."),
			*Record.Id, *FString::Join(Names, TEXT(", ")));
		return false;
	}
	if (bAllowsEdit || bPlaying)
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
	const FString Missing = McpMissingCompiledFeature(Record);
	FString RoleWhy;
	const FString Role = McpMissingEngineRole(Record, &RoleWhy);
	const bool bRunsHere = Missing.IsEmpty() && Role.IsEmpty();
	View->SetStringField(TEXT("status"), !bEnabled ? TEXT("disabled") : bRunsHere ? TEXT("available") : TEXT("unavailable"));
	TArray<FString> Reasons;
	if (!bEnabled) Reasons.Add(TEXT("parent_tool_disabled"));
	if (!Missing.IsEmpty()) Reasons.Add(TEXT("not_compiled"));
	else if (!Role.IsEmpty()) Reasons.Add(TEXT("engine_role"));
	View->SetArrayField(TEXT("reasons"), McpHandlerUtils::ToJsonStringArray(Reasons));
	if (!Missing.IsEmpty()) View->SetStringField(TEXT("missingFeature"), Missing);
	if (Missing.IsEmpty() && !Role.IsEmpty())
	{
		View->SetStringField(TEXT("missingRole"), Role);
		View->SetStringField(TEXT("missingRoleWhy"), RoleWhy);
	}
	// Declared requirements are data, not decided here: discovery has no live editor state to probe.
	const TCHAR* const Declared[] = {TEXT("requiredPlugins"), TEXT("editorStates"), TEXT("compiled"), TEXT("roles")};
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
		// Most records need no compiled feature and no engine role: an empty list there was bytes on every search row.
		const bool bOptional = FCString::Strcmp(Field, TEXT("compiled")) == 0 || FCString::Strcmp(Field, TEXT("roles")) == 0;
		if (Values.Num() > 0 || !bOptional) View->SetArrayField(Field, McpHandlerUtils::ToJsonStringArray(Values));
	}
	return View;
}

FString McpOneLineSummary(const FString& Summary)
{
	// The first sentence: a ". " that ends one, not one inside an abbreviation (e.g., i.e., etc.).
	for (int32 At = Summary.Find(TEXT(". ")); At != INDEX_NONE; At = Summary.Find(TEXT(". "), ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 2))
	{
		const FString Before = Summary.Left(At).Right(4);
		if (!Before.EndsWith(TEXT("e.g")) && !Before.EndsWith(TEXT("i.e")) && !Before.EndsWith(TEXT("etc")) && !Before.EndsWith(TEXT(" vs"))) return Summary.Left(At + 1);
	}
	return Summary;
}

// A search row says what a model needs to choose: the capability, a one-line summary, its effect and how to go on.
// The rest describe answers (the output contract, the policy, the declared requirements), so a row carries the
// availability block only when the capability does not run here, and the policy only when the call needs a grant: on
// most rows they read "available" and "none", and with every output name a 12-row page was about 14.5 KB.
TSharedPtr<FJsonObject> McpSearchRow(const FMcpCapabilityRecord& Record, bool bEnabled)
{
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("capability"), Record.Id);
	Row->SetStringField(TEXT("parentTool"), Record.Parent);
	Row->SetStringField(TEXT("action"), McpCapabilityPublicAction(Record));
	Row->SetStringField(TEXT("domain"), Record.Domain);
	Row->SetStringField(TEXT("family"), Record.Family);
	Row->SetStringField(TEXT("summary"), McpOneLineSummary(Record.Summary));
	Row->SetStringField(TEXT("effect"), Record.Effect);
	const bool bRunnable = bEnabled && McpRunsOnThisEngine(Record);
	if (!bRunnable) Row->SetObjectField(TEXT("availability"), McpCapabilityAvailabilityView(Record, bEnabled));
	FString Consent;
	if (Record.Policy.IsValid() && Record.Policy->TryGetStringField(TEXT("consent"), Consent) && !Consent.Equals(TEXT("none"), ESearchCase::IgnoreCase))
	{
		Row->SetObjectField(TEXT("policy"), Record.Policy);
	}
	Row->SetBoolField(TEXT("runnable"), bRunnable);
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

void McpSearchEmptyAdvice(FJsonObject& Out, const FString& Query)
{
	// An empty page is where a caller starts inventing names: say what to change and hand over the one call that always works.
	Out.SetStringField(TEXT("message"), Query.IsEmpty()
		? FString(TEXT("No capability matches these filters. Remove a filter, or call describe with no selector to browse domains."))
		: FString::Printf(TEXT("No capability matched '%s'. Use 2-4 plain words naming the verb and the object (e.g. 'spawn actor'), drop any filter, or call describe with no selector to browse domains. Game logic no capability makes in one call is built as a Blueprint graph: search 'blueprint logic'."), *Query));
	Out.SetObjectField(TEXT("nextCall"), GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString()));
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
