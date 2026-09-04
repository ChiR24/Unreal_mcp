// McpNativeGatewayValidation.cpp — see header for the normative stage order.

#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "Editor.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"

namespace
{
// Mirror of HEX_REVISION (/^[0-9a-f]{1,64}$/) in ids.ts: a well-formed catalog
// revision digest is 1..64 lowercase hex characters. Anything else is malformed.
bool IsCatalogRevisionDigest(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64)
	{
		return false;
	}
	for (const TCHAR Ch : Value)
	{
		const bool bHex = (Ch >= '0' && Ch <= '9') || (Ch >= 'a' && Ch <= 'f');
		if (!bHex)
		{
			return false;
		}
	}
	return true;
}

TSharedPtr<FJsonObject> DisabledCapabilityGuidance(const FString& ParentTool)
{
	TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
	Guidance->SetStringField(TEXT("tool"), ParentTool);
	Guidance->SetObjectField(TEXT("nextCall"),
		GatewayBuildNextCall(TEXT("configure"), ParentTool, FString(), FString()));
	return Guidance;
}

TSharedPtr<FJsonObject> SchemaGuidance(
	const FString& ParentTool, const FString& Action, const FString& Pointer)
{
	TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
	Guidance->SetStringField(TEXT("tool"), ParentTool);
	Guidance->SetStringField(TEXT("action"), Action);
	if (!Pointer.IsEmpty())
	{
		Guidance->SetStringField(TEXT("pointer"), Pointer);
	}
	Guidance->SetObjectField(TEXT("nextCall"),
		GatewayBuildNextCall(TEXT("describe"), ParentTool, Action, FString()));
	return Guidance;
}
}

TSharedPtr<FJsonObject> ValidateAndResolveGatewayExecute(
	const TSharedPtr<FJsonObject>& GatewayParams,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	const FMcpReceiptContext& Context, FMcpGatewayExecutePlan& OutPlan)
{
	if (!GatewayParams.IsValid())
	{
		return McpBuildErrorReceipt(FString(),
			McpValidationError(TEXT("INVALID_PARAMS"), TEXT("execute requires an arguments object.")),
			Context);
	}

	FMcpGatewayExecuteRequest Request;
	FMcpSemanticError Error;
	TSharedPtr<FJsonObject> Guidance;
	if (!McpParseGatewayExecuteRequest(GatewayParams, Request, Error, Guidance))
	{
		return McpBuildErrorReceipt(Request.CapabilityId, Error, Context, Guidance);
	}

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	const FString ParentTool = Request.Record->Parent;
	const FString LegacyAction = Index.GetLegacyActionForCapability(Request.CapabilityId);

	FMcpToolDefinition* Tool = Registry.FindTool(ParentTool);
	if (!Tool)
	{
		return McpBuildErrorReceipt(Request.CapabilityId,
			McpValidationError(TEXT("UNKNOWN_TOOL"),
				FString::Printf(TEXT("Parent tool '%s' is not registered on this surface."), *ParentTool)),
			Context);
	}

	if (!ToolManager.IsToolEnabled(ParentTool))
	{
		return McpBuildErrorReceipt(Request.CapabilityId,
			McpCapabilityError(TEXT("TOOL_DISABLED"), TEXT("CAPABILITY_DISABLED"),
				FString::Printf(TEXT("Capability '%s' is disabled or unavailable."), *Request.CapabilityId),
				false),
			Context, DisabledCapabilityGuidance(ParentTool));
	}

	// Canonical per-action schemas mostly omit `action` (the action IS the
	// capability), so it is injected for validation only where declared.
	const TSharedPtr<FJsonObject> InputSchema = Request.Record->InputSchema;
	TSharedPtr<FJsonObject> WithDefaults = McpCoerceCanonicalVectorShapes(
		McpApplyCanonicalSchemaDefaults(Request.Params, InputSchema), InputSchema);

	TSharedPtr<FJsonObject> ToValidate = MakeShared<FJsonObject>();
	ToValidate->Values = WithDefaults->Values;
	if (McpSchemaDeclaresProperty(InputSchema, TEXT("action")))
	{
		ToValidate->SetStringField(TEXT("action"), LegacyAction);
	}

	FMcpSchemaViolationDetail Violation;
	if (!McpValidateObjectAgainstCanonicalSchema(ToValidate, InputSchema, Violation))
	{
		const FString Code = McpSchemaViolationCode(Violation.Reason);
		FMcpSemanticError SchemaError = Violation.Reason == EMcpSchemaViolation::Range
			? McpRangeError(Violation.Pointer, Violation.Message)
			: McpValidationError(Code, Violation.Message, Violation.Pointer);
		return McpBuildErrorReceipt(Request.CapabilityId, SchemaError, Context,
			SchemaGuidance(ParentTool, LegacyAction, Violation.Pointer));
	}

	// Editor-state gate (dogfood #91): a capability whose editorStates exclude "edit" needs a
	// running world; refuse it up front in plain edit mode instead of failing deep in a handler.
	{
		const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
		if (Request.Record->Availability.IsValid() &&
			Request.Record->Availability->TryGetArrayField(TEXT("editorStates"), States) && States && States->Num() > 0)
		{
			bool bAllowsEdit = false;
			TArray<FString> Names;
			for (const TSharedPtr<FJsonValue>& State : *States)
			{
				const FString Name = State.IsValid() ? State->AsString() : FString();
				bAllowsEdit |= Name.Equals(TEXT("edit"), ESearchCase::IgnoreCase);
				Names.Add(Name);
			}
			const bool bWorldRunning = GEditor && GEditor->PlayWorld != nullptr;
			if (!bAllowsEdit && !bWorldRunning)
			{
				return McpBuildErrorReceipt(Request.CapabilityId,
					McpValidationError(TEXT("EDITOR_STATE_MISMATCH"),
						FString::Printf(TEXT("'%s' needs a running world (editor states: %s); start Play In Editor (control_editor play) first."),
							*Request.CapabilityId, *FString::Join(Names, TEXT(", ")))),
					Context, nullptr);
			}
		}
	}
	// Task 39 pre-dispatch policy seam: a client that pinned the catalog revision
	// it planned against is refused before dispatch if the live digest moved on,
	// so a stale call never reaches the subsystem queue or editor work.
	if (Request.Options.IsValid() && Request.Options->HasField(TEXT("expectedCatalogRevision")))
	{
		const FString Current = FMcpCanonicalRecordIndex::Get().GetCatalogRevision();
		// TryGetStringField silently coerces a JSON number to its digits (so 12345
		// would read as the hex-looking "12345"), so the JSON type is checked
		// explicitly: only a genuine string is a candidate pin, matching the TS
		// CatalogRevisionSchema which rejects any non-string value.
		const TSharedPtr<FJsonValue>* PinValue = Request.Options->Values.Find(TEXT("expectedCatalogRevision"));
		const bool bIsStringPin = PinValue != nullptr && PinValue->IsValid()
			&& (*PinValue)->Type == EJson::String;
		FString Expected;
		if (!bIsStringPin || !(*PinValue)->TryGetString(Expected) || !IsCatalogRevisionDigest(Expected))
		{
			// Fail closed: a present-but-malformed pin (non-string / empty / non-hex
			// / over-length) is a validation error, never coerced into a stale-state
			// refusal, and can never skip the stale guard and reach the subsystem
			// queue or editor work.
			return McpBuildErrorReceipt(Request.CapabilityId,
				McpValidationError(TEXT("INVALID_OPTIONS"),
					TEXT("options.expectedCatalogRevision must be a lowercase hex catalog-revision digest of 1..64 characters."),
					TEXT("/options/expectedCatalogRevision")),
				Context);
		}
		if (Expected != Current)
		{
			return McpBuildErrorReceipt(Request.CapabilityId,
				McpStaleStateError(
					FString::Printf(
						TEXT("The capability catalog revision changed since it was read (expected '%s', current '%s'). Re-run search or describe and retry."),
						*Expected, *Current),
					Current, Expected),
				Context);
		}
	}

	// Task 42: the live-state pins are shape-checked here so a malformed envelope
	// is refused before dispatch, but the revision COMPARISON is deliberately not
	// done here — it belongs on the game thread immediately before mutation,
	// because a transport-thread snapshot is already stale by dispatch time.
	FMcpSemanticError RevisionError;
	if (!McpParseExpectedRevisions(Request.Options, OutPlan.ExpectedRevisions, RevisionError))
	{
		return McpBuildErrorReceipt(Request.CapabilityId, RevisionError, Context);
	}

	TSharedPtr<FJsonObject> Arguments = MakeShared<FJsonObject>();
	Arguments->Values = WithDefaults->Values;
	Arguments->SetStringField(TEXT("action"), LegacyAction);

	OutPlan.CapabilityId = Request.CapabilityId;
	OutPlan.ParentTool = ParentTool;
	OutPlan.LegacyAction = LegacyAction;
	OutPlan.DispatchAction = Tool->UsesToolNameDispatch() ? ParentTool : LegacyAction;
	OutPlan.Arguments = Arguments;
	OutPlan.OutputSchema = Request.Record->OutputSchema;
	return nullptr;
}

// McpProjectCanonicalOutput lives in MCP/Gateway/McpNativeGatewayOutputProjection.cpp.

TSharedPtr<FJsonObject> ValidateGatewayExecuteOutput(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& OutputSchema,
	const TSharedPtr<FJsonObject>& CanonicalOutput, const TSharedPtr<FJsonObject>& RawResult,
	const FMcpReceiptContext& Context)
{
	if (!OutputSchema.IsValid())
	{
		return nullptr;
	}

	FMcpSchemaViolationDetail Violation;
	if (McpValidateObjectAgainstCanonicalSchema(CanonicalOutput, OutputSchema, Violation))
	{
		return nullptr;
	}

	FMcpSemanticError Error = McpOutputError(TEXT("OUTPUT_SCHEMA_VIOLATION"),
		FString::Printf(TEXT("Capability '%s' returned a result its output schema refuses: %s"),
			*CapabilityId, *Violation.Message),
		Violation.Pointer);
	// The raw handler payload is retained verbatim so a schema violation never
	// discards the structured detail Unreal actually reported.
	Error.UnrealDetail = RawResult;
	return McpBuildErrorReceipt(CapabilityId, Error, Context);
}

bool McpValidateCanonicalToolArguments(
	const FString& ParentTool, const TSharedPtr<FJsonObject>& Arguments,
	FString& OutArgumentPath, FString& OutErrorCode, FString& OutErrorMessage)
{
	if (!Arguments.IsValid())
	{
		OutErrorCode = TEXT("INVALID_TOOL_ARGUMENT");
		OutErrorMessage = TEXT("Tool arguments could not be validated");
		return false;
	}

	FString Action;
	Arguments->TryGetStringField(TEXT("action"), Action);

	const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
	const FMcpCapabilityRecord* Record = Index.FindByLegacy(ParentTool, Action);
	if (!Record)
	{
		OutArgumentPath = TEXT("action");
		OutErrorCode = TEXT("UNKNOWN_ACTION");
		OutErrorMessage = FString::Printf(
			TEXT("Unknown action '%s' for tool '%s'."), *Action, *ParentTool);
		return false;
	}

	// action/subAction are transport-owned routing fields; they are validated
	// only when the capability's own schema declares them.
	TSharedPtr<FJsonObject> Candidate = McpApplyCanonicalSchemaDefaults(
		Arguments, Record->InputSchema);
	if (!McpSchemaDeclaresProperty(Record->InputSchema, TEXT("action")))
	{
		Candidate->RemoveField(TEXT("action"));
	}
	if (!McpSchemaDeclaresProperty(Record->InputSchema, TEXT("subAction")))
	{
		Candidate->RemoveField(TEXT("subAction"));
	}

	FMcpSchemaViolationDetail Violation;
	if (McpValidateObjectAgainstCanonicalSchema(Candidate, Record->InputSchema, Violation))
	{
		return true;
	}
	OutArgumentPath = Violation.Pointer;
	OutErrorCode = McpSchemaViolationCode(Violation.Reason);
	OutErrorMessage = Violation.Message;
	return false;
}
