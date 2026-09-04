// McpNativeGatewayReceipt.cpp — see header for the envelope contract.

#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeReceiptEnrichment.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"

namespace
{
FMcpSemanticError McpGatewayMakeSemanticError(
	const TCHAR* Kind, const TCHAR* Code, const FString& GatewayCode, const FString& Message)
{
	FMcpSemanticError Error;
	Error.Kind = Kind;
	Error.Code = Code;
	Error.GatewayCode = GatewayCode;
	Error.Message = Message;
	return Error;
}

void SetIfPresent(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(Field, Value);
	}
}
}

FMcpSemanticError McpValidationError(
	const FString& GatewayCode, const FString& Message, const FString& Pointer)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("validation"), TEXT("VALIDATION_ERROR"), GatewayCode, Message);
	Error.Pointer = Pointer;
	return Error;
}

FMcpSemanticError McpOptionError(
	const FString& Option, const TArray<FString>& Supported, const FString& Message)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(
		TEXT("option"), TEXT("UNSUPPORTED_OPTION"), TEXT("UNSUPPORTED_OPTION"), Message);
	Error.Option = Option;
	Error.Supported = Supported;
	return Error;
}

FMcpSemanticError McpRangeError(const FString& Field, const FString& Message)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("range"), TEXT("OUT_OF_RANGE"), TEXT("OUT_OF_RANGE"), Message);
	Error.Field = Field;
	return Error;
}

FMcpSemanticError McpExecutionError(
	const FString& GatewayCode, const FString& Message, bool bRetryable)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("execution"), TEXT("EXECUTION_ERROR"), GatewayCode, Message);
	Error.bHasRetryable = true;
	Error.bRetryable = bRetryable;
	return Error;
}

FMcpSemanticError McpCapabilityError(
	const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("capability"), *Code, GatewayCode, Message);
	Error.bHasRetryable = true;
	Error.bRetryable = bRetryable;
	return Error;
}

FMcpSemanticError McpDispatchError(
	const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("dispatch"), *Code, GatewayCode, Message);
	Error.bHasRetryable = true;
	Error.bRetryable = bRetryable;
	return Error;
}

FMcpSemanticError McpOutputError(const FString& Code, const FString& Message, const FString& Pointer)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("output"), *Code, Code, Message);
	Error.Pointer = Pointer;
	return Error;
}

FMcpSemanticError McpStaleStateError(
	const FString& Message, const FString& CurrentRevision, const FString& ExpectedRevision)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(TEXT("staleState"), TEXT("STALE_STATE"), TEXT("STALE_STATE"), Message);
	Error.CurrentRevision = CurrentRevision;
	Error.ExpectedRevision = ExpectedRevision;
	return Error;
}

FMcpSemanticError McpUnrealExecutionError(
	const FString& Message, const TSharedPtr<FJsonObject>& UnrealDetail)
{
	FMcpSemanticError Error = McpGatewayMakeSemanticError(
		TEXT("execution"), TEXT("UNREAL_ENGINE_ERROR"), TEXT("UNREAL_EXECUTION_ERROR"), Message);
	Error.UnrealDetail = UnrealDetail;
	return Error;
}

namespace
{
// The capability and schema revisions come straight from the resolved record's
// content/schema hashes, the same runtime sources the TypeScript receipt reads,
// so all three revision strings stay distinct and truthful across transports.
void SetRevisionsForCapability(const TSharedPtr<FJsonObject>& Receipt, const FString& CapabilityId)
{
	if (CapabilityId.IsEmpty())
	{
		return;
	}
	const FMcpCapabilityRecord* Record = FMcpCanonicalRecordIndex::Get().FindById(CapabilityId);
	if (Record == nullptr || !Record->Hashes.IsValid())
	{
		return;
	}
	FString Content;
	if (Record->Hashes->TryGetStringField(TEXT("content"), Content))
	{
		Receipt->SetStringField(TEXT("capabilityRevision"), Content);
	}
	FString Schema;
	if (Record->Hashes->TryGetStringField(TEXT("schema"), Schema))
	{
		Receipt->SetStringField(TEXT("schemaRevision"), Schema);
	}
}

TSharedPtr<FJsonObject> BuildReceiptShell(const FString& CapabilityId, const FString& CorrelationId)
{
	TSharedPtr<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Receipt->SetStringField(TEXT("capabilityId"), CapabilityId);
	// capabilityId is the catalog record id (asset.rename); name the parent tool and public
	// action beside it so callers do not have to decode the namespace (dogfood #12).
	if (const FMcpCapabilityRecord* Record = FMcpCanonicalRecordIndex::Get().FindById(CapabilityId))
	{
		Receipt->SetStringField(TEXT("tool"), Record->Parent);
		Receipt->SetStringField(TEXT("action"), McpCapabilityPublicAction(*Record));
	}
	Receipt->SetStringField(
		TEXT("catalogRevision"), FMcpCanonicalRecordIndex::Get().GetCatalogRevision());
	SetRevisionsForCapability(Receipt, CapabilityId);
	SetIfPresent(Receipt, TEXT("correlationId"), CorrelationId);
	return Receipt;
}
}

TSharedPtr<FJsonObject> McpBuildErrorReceipt(
	const FString& CapabilityId, const FMcpSemanticError& Error,
	const FMcpReceiptContext& Context, const TSharedPtr<FJsonObject>& Guidance)
{
	TSharedPtr<FJsonObject> Receipt = BuildReceiptShell(CapabilityId, Context.CorrelationId);
	Receipt->SetStringField(TEXT("status"), TEXT("error"));
	const TSharedRef<FJsonObject> LiveRevisions = FMcpLiveStateRevisions::Get().Snapshot().ToJson();
	Receipt->SetObjectField(TEXT("liveRevisions"), LiveRevisions);

	// `success` and `message` are retained so pre-Task-27 gateway clients keep
	// reading the same fields they already branch on.
	Receipt->SetBoolField(TEXT("success"), false);
	Receipt->SetStringField(TEXT("operation"), TEXT("execute"));
	const FString SafeMessage = McpMaskSecrets(Error.Message);
	Receipt->SetStringField(TEXT("message"), SafeMessage);
	Receipt->SetStringField(TEXT("error"), SafeMessage);
	SetIfPresent(Receipt, TEXT("errorCode"), Error.GatewayCode);

	TSharedPtr<FJsonObject> TypedError = MakeShared<FJsonObject>();
	TypedError->SetStringField(TEXT("kind"), Error.Kind);
	TypedError->SetStringField(TEXT("code"), Error.Code);
	TypedError->SetStringField(TEXT("message"), SafeMessage);
	SetIfPresent(TypedError, TEXT("pointer"), Error.Pointer);
	SetIfPresent(TypedError, TEXT("option"), Error.Option);
	SetIfPresent(TypedError, TEXT("field"), Error.Field);
	SetIfPresent(TypedError, TEXT("currentRevision"), Error.CurrentRevision);
	SetIfPresent(TypedError, TEXT("expectedRevision"), Error.ExpectedRevision);
	SetIfPresent(TypedError, TEXT("requiredScope"), Error.RequiredScope);
	SetIfPresent(TypedError, TEXT("scope"), Error.ConsentScope);
	// grantedScopes is REQUIRED (not optional) on the TypeScript authorization
	// variant, so it is emitted for that kind even when the principal holds none.
	if (Error.Kind == TEXT("authorization"))
	{
		TypedError->SetArrayField(TEXT("grantedScopes"), GatewayStringArray(Error.GrantedScopes));
	}
	if (Error.bHasRetryable)
	{
		TypedError->SetBoolField(TEXT("retryable"), Error.bRetryable);
	}
	if (Error.Supported.Num() > 0)
	{
		TypedError->SetArrayField(TEXT("supported"), GatewayStringArray(Error.Supported));
	}
	if (Error.bHasResultChars)
	{
		TypedError->SetNumberField(TEXT("resultChars"), static_cast<double>(Error.ResultChars));
		Receipt->SetNumberField(TEXT("resultChars"), static_cast<double>(Error.ResultChars));
	}
	if (Error.UnrealDetail.IsValid())
	{
		McpMaskSecretsDeep(Error.UnrealDetail);
		TypedError->SetObjectField(TEXT("unrealDetail"), Error.UnrealDetail);
	}
	Receipt->SetObjectField(TEXT("typedError"), TypedError);

	// The nested canonical receipt names a capability; when none resolved (an
	// unknown-capability guided error) it is omitted, matching the TS gateway
	// which builds no receipt for an unresolved request.
	if (!CapabilityId.IsEmpty())
	{
		TSharedPtr<FJsonObject> Canonical =
			McpBuildCanonicalReceipt(CapabilityId, Context, false, &Error, nullptr, nullptr);
		Canonical->SetObjectField(TEXT("liveRevisions"), LiveRevisions);
		Receipt->SetObjectField(TEXT("receipt"), Canonical);
	}

	if (Guidance.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Guidance->Values)
		{
			if (!Receipt->HasField(Field.Key))
			{
				Receipt->SetField(Field.Key, Field.Value);
			}
		}
	}
	return Receipt;
}

TSharedPtr<FJsonObject> McpBuildSuccessReceipt(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& Data,
	const FMcpReceiptContext& Context, const TSharedPtr<FJsonObject>& RawResult,
	const FString& Message)
{
	TSharedPtr<FJsonObject> Receipt = BuildReceiptShell(CapabilityId, Context.CorrelationId);
	Receipt->SetStringField(TEXT("status"), TEXT("success"));
	const TSharedRef<FJsonObject> LiveRevisions = FMcpLiveStateRevisions::Get().Snapshot().ToJson();
	Receipt->SetObjectField(TEXT("liveRevisions"), LiveRevisions);
	Receipt->SetBoolField(TEXT("success"), true);
	Receipt->SetStringField(TEXT("operation"), TEXT("execute"));
	SetIfPresent(Receipt, TEXT("message"), Message);
	TSharedPtr<FJsonObject> Canonical =
		McpBuildCanonicalReceipt(CapabilityId, Context, true, nullptr, Data, RawResult);
	Canonical->SetObjectField(TEXT("liveRevisions"), LiveRevisions);
	Receipt->SetObjectField(TEXT("receipt"), Canonical);
	if (Data.IsValid())
	{
		Receipt->SetObjectField(TEXT("data"), Data);
	}
	return Receipt;
}

FString McpReceiptMessage(const TSharedPtr<FJsonObject>& Receipt)
{
	if (!Receipt.IsValid())
	{
		return TEXT("execute failed");
	}
	FString Message;
	if (Receipt->TryGetStringField(TEXT("message"), Message) && !Message.IsEmpty())
	{
		return Message;
	}
	return McpReceiptSucceeded(Receipt) ? TEXT("ok") : TEXT("execute failed");
}

bool McpReceiptSucceeded(const TSharedPtr<FJsonObject>& Receipt)
{
	FString Status;
	return Receipt.IsValid() && Receipt->TryGetStringField(TEXT("status"), Status) &&
		Status == TEXT("success");
}
