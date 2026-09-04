// McpNativeGatewayOutputProjection.cpp — projects a handler result onto the
// capability output contract: declared properties, with undeclared handler
// fields folded into a declared `details` boundary object.

#include "MCP/Execute/McpNativeGatewayValidation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedPtr<FJsonObject> McpProjectCanonicalOutput(
	const TSharedPtr<FJsonObject>& Result, const TSharedPtr<FJsonObject>& OutputSchema)
{
	if (!Result.IsValid())
	{
		return Result;
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!OutputSchema.IsValid() ||
		!OutputSchema->TryGetObjectField(TEXT("properties"), Properties) || !Properties)
	{
		return MakeShared<FJsonObject>();
	}
	const TSharedPtr<FJsonObject>* Payload = nullptr;
	const bool bHasPayload = Result->TryGetObjectField(TEXT("data"), Payload) && Payload;
	TSharedPtr<FJsonObject> Projected = MakeShared<FJsonObject>();
	for (const auto& Property : (*Properties)->Values)
	{
		if (const TSharedPtr<FJsonValue>* Field = Result->Values.Find(Property.Key))
		{
			Projected->Values.Add(Property.Key, *Field);
		}
		else if (bHasPayload)
		{
			if (const TSharedPtr<FJsonValue>* PayloadField = (*Payload)->Values.Find(Property.Key))
			{
				Projected->Values.Add(Property.Key, *PayloadField);
			}
		}
	}
	// Handlers publish rich payloads at the top level while many records declare
	// only {success, details}. Fold every undeclared handler field into the
	// declared `details` reflection-boundary object so the data survives a
	// closed contract (struct/enum/datatable reads used to return bare success).
	if ((*Properties)->HasField(TEXT("details")))
	{
		TSharedPtr<FJsonObject> Details;
		if (const TSharedPtr<FJsonValue>* Existing = Projected->Values.Find(TEXT("details")))
		{
			const TSharedPtr<FJsonObject>* ExistingObject = nullptr;
			if ((*Existing)->TryGetObject(ExistingObject) && ExistingObject)
			{
				Details = *ExistingObject;
			}
		}
		auto Fold = [&Properties, &Details](const TSharedPtr<FJsonObject>& Source)
		{
			for (const auto& Entry : Source->Values)
			{
				if (Entry.Key == TEXT("data") || Entry.Key == TEXT("requestId") || Entry.Key == TEXT("type") ||
					(*Properties)->HasField(Entry.Key))
				{
					continue;
				}
				if (!Details.IsValid())
				{
					Details = MakeShared<FJsonObject>();
				}
				if (!Details->HasField(Entry.Key))
				{
					Details->Values.Add(Entry.Key, Entry.Value);
				}
			}
		};
		Fold(Result);
		if (bHasPayload)
		{
			Fold(*Payload);
		}
		if (Details.IsValid())
		{
			Projected->SetObjectField(TEXT("details"), Details);
		}
	}
	return Projected;
}
