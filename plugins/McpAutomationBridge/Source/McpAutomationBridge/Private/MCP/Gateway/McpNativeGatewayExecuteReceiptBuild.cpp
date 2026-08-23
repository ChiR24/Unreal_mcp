// McpNativeGatewayExecuteReceiptBuild.cpp — see header.

#include "MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"

// A gateway execute call answers with a semantic receipt: the handler result
// is checked against the capability output schema first, so a schema violation
// is reported as an error instead of being returned as a success.
TSharedPtr<FJsonObject> McpBuildGatewayExecuteReceipt(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& OutputSchema,
	const FMcpReceiptContext& Context, bool bSuccess, const FString& Message,
	const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
	if (!bSuccess)
	{
		FMcpSemanticError Error;
		if (ErrorCode == TEXT("NOT_CONNECTED"))
		{
			Error = McpDispatchError(ErrorCode, TEXT("NOT_CONNECTED"), Message, true);
		}
		else if (ErrorCode == TEXT("NOT_AVAILABLE") || ErrorCode == TEXT("AUTOMATION_QUEUE_FULL")
			|| ErrorCode == TEXT("INVALID_SESSION"))
		{
			Error = McpDispatchError(ErrorCode, TEXT("DISPATCH_ERROR"), Message, true);
		}
		else
		{
			Error = McpUnrealExecutionError(Message, Result);
			if (!ErrorCode.IsEmpty())
			{
				Error.GatewayCode = ErrorCode;
			}
		}
		return McpBuildErrorReceipt(CapabilityId, Error, Context);
	}

	// Project the handler result to the capability's declared output fields
	// before validating and publishing it, mirroring the TypeScript gateway
	// (projectCanonicalOutput). This keeps undeclared native transport and
	// verification fields (compiled, saved, scsVerification, assetPath, ...) from
	// turning a correct success payload into OUTPUT_SCHEMA_VIOLATION. The native
	// completion carries the success verdict separately from the payload (the
	// WebSocket frame the TS gateway projects embeds it), so it is reunited
	// first; the projected output is what is both validated and published.
	// The 100k budget exists to stop an unbounded listing from flooding a client, and
	// every capability that can page or filter keeps it. A screenshot can do neither: the
	// payload is one indivisible base64 image, so the cap turned a working capture into
	// RESULT_TOO_LARGE with advice ("retry with pagination") that cannot be followed. The
	// image is separately bounded by MaxScreenshotPngBytesForBase64ForMcp, so the raised
	// budget here is not unbounded — it is the one already enforced at the handler.
	// Deliberately narrow: this must not become a general escape hatch.
	const bool bIsImagePayload =
		CapabilityId == TEXT("control_editor.screenshot") ||
		CapabilityId == TEXT("system_control.screenshot");
	const int32 ResultCharBudget = bIsImagePayload ? 6000000 : 100000;

	int64 SerializedChars = 0;
	if (McpSerializedResultExceeds(Result, ResultCharBudget, &SerializedChars))
	{
		FMcpSemanticError TooLarge = McpOutputError(TEXT("RESULT_TOO_LARGE"),
			TEXT("Result exceeded the gateway safety limit. Retry with the action pagination or filtering parameters described by this capability."));
		TooLarge.bHasResultChars = true;
		TooLarge.ResultChars = SerializedChars;
		return McpBuildErrorReceipt(CapabilityId, TooLarge, Context);
	}

	TSharedPtr<FJsonObject> WithVerdict = MakeShared<FJsonObject>();
	if (Result.IsValid())
	{
		WithVerdict->Values = Result->Values;
	}
	if (!WithVerdict->HasField(TEXT("success")))
	{
		WithVerdict->SetBoolField(TEXT("success"), true);
	}
	// `message` is carried beside the payload on a native completion but INSIDE
	// it on the WebSocket frame the TS gateway projects, so reuniting only
	// `success` dropped a field the output schema declares and left the two
	// surfaces publishing different `data` for the same call.
	if (!Message.IsEmpty() && !WithVerdict->HasField(TEXT("message")))
	{
		WithVerdict->SetStringField(TEXT("message"), Message);
	}
	const TSharedPtr<FJsonObject> Canonical =
		McpProjectCanonicalOutput(WithVerdict, OutputSchema);
	const TSharedPtr<FJsonObject> OutputError = ValidateGatewayExecuteOutput(
		CapabilityId, OutputSchema, Canonical, Result, Context);
	return OutputError.IsValid()
		? OutputError
		: McpBuildSuccessReceipt(CapabilityId, Canonical, Context, Result, Message);
}
