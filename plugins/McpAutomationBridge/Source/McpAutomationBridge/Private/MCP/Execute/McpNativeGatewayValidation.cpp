// McpNativeGatewayValidation.cpp — see header for the normative stage order.

#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "Editor.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayFolding.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Registry/McpToolDefinition.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

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

// The parameters a call sent (the caller's names, before defaults) that only other variants of its
// folded family read. The family declares them, so the call is accepted, and the variant that ran ignored
// them without a word: inspect_graph info=node with pinName answered every pin. One receipt warning each
// (mirror of unreadVariantParams in gateway-dispatch-by.ts).
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
}

TSharedPtr<FJsonObject> ValidateAndResolveGatewayExecute(
	const TSharedPtr<FJsonObject>& GatewayParams,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	FMcpReceiptContext& Context, FMcpGatewayExecutePlan& OutPlan)
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
	const bool bParsed = McpParseGatewayExecuteRequest(GatewayParams, Request, Error, Guidance);
	Context.Provenance = Request.Provenance;
	if (!bParsed)
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
			Context, GatewayDisabledCapabilityGuidance(ParentTool));
	}

	// Canonical per-action schemas mostly omit `action` (the action IS the
	// capability), so it is injected for validation only where declared.
	const TSharedPtr<FJsonObject> InputSchema = Request.Record->InputSchema;
	// A folded family: an old name implies selector values, pinned before
	// defaults and validation so the folded contract accepts the old call
	// (mirror of applyFoldedPins in gateway-execute.ts). A caller who names the
	// old action AND sends a disagreeing selector value is contradictory, not
	// legacy — refuse instead of letting the mismatch ride into the handler.
	const FString RequestedAction = McpRequestedLegacyAction(GatewayParams, Request.CapabilityId);
	TSharedPtr<FJsonObject> SentParams = MakeShared<FJsonObject>();
	if (Request.Params.IsValid())
	{
		SentParams->Values = Request.Params->Values;
	}
	FString ConflictKey, PinnedValue, SentValue;
	if (!McpApplyFoldedPins(*Request.Record, RequestedAction, Request.Params, &ConflictKey, &PinnedValue))
	{
		// Name the pinned value and hand back the primary action with the same
		// params (mirror of gateway-execute-static-check.ts): "conflicts with the
		// one supplied" left a caller with nothing to try next.
		SentParams->TryGetStringField(ConflictKey, SentValue);
		TSharedPtr<FJsonObject> ConflictGuidance = GatewaySchemaGuidance(ParentTool, LegacyAction, TEXT("/") + ConflictKey);
		TSharedPtr<FJsonObject> NextCall = GatewayBuildNextCall(TEXT("execute"), ParentTool, LegacyAction, FString());
		NextCall->SetObjectField(TEXT("params"), SentParams);
		ConflictGuidance->SetObjectField(TEXT("nextCall"), NextCall);
		return McpBuildErrorReceipt(Request.CapabilityId,
			McpValidationError(TEXT("INVALID_PARAMETER_VALUE"),
				FString::Printf(TEXT("'%s' always runs with %s \"%s\", but the call sent %s \"%s\". '%s' takes any %s: nextCall runs it with these params."),
					*RequestedAction, *ConflictKey, *PinnedValue, *ConflictKey, *SentValue, *LegacyAction, *ConflictKey),
				TEXT("/") + ConflictKey),
			Context, ConflictGuidance);
	}
	// Before defaults fill the selector in (mirror of inferSelector).
	McpInferFoldSelector(*Request.Record, Request.Params);
	TArray<FString> SentNames;
	if (Request.Params.IsValid())
	{
		for (const auto& Pair : Request.Params->Values)
		{
			SentNames.Add(FString(*Pair.Key));
		}
	}
	TSharedPtr<FJsonObject> WithDefaults = McpCoerceCanonicalVectorShapes(
		McpApplyCanonicalSchemaDefaults(Request.Params, InputSchema), InputSchema);
	OutPlan.Warnings = McpUnreadVariantParams(*Request.Record, SentNames, WithDefaults);

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
			GatewaySchemaGuidance(ParentTool, LegacyAction, Violation.Pointer));
	}

	// Editor-state gate (dogfood #91): a capability whose editorStates exclude
	// "edit" needs a running world; refuse it up front in plain edit mode.
	{
		FString EditorStateMessage;
		if (!McpCheckEditorStateGate(*Request.Record, EditorStateMessage))
		{
			return McpBuildErrorReceipt(Request.CapabilityId,
				McpValidationError(TEXT("EDITOR_STATE_MISMATCH"), EditorStateMessage),
				Context, nullptr);
		}
	}
	// Pre-dispatch policy seam: a client that pinned the catalog revision
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
		const FString Expected = bIsStringPin ? (*PinValue)->AsString() : FString();
		if (!IsCatalogRevisionDigest(Expected))
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

	// The live-state pins are shape-checked here so a malformed envelope
	// is refused before dispatch, but the revision COMPARISON is deliberately not
	// done here — it belongs on the game thread immediately before mutation,
	// because a transport-thread snapshot is already stale by dispatch time.
	FMcpSemanticError RevisionError;
	if (!McpParseExpectedRevisions(Request.Options, OutPlan.ExpectedRevisions, RevisionError))
	{
		return McpBuildErrorReceipt(Request.CapabilityId, RevisionError, Context);
	}

	// An old name dispatches itself; the primary maps its selector to the bridge
	// action the handlers already implement (mirror of resolveDispatchAction).
	// An unmapped selector fails closed rather than dispatching the primary.
	const FString DispatchTarget = McpResolveDispatchAction(*Request.Record, RequestedAction, WithDefaults, LegacyAction);
	if (DispatchTarget.IsEmpty())
	{
		return McpBuildErrorReceipt(Request.CapabilityId,
			McpValidationError(TEXT("INVALID_PARAMETER_VALUE"),
				TEXT("The selector value does not map to an action on this capability.")),
			Context, GatewaySchemaGuidance(ParentTool, LegacyAction, FString()));
	}

	// A grant naming a folded old pair authorizes that pair's operation only;
	// the dispatch target must agree with it (mirror of matchedFoldedGrant).
	// Scoped to consent-bearing policies, so a grant the caller sent for a
	// policy-`none` capability cannot refuse the call that needs no grant.
	FString RecordConsentMode = TEXT("none");
	if (Request.Record->Policy.IsValid())
	{
		Request.Record->Policy->TryGetStringField(TEXT("consent"), RecordConsentMode);
	}
	const TSharedPtr<FJsonObject>* ConsentField = nullptr;
	if (RecordConsentMode != TEXT("none")
		&& GatewayParams->TryGetObjectField(TEXT("consent"), ConsentField) && ConsentField)
	{
		FString GrantedCapability;
		FString GrantedPairName;
		if ((*ConsentField)->TryGetStringField(TEXT("capability"), GrantedCapability)
			&& !McpFoldedGrantMatchesDispatch(*Request.Record, GrantedCapability, DispatchTarget, GrantedPairName))
		{
			return McpBuildErrorReceipt(Request.CapabilityId,
				McpValidationError(TEXT("CONSENT_REQUIRED"),
					FString::Printf(
						TEXT("The consent grant names '%s', which authorizes that operation only; this call dispatches '%s'. Re-run with consent naming the capability id '%s' to authorize the family."),
						*GrantedPairName, *DispatchTarget, *Request.Record->Id)),
				Context, GatewaySchemaGuidance(ParentTool, DispatchTarget, FString()));
		}
	}

	TSharedPtr<FJsonObject> Arguments = MakeShared<FJsonObject>();
	Arguments->Values = WithDefaults->Values;
	Arguments->SetStringField(TEXT("action"), DispatchTarget);

	OutPlan.CapabilityId = Request.CapabilityId;
	OutPlan.ParentTool = ParentTool;
	OutPlan.LegacyAction = DispatchTarget;
	OutPlan.DispatchAction = ParentTool;
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
