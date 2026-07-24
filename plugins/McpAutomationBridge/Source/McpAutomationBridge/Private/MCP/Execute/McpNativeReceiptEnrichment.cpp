// McpNativeReceiptEnrichment.cpp — see header for the parity contract.

#include "MCP/Execute/McpNativeReceiptEnrichment.h"
#include "MCP/Execute/McpNativeReceiptRedaction.h"
#include "MCP/Execute/McpNativeReceiptOutcome.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "HAL/PlatformTime.h"

namespace
{
void SetRecordRevisions(const TSharedPtr<FJsonObject>& Receipt, const FString& CapabilityId)
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

TArray<FString> DeprecationWarnings(const FString& CapabilityId)
{
	TArray<FString> Warnings;
	const FMcpCapabilityRecord* Record = FMcpCanonicalRecordIndex::Get().FindById(CapabilityId);
	if (Record != nullptr && Record->DeprecationStatus == TEXT("deprecated"))
	{
		FString Guidance;
		if (Record->Deprecation.IsValid())
		{
			Record->Deprecation->TryGetStringField(TEXT("guidance"), Guidance);
		}
		Warnings.Add(FString::Printf(TEXT("Capability '%s' is deprecated: %s"), *CapabilityId, *Guidance));
	}
	return Warnings;
}

TSharedPtr<FJsonObject> BuildCanonicalError(const FMcpSemanticError& Error)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("kind"), Error.Kind);
	Object->SetStringField(TEXT("code"), Error.Code);
	Object->SetStringField(TEXT("message"), McpRedactText(Error.Message));
	if (!Error.Pointer.IsEmpty()) Object->SetStringField(TEXT("pointer"), Error.Pointer);
	if (!Error.Option.IsEmpty()) Object->SetStringField(TEXT("option"), Error.Option);
	if (!Error.Field.IsEmpty()) Object->SetStringField(TEXT("field"), Error.Field);
	if (!Error.CurrentRevision.IsEmpty()) Object->SetStringField(TEXT("currentRevision"), Error.CurrentRevision);
	if (!Error.ExpectedRevision.IsEmpty()) Object->SetStringField(TEXT("expectedRevision"), Error.ExpectedRevision);
	if (Error.bHasRetryable) Object->SetBoolField(TEXT("retryable"), Error.bRetryable);
	if (Error.Supported.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Supported;
		for (const FString& Item : Error.Supported)
		{
			Supported.Add(MakeShared<FJsonValueString>(Item));
		}
		Object->SetArrayField(TEXT("supported"), Supported);
	}
	return Object;
}
}  // namespace

FString McpCanonicalizeRequestId(const TSharedPtr<FJsonValue>& Id)
{
	if (!Id.IsValid())
	{
		return FString();
	}
	if (Id->Type == EJson::String)
	{
		FString Text;
		Id->TryGetString(Text);
		return TEXT("str:") + Text;
	}
	if (Id->Type == EJson::Number)
	{
		const double Number = Id->AsNumber();
		if (Number == FMath::TruncToDouble(Number))
		{
			return FString::Printf(TEXT("num:%lld"), static_cast<int64>(Number));
		}
		return FString::Printf(TEXT("num:%g"), Number);
	}
	return FString();
}

TSharedPtr<FJsonObject> McpBuildCanonicalReceipt(
	const FString& CapabilityId, const FMcpReceiptContext& Context,
	bool bSuccess, const FMcpSemanticError* Error,
	const TSharedPtr<FJsonObject>& Data, const TSharedPtr<FJsonObject>& RawResult)
{
	TSharedPtr<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Receipt->SetStringField(TEXT("status"), bSuccess ? TEXT("success") : TEXT("error"));
	Receipt->SetStringField(TEXT("capabilityId"), CapabilityId);
	if (!Context.CorrelationId.IsEmpty()) Receipt->SetStringField(TEXT("correlationId"), Context.CorrelationId);
	if (!Context.RequestId.IsEmpty()) Receipt->SetStringField(TEXT("requestId"), Context.RequestId);
	if (!Context.IdempotencyId.IsEmpty()) Receipt->SetStringField(TEXT("idempotencyId"), Context.IdempotencyId);
	Receipt->SetStringField(TEXT("catalogRevision"), FMcpCanonicalRecordIndex::Get().GetCatalogRevision());
	SetRecordRevisions(Receipt, CapabilityId);
	if (Context.StartTimeSeconds > 0.0)
	{
		const double Ms = (FPlatformTime::Seconds() - Context.StartTimeSeconds) * 1000.0;
		Receipt->SetNumberField(TEXT("timingMs"), Ms > 0.0 ? FMath::FloorToDouble(Ms) : 0.0);
	}
	Receipt->SetArrayField(TEXT("nextCalls"), TArray<TSharedPtr<FJsonValue>>());

	if (bSuccess)
	{
		Receipt->SetArrayField(TEXT("handles"),
			McpBoundJsonArray(McpExtractReceiptHandles(RawResult)));
		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FString& Change : McpExtractReceiptChanges(RawResult))
		{
			Changes.Add(MakeShared<FJsonValueString>(McpRedactText(Change)));
		}
		Receipt->SetArrayField(TEXT("changes"), McpBoundJsonArray(MoveTemp(Changes)));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& Warning : DeprecationWarnings(CapabilityId))
		{
			Warnings.Add(MakeShared<FJsonValueString>(McpRedactText(Warning)));
		}
		Receipt->SetArrayField(TEXT("warnings"), McpBoundJsonArray(MoveTemp(Warnings)));
		TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
		Validation->SetStringField(TEXT("outputSchema"), TEXT("passed"));
		Receipt->SetObjectField(TEXT("validation"), Validation);
		if (const TSharedPtr<FJsonObject> Task = McpExtractReceiptTask(RawResult))
		{
			Receipt->SetObjectField(TEXT("task"), Task);
		}
		if (Data.IsValid())
		{
			McpMaskSecretsDeep(Data);
			Receipt->SetObjectField(TEXT("data"), Data);
		}
		else
		{
			Receipt->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
		}
	}
	else if (Error != nullptr)
	{
		Receipt->SetObjectField(TEXT("error"), BuildCanonicalError(*Error));
	}
	return Receipt;
}
