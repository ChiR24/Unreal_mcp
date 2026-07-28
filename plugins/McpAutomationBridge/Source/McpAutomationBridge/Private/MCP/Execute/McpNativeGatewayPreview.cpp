// McpNativeGatewayPreview.cpp — options.preview is refused, never faked.
//
// `preview` is an accepted execution option that no dispatch path implements:
// nothing between this stage and the subsystem queue can produce a dry run.
// Accepting it would mean performing the real, irreversible editor mutation and
// then reporting it to the client as a preview, so it is refused here — before
// the request is queued and before any editor work exists to undo.
//
// The refusal is uniform, and behavior.supportsPreview is deliberately NOT
// consulted. 124 canonical records declare it, 10 of them destructive
// (asset.delete, asset.bulk_delete, datatable.clear_data_table_rows, ...), and
// not one declaration is backed by an implementation. Trusting it would leave
// the fake dry run in place for exactly the most dangerous capabilities.
//
// This lives beside McpNativeGatewayExecuteRequest.cpp rather than inside it
// because that file sits at the 250 pure-line ceiling.

#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"

namespace
{
TArray<FString> HonoredExecutionOptionKeys()
{
	TArray<FString> Honored = McpExecutionOptionKeys();
	Honored.Remove(TEXT("preview"));
	return Honored;
}

// The caller's own request minus the control the gateway cannot honor IS the
// call that will run for real, so the refusal hands back something executable.
TSharedPtr<FJsonObject> PreviewFreeNextCall(
	const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Params,
	const FMcpCapabilityRecord& Record)
{
	const FString Action =
		FMcpCanonicalRecordIndex::Get().GetLegacyActionForCapability(Record.Id);
	TSharedPtr<FJsonObject> NextCall =
		GatewayBuildNextCall(TEXT("execute"), Record.Parent, Action, FString());
	NextCall->SetObjectField(TEXT("params"),
		Params.IsValid() ? Params : MakeShared<FJsonObject>());

	TSharedPtr<FJsonObject> Remaining = MakeShared<FJsonObject>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Options->Values)
	{
		if (Entry.Key != TEXT("preview"))
		{
			Remaining->SetField(Entry.Key, Entry.Value);
		}
	}
	if (Remaining->Values.Num() > 0)
	{
		NextCall->SetObjectField(TEXT("options"), Remaining);
	}
	return NextCall;
}
}

bool McpValidateExecuteOptionsForCapability(
	const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Params,
	const FMcpCapabilityRecord* Record, FMcpSemanticError& OutError,
	TSharedPtr<FJsonObject>& OutGuidance)
{
	if (!McpValidateExecutionOptions(Options, OutError))
	{
		return false;
	}
	if (!Options.IsValid() || !Record)
	{
		return true;
	}

	// A non-boolean preview was already refused as INVALID_OPTIONS above, so the
	// value here is absent or a real boolean; only an explicit true is a request
	// for a dry run. `preview: false` asks for real execution and is honored.
	const TSharedPtr<FJsonValue> Preview = Options->TryGetField(TEXT("preview"));
	if (!Preview.IsValid() || Preview->Type != EJson::Boolean || !Preview->AsBool())
	{
		return true;
	}

	OutGuidance = MakeShared<FJsonObject>();
	OutGuidance->SetArrayField(TEXT("suggestions"), GatewayStringArray(
		GatewayClosestMatches(TEXT("preview"), HonoredExecutionOptionKeys(), 3)));
	OutGuidance->SetObjectField(TEXT("nextCall"),
		PreviewFreeNextCall(Options, Params, *Record));
	OutError = McpValidationError(TEXT("UNSUPPORTED_PREVIEW"), FString::Printf(
		TEXT("Capability '%s' does not implement options.preview. No dispatch path performs a dry run, so preview:true would perform the real operation. Re-send without options.preview to execute for real."),
		*Record->Id), TEXT("/options/preview"));
	return false;
}
