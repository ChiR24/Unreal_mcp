#include "MCP/Execute/McpNativeReceiptRedaction.h"

#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
// The receipt's array and free-text bounds.
constexpr int32 McpMaxReceiptArray = 200;
constexpr int32 McpMaxReceiptText = 2048;
}  // namespace

FString McpRedactText(const FString& Value)
{
	FString Masked = McpMaskSecrets(Value);
	if (Masked.Len() > McpMaxReceiptText)
	{
		Masked = Masked.Left(McpMaxReceiptText - 1);
		Masked.AppendChar(static_cast<TCHAR>(0x2026));
	}
	return Masked;
}

TArray<TSharedPtr<FJsonValue>> McpBoundJsonArray(TArray<TSharedPtr<FJsonValue>> Values)
{
	if (Values.Num() > McpMaxReceiptArray)
	{
		Values.SetNum(McpMaxReceiptArray);
	}
	return Values;
}

bool McpSerializedResultExceeds(
	const TSharedPtr<FJsonObject>& Result, int32 MaxChars, int64* OutSerializedChars)
{
	if (!Result.IsValid())
	{
		return false;
	}
	FString Serialized;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialized);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	if (OutSerializedChars != nullptr)
	{
		*OutSerializedChars = static_cast<int64>(Serialized.Len());
	}
	return Serialized.Len() > MaxChars;
}
