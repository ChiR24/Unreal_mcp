// McpNativeGatewayExecuteRequest.cpp — see header for the accepted forms.

#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"

const TArray<FString>& McpExecutionOptionKeys()
{
	static const TArray<FString> Keys = {
		TEXT("idempotencyKey"),
		TEXT("expectedCatalogRevision"),
		TEXT("expectedRevisions"),
		TEXT("preview"),
		TEXT("savePolicy"),
		TEXT("timeoutMs"),
		TEXT("validationLevel"),
		TEXT("taskPreference"),
	};
	return Keys;
}

namespace
{
TSharedPtr<FJsonObject> BuildGuidance(
	const TArray<FString>& Suggestions, const TSharedPtr<FJsonObject>& NextCall)
{
	TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
	Guidance->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
	if (NextCall.IsValid())
	{
		Guidance->SetObjectField(TEXT("nextCall"), NextCall);
	}
	return Guidance;
}

FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	FString Value;
	Object->TryGetStringField(Field, Value);
	return Value;
}
}

bool McpValidateExecutionOptions(
	const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
	if (!Options.IsValid())
	{
		return true;
	}
	const TArray<FString>& Supported = McpExecutionOptionKeys();
	for (const TPair<FString, TSharedPtr<FJsonValue>> Entry : Options->Values)
	{
		if (!Supported.Contains(Entry.Key))
		{
			OutError = McpOptionError(Entry.Key, Supported,
				FString::Printf(TEXT("Unsupported execution option '%s'. Supported: [%s]"),
					*Entry.Key, *FString::Join(Supported, TEXT(", "))));
			return false;
		}
	}

	const TSharedPtr<FJsonValue> Timeout = Options->TryGetField(TEXT("timeoutMs"));
	if (Timeout.IsValid())
	{
		const bool bNumeric = Timeout->Type == EJson::Number;
		const double Value = bNumeric ? Timeout->AsNumber() : 0.0;
		const bool bWholeNumber = bNumeric && FMath::IsFinite(Value) &&
			Value == FMath::TruncToDouble(Value);
		if (!bWholeNumber || Value <= 0.0 || Value > static_cast<double>(McpMaxExecutionTimeoutMs))
		{
			OutError = McpRangeError(TEXT("timeoutMs"),
				FString::Printf(TEXT("options.timeoutMs must be an integer in 1..%lld"),
					McpMaxExecutionTimeoutMs));
			return false;
		}
	}

	const TSharedPtr<FJsonValue> Preview = Options->TryGetField(TEXT("preview"));
	if (Preview.IsValid() && Preview->Type != EJson::Boolean)
	{
		OutError = McpValidationError(TEXT("INVALID_OPTIONS"),
			TEXT("options.preview must be a boolean."), TEXT("/options/preview"));
		return false;
	}
	// Mirrors the 1..128 bound in gateway-execute-validate.ts. Only the key NAME
	// was checked here, so a malformed value fell through to TryGetStringField in
	// McpNativeTransportGatewayExecute, left Context.IdempotencyId empty and took
	// the no-ledger path: a retry re-ran the mutation that the stdio surface had
	// already refused. A dedup guard that cannot be honoured must refuse. It lives
	// in McpNativeGatewayIdempotency.cpp because this file is at the line ceiling.
	return McpValidateIdempotencyKeyOption(Options, OutError);
}

namespace
{
bool ResolveFromCapabilityField(
	const FString& Capability, const FMcpCanonicalRecordIndex& Index,
	const FMcpCapabilityRecord*& OutRecord, FMcpSemanticError& OutError,
	TSharedPtr<FJsonObject>& OutGuidance)
{
	OutRecord = Index.FindById(Capability);
	if (OutRecord)
	{
		return true;
	}

	FString AliasTarget;
	switch (Index.ResolveAlias(Capability, AliasTarget))
	{
	case EMcpAliasResolution::Ambiguous:
		OutError = McpValidationError(TEXT("ALIAS_CONFLICT"),
			FString::Printf(TEXT("Alias '%s' resolves to more than one capability."), *Capability));
		return false;
	case EMcpAliasResolution::Unique:
		OutRecord = Index.FindById(AliasTarget);
		if (OutRecord)
		{
			return true;
		}
		break;
	case EMcpAliasResolution::Unknown:
	default:
		break;
	}

	const TArray<FString> Suggestions =
		GatewayClosestMatches(Capability, Index.GetCapabilityIds(), 3);
	OutGuidance = BuildGuidance(Suggestions,
		GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
	OutError = McpValidationError(TEXT("UNKNOWN_CAPABILITY"),
		FString::Printf(TEXT("Unknown capability '%s'. Call search before execute."), *Capability));
	return false;
}

bool ResolveFromLegacyFields(
	const FString& Tool, const FString& Action, const FMcpCanonicalRecordIndex& Index,
	const FMcpCapabilityRecord*& OutRecord, FMcpSemanticError& OutError,
	TSharedPtr<FJsonObject>& OutGuidance)
{
	OutRecord = Index.FindByLegacy(Tool, Action);
	if (OutRecord)
	{
		return true;
	}
	const TArray<FString> Actions = Index.GetLegacyActionsForTool(Tool);
	if (Actions.Num() == 0)
	{
		OutGuidance = BuildGuidance({},
			GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString()));
		OutError = McpValidationError(TEXT("UNKNOWN_TOOL"),
			FString::Printf(TEXT("Unknown tool '%s'. Call search before execute."), *Tool));
		return false;
	}
	const TArray<FString> Suggestions = GatewayClosestMatches(Action, Actions, 3);
	OutGuidance = BuildGuidance(Suggestions,
		GatewayBuildNextCall(TEXT("describe"), Tool,
			Suggestions.Num() > 0 ? Suggestions[0] : FString(), FString()));
	OutGuidance->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
	OutError = McpValidationError(TEXT("UNKNOWN_ACTION"),
		FString::Printf(TEXT("Unknown action '%s' for tool '%s'. Call describe before execute."),
			*Action, *Tool));
	return false;
}

bool RejectReservedParams(
	const TSharedPtr<FJsonObject>& Params, FMcpSemanticError& OutError)
{
	if (Params->HasField(TEXT("action")) || Params->HasField(TEXT("subAction")))
	{
		OutError = McpValidationError(TEXT("INVALID_PARAMS"),
			TEXT("params must not override action or subAction. "
				"Supply the selected action at the gateway level."));
		return false;
	}
	for (const FString& Control : McpExecutionOptionKeys())
	{
		if (Params->HasField(Control))
		{
			OutError = McpOptionError(Control, McpExecutionOptionKeys(),
				FString::Printf(TEXT("Gateway control '%s' must not appear in action params"), *Control));
			return false;
		}
	}
	return true;
}
}

bool McpParseGatewayExecuteRequest(
	const TSharedPtr<FJsonObject>& Params, FMcpGatewayExecuteRequest& OutRequest,
	FMcpSemanticError& OutError, TSharedPtr<FJsonObject>& OutGuidance)
{
	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	if (!Index.IsLoaded())
	{
		OutError = McpExecutionError(TEXT("CANONICAL_RECORDS_UNAVAILABLE"),
			FString::Printf(TEXT("Canonical capability records are unavailable: %s"),
				*Index.GetLoadError()),
			false);
		return false;
	}

	const FString Capability = ReadString(Params, TEXT("capability"));
	const FString Tool = ReadString(Params, TEXT("tool"));
	const FString Action = ReadString(Params, TEXT("action"));

	const FMcpCapabilityRecord* FromCapability = nullptr;
	if (!Capability.IsEmpty() &&
		!ResolveFromCapabilityField(Capability, Index, FromCapability, OutError, OutGuidance))
	{
		return false;
	}

	const FMcpCapabilityRecord* FromLegacy = nullptr;
	if (!Tool.IsEmpty() && !Action.IsEmpty() &&
		!ResolveFromLegacyFields(Tool, Action, Index, FromLegacy, OutError, OutGuidance))
	{
		return false;
	}

	if (FromCapability && FromLegacy && FromCapability->Id != FromLegacy->Id)
	{
		OutError = McpValidationError(TEXT("FORM_CONFLICT"),
			FString::Printf(TEXT("capability '%s' conflicts with tool/action '%s'"),
				*FromCapability->Id, *FromLegacy->Id));
		return false;
	}

	const FMcpCapabilityRecord* Record = FromCapability ? FromCapability : FromLegacy;
	if (!Record)
	{
		// describe {} lists the parent tools, so point the caller there (dogfood #2).
		OutGuidance = BuildGuidance({},
			GatewayBuildNextCall(TEXT("describe"), FString(), FString(), FString()));
		OutError = McpValidationError(TEXT("UNKNOWN_CAPABILITY"),
			TEXT("execute requires either capability or tool + action. Call describe with no arguments to list the parent tools, or search to find a capability."));
		return false;
	}

	// Bind the resolved capability id now so a later param/option failure still
	// carries it (the receipt then keeps its capability + revision triple, like
	// the TS gateway which resolves the capability before validating options).
	OutRequest.CapabilityId = Record->Id;

	TSharedPtr<FJsonObject> ActionParams;
	const TSharedPtr<FJsonValue> RawParams = Params->TryGetField(TEXT("params"));
	if (RawParams.IsValid() && RawParams->Type != EJson::Null)
	{
		if (RawParams->Type != EJson::Object)
		{
			OutError = McpValidationError(TEXT("INVALID_PARAMS"), TEXT("params must be an object."));
			return false;
		}
		ActionParams = RawParams->AsObject();
	}
	else
	{
		ActionParams = MakeShared<FJsonObject>();
	}
	if (!RejectReservedParams(ActionParams, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Options;
	const TSharedPtr<FJsonValue> RawOptions = Params->TryGetField(TEXT("options"));
	if (RawOptions.IsValid() && RawOptions->Type != EJson::Null)
	{
		if (RawOptions->Type != EJson::Object)
		{
			OutError = McpValidationError(TEXT("INVALID_OPTIONS"), TEXT("options must be an object."));
			return false;
		}
		Options = RawOptions->AsObject();
	}
	if (!McpValidateExecuteOptionsForCapability(Options, ActionParams, Record, OutError, OutGuidance))
	{
		return false;
	}

	OutRequest.Record = Record;
	OutRequest.Params = ActionParams;
	OutRequest.Options = Options;
	return true;
}
