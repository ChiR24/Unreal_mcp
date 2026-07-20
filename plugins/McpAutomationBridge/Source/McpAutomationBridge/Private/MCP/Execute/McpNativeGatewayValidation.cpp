// McpNativeGatewayValidation.cpp — see header for the normative stage order.

#include "MCP/Execute/McpNativeGatewayValidation.h"
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
	const FString& CorrelationId, FMcpGatewayExecutePlan& OutPlan)
{
	if (!GatewayParams.IsValid())
	{
		return McpBuildErrorReceipt(FString(),
			McpValidationError(TEXT("INVALID_PARAMS"), TEXT("execute requires an arguments object.")),
			CorrelationId);
	}

	FMcpGatewayExecuteRequest Request;
	FMcpSemanticError Error;
	TSharedPtr<FJsonObject> Guidance;
	if (!McpParseGatewayExecuteRequest(GatewayParams, Request, Error, Guidance))
	{
		return McpBuildErrorReceipt(FString(), Error, CorrelationId, Guidance);
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
			CorrelationId);
	}

	if (!ToolManager.IsToolEnabled(ParentTool))
	{
		return McpBuildErrorReceipt(Request.CapabilityId,
			McpExecutionError(TEXT("TOOL_DISABLED"),
				FString::Printf(TEXT("Capability '%s' is disabled or unavailable."), *Request.CapabilityId),
				false),
			CorrelationId, DisabledCapabilityGuidance(ParentTool));
	}

	// Canonical per-action schemas mostly omit `action` (the action IS the
	// capability), so it is injected for validation only where declared.
	const TSharedPtr<FJsonObject> InputSchema = Request.Record->InputSchema;
	TSharedPtr<FJsonObject> WithDefaults =
		McpApplyCanonicalSchemaDefaults(Request.Params, InputSchema);

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
		return McpBuildErrorReceipt(Request.CapabilityId, SchemaError, CorrelationId,
			SchemaGuidance(ParentTool, LegacyAction, Violation.Pointer));
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

TSharedPtr<FJsonObject> ValidateGatewayExecuteOutput(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& OutputSchema,
	const TSharedPtr<FJsonObject>& Result, const FString& CorrelationId)
{
	if (!OutputSchema.IsValid())
	{
		return nullptr;
	}

	FMcpSchemaViolationDetail Violation;
	if (McpValidateObjectAgainstCanonicalSchema(Result, OutputSchema, Violation))
	{
		return nullptr;
	}

	FMcpSemanticError Error = McpValidationError(TEXT("OUTPUT_SCHEMA_VIOLATION"),
		FString::Printf(TEXT("Capability '%s' returned a result its output schema refuses: %s"),
			*CapabilityId, *Violation.Message),
		Violation.Pointer);
	// The handler payload is retained verbatim so a schema violation never
	// discards the structured detail Unreal actually reported.
	Error.UnrealDetail = Result;
	return McpBuildErrorReceipt(CapabilityId, Error, CorrelationId);
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
