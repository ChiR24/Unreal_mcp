// McpNativeGatewaySchemaValidation.cpp — see header for the fail-closed contract.

#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"
#include "MCP/Execute/McpNativeGatewaySchemaKeywords.h"

namespace
{

bool ValidateObjectBody(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation);
}

const TCHAR* McpSchemaViolationCode(EMcpSchemaViolation Reason)
{
	switch (Reason)
	{
	case EMcpSchemaViolation::MissingRequired: return TEXT("MISSING_REQUIRED_PARAMETER");
	case EMcpSchemaViolation::Undeclared: return TEXT("UNDECLARED_PARAMETER");
	case EMcpSchemaViolation::Type: return TEXT("INVALID_PARAMETER_TYPE");
	case EMcpSchemaViolation::Enum: return TEXT("INVALID_PARAMETER_VALUE");
	case EMcpSchemaViolation::Range: return TEXT("OUT_OF_RANGE");
	case EMcpSchemaViolation::UnsupportedKeyword: return TEXT("UNSUPPORTED_SCHEMA_KEYWORD");
	default: return TEXT("VALIDATION_ERROR");
	}
}

const TCHAR* McpSchemaViolationKind(EMcpSchemaViolation Reason)
{
	return Reason == EMcpSchemaViolation::Range ? TEXT("range") : TEXT("validation");
}

bool McpSchemaDeclaresProperty(const TSharedPtr<FJsonObject>& Schema, const TCHAR* PropertyName)
{
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	return Schema.IsValid() && Schema->TryGetObjectField(TEXT("properties"), Properties) &&
		Properties && (*Properties)->HasField(PropertyName);
}

bool McpValidateAgainstCanonicalSchema(
	const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation)
{
	if (!Schema.IsValid() || !Value.IsValid())
	{
		OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Type, McpSchemaKeywords::PointerOrRoot(OutViolation.Pointer),
			TEXT("value or schema is missing"));
		return false;
	}

	const FString Pointer = OutViolation.Pointer;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Keyword : Schema->Values)
	{
		if (!McpSchemaKeywords::IsSupportedKeyword(Keyword.Key))
		{
			OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::UnsupportedKeyword, McpSchemaKeywords::PointerOrRoot(Pointer),
				FString::Printf(TEXT("Schema keyword '%s' at %s is not implemented by the canonical validator"),
					*Keyword.Key, *McpSchemaKeywords::PointerOrRoot(Pointer)));
			return false;
		}
	}

	const TArray<FString> Types = McpSchemaKeywords::DeclaredTypes(Schema);
	if (Types.Num() > 0)
	{
		bool bMatched = false;
		for (const FString& Declared : Types)
		{
			if (McpSchemaKeywords::ValueMatchesType(Value, Declared))
			{
				bMatched = true;
				break;
			}
		}
		if (!bMatched)
		{
			OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Type, McpSchemaKeywords::PointerOrRoot(Pointer),
				FString::Printf(TEXT("%s must be of type %s"), *McpSchemaKeywords::PointerOrRoot(Pointer),
					*FString::Join(Types, TEXT(" | "))));
			return false;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Allowed = nullptr;
	if (Schema->TryGetArrayField(TEXT("enum"), Allowed) && Allowed)
	{
		bool bAllowed = false;
		for (const TSharedPtr<FJsonValue>& Candidate : *Allowed)
		{
			if (McpSchemaKeywords::JsonValuesEqual(Candidate, Value))
			{
				bAllowed = true;
				break;
			}
		}
		if (!bAllowed)
		{
			OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Enum, McpSchemaKeywords::PointerOrRoot(Pointer),
				FString::Printf(TEXT("%s is not an allowed value"), *McpSchemaKeywords::PointerOrRoot(Pointer)));
			return false;
		}
	}

	if (!McpSchemaKeywords::CheckBounds(Value, Schema, McpSchemaKeywords::PointerOrRoot(Pointer), OutViolation))
	{
		return false;
	}

	if (Value->Type == EJson::Object)
	{
		return ValidateObjectBody(Value->AsObject(), Schema, Pointer, OutViolation);
	}

	if (Value->Type == EJson::Array)
	{
		const TSharedPtr<FJsonObject>* ItemSchema = nullptr;
		if (Schema->TryGetObjectField(TEXT("items"), ItemSchema) && ItemSchema)
		{
			const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
			for (int32 Index = 0; Index < Items.Num(); ++Index)
			{
				FMcpSchemaViolationDetail ItemViolation;
				ItemViolation.Pointer = McpSchemaKeywords::JoinPointer(Pointer, FString::FromInt(Index));
				if (!McpValidateAgainstCanonicalSchema(Items[Index], *ItemSchema, ItemViolation))
				{
					OutViolation = ItemViolation;
					return false;
				}
			}
		}
	}

	OutViolation = FMcpSchemaViolationDetail();
	return true;
}

namespace
{
bool ValidateObjectBody(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation)
{
	// A reflection boundary is an intentionally open object (Task 2): its interior
	// is arbitrary Unreal property data, so interior keys are not whitelisted.
	bool bReflectionBoundary = false;
	if (Schema->TryGetBoolField(TEXT("x-unreal-reflection-boundary"), bReflectionBoundary) &&
		bReflectionBoundary)
	{
		OutViolation = FMcpSchemaViolationDetail();
		return true;
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	const bool bHasProperties =
		Schema->TryGetObjectField(TEXT("properties"), Properties) && Properties;

	const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
	if (Schema->TryGetArrayField(TEXT("required"), Required) && Required)
	{
		for (const TSharedPtr<FJsonValue>& RequiredValue : *Required)
		{
			FString Name;
			if (RequiredValue.IsValid() && RequiredValue->TryGetString(Name) &&
				!Object->HasField(Name))
			{
				OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::MissingRequired,
					McpSchemaKeywords::JoinPointer(Pointer, Name),
					FString::Printf(TEXT("Missing required parameter '%s'"), *Name));
				return false;
			}
		}
	}

	bool bAdditionalProperties = true;
	const bool bClosed =
		Schema->TryGetBoolField(TEXT("additionalProperties"), bAdditionalProperties) &&
		!bAdditionalProperties;
	if (bClosed && bHasProperties)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
		{
			if (!(*Properties)->HasField(Entry.Key))
			{
				OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Undeclared,
					McpSchemaKeywords::JoinPointer(Pointer, Entry.Key),
					FString::Printf(TEXT("Undeclared parameter '%s'"), *Entry.Key));
				return false;
			}
		}
	}

	if (bHasProperties)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
		{
			const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
			if (!(*Properties)->TryGetObjectField(Entry.Key, PropertySchema) || !PropertySchema)
			{
				continue;
			}
			FMcpSchemaViolationDetail PropertyViolation;
			PropertyViolation.Pointer = McpSchemaKeywords::JoinPointer(Pointer, Entry.Key);
			if (!McpValidateAgainstCanonicalSchema(Entry.Value, *PropertySchema, PropertyViolation))
			{
				OutViolation = PropertyViolation;
				return false;
			}
		}
	}

	OutViolation = FMcpSchemaViolationDetail();
	return true;
}
}

bool McpValidateObjectAgainstCanonicalSchema(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation)
{
	OutViolation = FMcpSchemaViolationDetail();
	if (!Object.IsValid() || !Schema.IsValid())
	{
		OutViolation = McpSchemaKeywords::MakeViolation(EMcpSchemaViolation::Type, TEXT("/"),
			TEXT("arguments could not be validated"));
		return false;
	}
	return McpValidateAgainstCanonicalSchema(MakeShared<FJsonValueObject>(Object), Schema, OutViolation);
}

TSharedPtr<FJsonObject> McpApplyCanonicalSchemaDefaults(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema)
{
	TSharedPtr<FJsonObject> WithDefaults = MakeShared<FJsonObject>();
	if (Params.IsValid())
	{
		WithDefaults->Values = Params->Values;
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!Schema.IsValid() || !Schema->TryGetObjectField(TEXT("properties"), Properties) || !Properties)
	{
		return WithDefaults;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : (*Properties)->Values)
	{
		if (WithDefaults->HasField(Property.Key) || !Property.Value.IsValid() ||
			Property.Value->Type != EJson::Object)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& PropertySchema = Property.Value->AsObject();
		const TSharedPtr<FJsonValue> Default = PropertySchema->TryGetField(TEXT("default"));
		if (Default.IsValid())
		{
			WithDefaults->SetField(Property.Key, Default);
		}
	}
	return WithDefaults;
}
