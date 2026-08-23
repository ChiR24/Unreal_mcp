// McpNativeReceiptRedaction.cpp — see header for the parity contract.

#include "MCP/Execute/McpNativeReceiptRedaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace
{
// Mirror of MAX_RECEIPT_ARRAY / MAX_RECEIPT_TEXT in receipt-redaction.ts.
constexpr int32 McpMaxReceiptArray = 200;
constexpr int32 McpMaxReceiptText = 2048;
// Mirror of MAX_RECEIPT_DEPTH. The size gate cannot substitute for a depth gate:
// thousands of nesting levels are only a few KB, so the stack overflows before
// any size check runs. A subtree too deep to inspect is MASKED, not returned.
constexpr int32 McpMaxReceiptDepth = 100;

TSharedPtr<FJsonValue> MaskValue(const TSharedPtr<FJsonValue>& Value, int32 Depth);

void MaskSecretsDeepInternal(const TSharedPtr<FJsonObject>& Object, int32 Depth);

// Skip a single optional surrounding quote so JSON-like `"token":"x"` is masked.
void SkipOptionalQuote(const FString& Text, int32& Cursor)
{
	if (Cursor < Text.Len() && (Text[Cursor] == '"' || Text[Cursor] == '\''))
	{
		++Cursor;
	}
}

// Skip an optional `Bearer ` auth scheme so `Authorization: Bearer <token>` masks
// the TOKEN, not the scheme word (the earlier form left the credential exposed).
void SkipOptionalBearerScheme(const FString& Text, int32& Cursor)
{
	static const FString Scheme(TEXT("Bearer"));
	if (!Text.Mid(Cursor, Scheme.Len()).Equals(Scheme, ESearchCase::IgnoreCase))
	{
		return;
	}
	const int32 AfterScheme = Cursor + Scheme.Len();
	if (AfterScheme < Text.Len() && FChar::IsWhitespace(Text[AfterScheme]))
	{
		Cursor = AfterScheme;
		while (Cursor < Text.Len() && FChar::IsWhitespace(Text[Cursor]))
		{
			++Cursor;
		}
	}
}

// Mask the value token following `Marker`, mirroring receipt-redaction.ts. In
// assignment mode the marker may be followed by an optional quote, then a ':'/'='
// separator, then an optional quote and an optional `Bearer ` scheme, then the
// value. In Bearer mode the marker must sit at a word boundary and be followed by
// whitespace. The value run stops at whitespace OR a quote so JSON stays intact
// and the trailing quote survives; the run is replaced by [REDACTED].
void MaskAfterMarker(FString& Text, const FString& Marker, bool bAssignment)
{
	int32 Search = 0;
	while (Search < Text.Len())
	{
		const int32 Found = Text.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart, Search);
		if (Found == INDEX_NONE)
		{
			return;
		}
		int32 Cursor = Found + Marker.Len();
		bool bMatched = false;
		if (bAssignment)
		{
			SkipOptionalQuote(Text, Cursor);
			while (Cursor < Text.Len() && (Text[Cursor] == ' ' || Text[Cursor] == '\t'))
			{
				++Cursor;
			}
			if (Cursor < Text.Len() && (Text[Cursor] == ':' || Text[Cursor] == '='))
			{
				++Cursor;
				bMatched = true;
			}
		}
		else
		{
			const bool bBoundary = (Found == 0) || !FChar::IsAlnum(Text[Found - 1]);
			bMatched = bBoundary && Cursor < Text.Len() && FChar::IsWhitespace(Text[Cursor]);
		}
		if (bMatched)
		{
			while (Cursor < Text.Len() && FChar::IsWhitespace(Text[Cursor]))
			{
				++Cursor;
			}
			if (bAssignment)
			{
				SkipOptionalQuote(Text, Cursor);
				SkipOptionalBearerScheme(Text, Cursor);
			}
			const int32 ValueStart = Cursor;
			while (Cursor < Text.Len() && !FChar::IsWhitespace(Text[Cursor])
				&& Text[Cursor] != '"' && Text[Cursor] != '\'')
			{
				++Cursor;
			}
			if (Cursor > ValueStart)
			{
				Text = Text.Left(ValueStart) + TEXT("[REDACTED]") + Text.Mid(Cursor);
				Search = ValueStart + 10;
				continue;
			}
		}
		Search = Found + Marker.Len();
	}
}

TSharedPtr<FJsonValue> MaskValue(const TSharedPtr<FJsonValue>& Value, int32 Depth)
{
	if (!Value.IsValid())
	{
		return Value;
	}
	if (Value->Type == EJson::String)
	{
		FString Text;
		Value->TryGetString(Text);
		return MakeShared<FJsonValueString>(McpMaskSecrets(Text));
	}
	if (Value->Type == EJson::Object)
	{
		if (Depth >= McpMaxReceiptDepth)
		{
			return MakeShared<FJsonValueString>(TEXT("[REDACTED]"));
		}
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Value->TryGetObject(Object) && Object)
		{
			MaskSecretsDeepInternal(*Object, Depth + 1);
		}
		return Value;
	}
	if (Value->Type == EJson::Array)
	{
		if (Depth >= McpMaxReceiptDepth)
		{
			return MakeShared<FJsonValueString>(TEXT("[REDACTED]"));
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Value->TryGetArray(Array) && Array)
		{
			TArray<TSharedPtr<FJsonValue>> Masked;
			for (const TSharedPtr<FJsonValue>& Element : *Array)
			{
				Masked.Add(MaskValue(Element, Depth + 1));
			}
			return MakeShared<FJsonValueArray>(Masked);
		}
	}
	return Value;
}

void MaskSecretsDeepInternal(const TSharedPtr<FJsonObject>& Object, int32 Depth)
{
	if (!Object.IsValid())
	{
		return;
	}
	// A reflection reply names the property it read in one field and carries the
	// value in a generic sibling: `{propertyName:"CapabilityToken", value:"<token>"}`.
	// Key-name classification is blind to that — the key holding the secret is
	// `value`, which names nothing — so the NAME-BEARING sibling is consulted once
	// per object and, when it names a credential, the generic carriers are masked
	// alongside the secret-named keys.
	const bool bSiblingNamesCredential = McpNamesCredentialBySibling(Object);
	for (auto& Pair : Object->Values)
	{
		// A secret-named KEY masks its ENTIRE value whatever the value's shape:
		// an object, an array and a number can each carry a credential just as
		// well as a string, and recursing would only find leaves that no longer
		// carry the keyword context the string masker needs.
		const FString Key(*Pair.Key);
		const bool bGenericSiblingCarrier =
			bSiblingNamesCredential && McpIsGenericValueKey(Key);
		// A boolean or number cannot carry a credential, so the sibling path spares
		// them (a secret-NAMED key still masks its whole value whatever the shape).
		const bool bMasked = McpIsSecretKey(Key)
			|| (bGenericSiblingCarrier && Pair.Value.IsValid()
				&& Pair.Value->Type != EJson::Boolean && Pair.Value->Type != EJson::Number);
		if (bMasked)
		{
			Pair.Value = MakeShared<FJsonValueString>(TEXT("[REDACTED]"));
			continue;
		}
		Pair.Value = MaskValue(Pair.Value, Depth);
	}
}
}  // namespace

FString McpMaskSecrets(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return Value;
	}
	FString Text = Value;
	static const TArray<FString> Keywords = {
		TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"), TEXT("pwd"),
		TEXT("api_key"), TEXT("api-key"), TEXT("apikey"), TEXT("authorization")};
	for (const FString& Keyword : Keywords)
	{
		MaskAfterMarker(Text, Keyword, true);
	}
	MaskAfterMarker(Text, TEXT("Bearer"), false);
	return Text;
}

void McpMaskSecretsDeep(const TSharedPtr<FJsonObject>& Object)
{
	MaskSecretsDeepInternal(Object, 0);
}

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
