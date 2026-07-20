// McpNativeGatewayReceipt.cpp — see header for the envelope contract.

#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
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
	Error.Field = bRetryable ? TEXT("retryable") : FString();
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
TSharedPtr<FJsonObject> BuildReceiptShell(const FString& CapabilityId, const FString& CorrelationId)
{
	TSharedPtr<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Receipt->SetStringField(TEXT("capabilityId"), CapabilityId);
	Receipt->SetStringField(
		TEXT("catalogRevision"), FMcpCanonicalRecordIndex::Get().GetCatalogRevision());
	SetIfPresent(Receipt, TEXT("correlationId"), CorrelationId);
	return Receipt;
}
}

TSharedPtr<FJsonObject> McpBuildErrorReceipt(
	const FString& CapabilityId, const FMcpSemanticError& Error,
	const FString& CorrelationId, const TSharedPtr<FJsonObject>& Guidance)
{
	TSharedPtr<FJsonObject> Receipt = BuildReceiptShell(CapabilityId, CorrelationId);
	Receipt->SetStringField(TEXT("status"), TEXT("error"));

	// `success` and `message` are retained so pre-Task-27 gateway clients keep
	// reading the same fields they already branch on.
	Receipt->SetBoolField(TEXT("success"), false);
	Receipt->SetStringField(TEXT("operation"), TEXT("execute"));
	Receipt->SetStringField(TEXT("message"), Error.Message);
	Receipt->SetStringField(TEXT("error"), Error.Message);
	SetIfPresent(Receipt, TEXT("errorCode"), Error.GatewayCode);

	TSharedPtr<FJsonObject> TypedError = MakeShared<FJsonObject>();
	TypedError->SetStringField(TEXT("kind"), Error.Kind);
	TypedError->SetStringField(TEXT("code"), Error.Code);
	TypedError->SetStringField(TEXT("message"), Error.Message);
	SetIfPresent(TypedError, TEXT("pointer"), Error.Pointer);
	SetIfPresent(TypedError, TEXT("option"), Error.Option);
	SetIfPresent(TypedError, TEXT("field"), Error.Field);
	if (Error.Supported.Num() > 0)
	{
		TypedError->SetArrayField(TEXT("supported"), GatewayStringArray(Error.Supported));
	}
	if (Error.UnrealDetail.IsValid())
	{
		TypedError->SetObjectField(TEXT("unrealDetail"), Error.UnrealDetail);
	}
	Receipt->SetObjectField(TEXT("typedError"), TypedError);

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
	const FString& CorrelationId, const FString& Message)
{
	TSharedPtr<FJsonObject> Receipt = BuildReceiptShell(CapabilityId, CorrelationId);
	Receipt->SetStringField(TEXT("status"), TEXT("success"));
	Receipt->SetBoolField(TEXT("success"), true);
	Receipt->SetStringField(TEXT("operation"), TEXT("execute"));
	SetIfPresent(Receipt, TEXT("message"), Message);
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
