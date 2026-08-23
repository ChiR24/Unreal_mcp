#include "MCP/Protocol/McpJsonRpcImageContent.h"

namespace
{
// Mirror of MAX_RECEIPT_DEPTH in receipt-redaction.ts: a size gate cannot
// substitute for a depth gate, so a tree deeper than this is not descended (the
// subtree is preserved verbatim rather than walked to a stack overflow).
constexpr int32 McpMaxImageDepth = 100;

/** Deep search for the object actually holding imageBase64.
 *
 *  A direct tool call puts it at the top level, but a gateway execute wraps the handler
 *  payload in a receipt, so it lands at data/imageBase64 and is mirrored again at
 *  receipt/data/imageBase64. Matching only the top level meant an execute receipt produced
 *  NO image content block at all and shipped the base64 as raw text instead — twice.
 */
const TSharedPtr<FJsonObject>* FindImageCarrier(const TSharedPtr<FJsonObject>& Data, int32 Depth)
{
	if (!Data.IsValid() || Depth > McpMaxImageDepth)
	{
		return nullptr;
	}
	FString Probe;
	if (Data->TryGetStringField(TEXT("imageBase64"), Probe) && !Probe.IsEmpty())
	{
		return &Data;
	}
	for (const auto& Field : Data->Values)
	{
		if (!Field.Value.IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Field.Value->TryGetObject(Nested) && Nested)
		{
			if (const TSharedPtr<FJsonObject>* Found = FindImageCarrier(*Nested, Depth + 1))
			{
				return Found;
			}
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Field.Value->TryGetArray(Array) || !Array)
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& Element : *Array)
		{
			const TSharedPtr<FJsonObject>* ElementObject = nullptr;
			if (!Element.IsValid() || !Element->TryGetObject(ElementObject) || !ElementObject)
			{
				continue;
			}
			if (const TSharedPtr<FJsonObject>* Found = FindImageCarrier(*ElementObject, Depth + 1))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

/** Copy of Data with every imageBase64 replaced by a placeholder, at any depth. */
TSharedPtr<FJsonObject> StripImagePayload(const TSharedPtr<FJsonObject>& Data, int32 Depth)
{
	if (!Data.IsValid() || Depth > McpMaxImageDepth)
	{
		return Data;
	}
	TSharedPtr<FJsonObject> Stripped = MakeShared<FJsonObject>();
	for (const auto& Field : Data->Values)
	{
		// Bound by value, not by reference: UE 5.8 changed FJsonObject::Values so
		// the key is no longer an FString, and a const FString& cannot bind to
		// the temporary that conversion produces. Copying compiles on both.
		const FString FieldName(Field.Key);
		if (FieldName == TEXT("imageBase64"))
		{
			Stripped->SetStringField(FieldName, TEXT("<omitted; see image content>"));
			continue;
		}
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Field.Value.IsValid() && Field.Value->TryGetObject(Nested) && Nested)
		{
			Stripped->SetObjectField(FieldName, StripImagePayload(*Nested, Depth + 1));
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Field.Value.IsValid() && Field.Value->TryGetArray(Array) && Array)
		{
			TArray<TSharedPtr<FJsonValue>> StrippedArray;
			for (const TSharedPtr<FJsonValue>& Element : *Array)
			{
				const TSharedPtr<FJsonObject>* ElementObject = nullptr;
				if (Element.IsValid() && Element->TryGetObject(ElementObject) && ElementObject)
				{
					StrippedArray.Add(
						MakeShared<FJsonValueObject>(StripImagePayload(*ElementObject, Depth + 1)));
				}
				else
				{
					StrippedArray.Add(Element);
				}
			}
			Stripped->SetArrayField(FieldName, StrippedArray);
			continue;
		}
		Stripped->SetField(FieldName, Field.Value);
	}
	return Stripped;
}
}

namespace McpJsonRpcImage
{
TSharedPtr<FJsonObject> MakeToolTextData(const TSharedPtr<FJsonObject>& Data)
{
	// Untouched when there is no image, so the common path copies nothing.
	if (FindImageCarrier(Data, 0) == nullptr)
	{
		return Data;
	}
	return StripImagePayload(Data, 0);
}

void AddImageContentIfPresent(const TSharedPtr<FJsonObject>& Data,
	TArray<TSharedPtr<FJsonValue>>& Content)
{
	const TSharedPtr<FJsonObject>* Carrier = FindImageCarrier(Data, 0);
	if (Carrier == nullptr)
	{
		return;
	}

	FString ImageBase64;
	(*Carrier)->TryGetStringField(TEXT("imageBase64"), ImageBase64);

	FString MimeType;
	(*Carrier)->TryGetStringField(TEXT("mimeType"), MimeType);
	if (MimeType.IsEmpty())
	{
		MimeType = TEXT("image/png");
	}

	TSharedPtr<FJsonObject> ImageContent = MakeShared<FJsonObject>();
	ImageContent->SetStringField(TEXT("type"), TEXT("image"));
	ImageContent->SetStringField(TEXT("data"), ImageBase64);
	ImageContent->SetStringField(TEXT("mimeType"), MimeType);
	Content.Add(MakeShared<FJsonValueObject>(ImageContent));
}
}
