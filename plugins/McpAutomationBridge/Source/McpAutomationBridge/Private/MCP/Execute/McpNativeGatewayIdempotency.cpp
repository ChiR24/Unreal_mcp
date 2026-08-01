#include "MCP/Execute/McpNativeGatewayReceipt.h"

#include "Containers/StringConv.h"
#include "Dom/JsonValue.h"
#include "Foundation/McpIdempotencyLedger.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "openssl/sha.h"

namespace
{
void AppendCanonical(FString& Out, const TSharedPtr<FJsonValue>& Value);

void AppendCanonicalObject(FString& Out, const TSharedPtr<FJsonObject>& Object)
{
	TArray<FString> Keys;
	Object->Values.GetKeys(Keys);
	// Sorting the keys is what makes the digest order-independent: two payloads
	// that differ only in key order produce the same fingerprint.
	Keys.Sort();
	Out += TEXT("{");
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (Index > 0)
		{
			Out += TEXT(",");
		}
		FString EncodedKey;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> KeyWriter =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&EncodedKey);
		KeyWriter->WriteValue(Keys[Index]);
		KeyWriter->Close();
		Out += EncodedKey;
		Out += TEXT(":");
		AppendCanonical(Out, Object->Values[Keys[Index]]);
	}
	Out += TEXT("}");
}

void AppendCanonical(FString& Out, const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		Out += TEXT("null");
		return;
	}
	switch (Value->Type)
	{
	case EJson::Object:
		AppendCanonicalObject(Out, Value->AsObject());
		return;
	case EJson::Array:
	{
		// Array order carries meaning, so it is preserved, not sorted.
		const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
		Out += TEXT("[");
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Index > 0)
			{
				Out += TEXT(",");
			}
			AppendCanonical(Out, Items[Index]);
		}
		Out += TEXT("]");
		return;
	}
	case EJson::String:
	{
		FString Encoded;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded);
		Writer->WriteValue(Value->AsString());
		Writer->Close();
		Out += Encoded;
		return;
	}
	case EJson::Number:
		Out += FString::SanitizeFloat(Value->AsNumber());
		return;
	case EJson::Boolean:
		Out += Value->AsBool() ? TEXT("true") : TEXT("false");
		return;
	default:
		Out += TEXT("null");
		return;
	}
}
} // namespace

FString McpCanonicalFingerprint(const FString& CapabilityId, const TSharedPtr<FJsonObject>& Params)
{
	FString Canonical = CapabilityId + TEXT(" ");
	if (Params.IsValid())
	{
		AppendCanonicalObject(Canonical, Params);
	}
	else
	{
		Canonical += TEXT("{}");
	}

	const FTCHARToUTF8 Utf8(*Canonical);
	unsigned char Hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()), Hash);
	FString Digest;
	Digest.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (int32 Index = 0; Index < SHA256_DIGEST_LENGTH; ++Index)
	{
		Digest += FString::Printf(TEXT("%02x"), Hash[Index]);
	}
	return Digest;
}

bool McpValidateIdempotencyKeyOption(
	const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
	if (!Options.IsValid())
	{
		return true;
	}
	const TSharedPtr<FJsonValue> Key = Options->TryGetField(TEXT("idempotencyKey"));
	if (!Key.IsValid())
	{
		return true;
	}
	FString Value;
	const bool bBoundedString = Key->Type == EJson::String && Key->TryGetString(Value) &&
		Value.Len() >= 1 && Value.Len() <= 128;
	if (bBoundedString)
	{
		return true;
	}
	OutError = McpValidationError(TEXT("INVALID_OPTIONS"),
		TEXT("options.idempotencyKey must be a string of 1..128 characters."),
		TEXT("/options/idempotencyKey"));
	OutError.Option = TEXT("idempotencyKey");
	return false;
}

void McpSettleIdempotency(const FString& Slot, bool bSuccess, const TSharedPtr<FJsonObject>& Receipt)
{
	if (Slot.IsEmpty())
	{
		return;
	}
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	if (!bSuccess || !Receipt.IsValid())
	{
		Ledger.Abandon(Slot);
		return;
	}
	FString ReceiptJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ReceiptJson);
	FJsonSerializer::Serialize(Receipt.ToSharedRef(), Writer);
	Ledger.Complete(Slot, ReceiptJson);
}
