// McpNativeGatewaySchemaKeywords.cpp — per-keyword semantics for the canonical
// Draft-2020-12 subset. The document traversal that applies these lives in
// McpNativeGatewaySchemaValidation.cpp.

#include "MCP/Execute/McpNativeGatewaySchemaKeywords.h"

namespace McpSchemaKeywords
{
const TCHAR* const SupportedKeywords[] = {
	TEXT("$schema"),
	TEXT("type"),
	TEXT("description"),
	TEXT("properties"),
	TEXT("required"),
	TEXT("additionalProperties"),
	TEXT("items"),
	TEXT("minItems"),
	TEXT("maxItems"),
	TEXT("enum"),
	TEXT("default"),
	TEXT("minimum"),
	TEXT("maximum"),
	TEXT("maxLength"),
	TEXT("x-unreal-reflection-boundary"),
};

bool IsSupportedKeyword(const FString& Keyword)
{
	for (const TCHAR* Supported : SupportedKeywords)
	{
		if (Keyword == Supported)
		{
			return true;
		}
	}
	return false;
}

FString JoinPointer(const FString& Pointer, const FString& Segment)
{
	return Pointer + TEXT("/") + Segment;
}

FString PointerOrRoot(const FString& Pointer)
{
	return Pointer.IsEmpty() ? TEXT("/") : Pointer;
}

bool ValueMatchesType(const TSharedPtr<FJsonValue>& Value, const FString& Declared)
{
	switch (Value->Type)
	{
	case EJson::String:
		return Declared == TEXT("string");
	case EJson::Number:
	{
		const double Number = Value->AsNumber();
		if (!FMath::IsFinite(Number))
		{
			return false;
		}
		if (Declared == TEXT("number"))
		{
			return true;
		}
		constexpr double MaxExactJsonInteger = 9007199254740991.0;
		return Declared == TEXT("integer") &&
			FMath::Abs(Number) <= MaxExactJsonInteger &&
			Number == FMath::TruncToDouble(Number);
	}
	case EJson::Boolean:
		return Declared == TEXT("boolean");
	case EJson::Array:
		return Declared == TEXT("array");
	case EJson::Object:
		return Declared == TEXT("object");
	case EJson::Null:
		return Declared == TEXT("null");
	default:
		return false;
	}
}

TArray<FString> DeclaredTypes(const TSharedPtr<FJsonObject>& Schema)
{
	TArray<FString> Types;
	FString Single;
	if (Schema->TryGetStringField(TEXT("type"), Single))
	{
		Types.Add(Single);
		return Types;
	}
	const TArray<TSharedPtr<FJsonValue>>* Declared = nullptr;
	if (Schema->TryGetArrayField(TEXT("type"), Declared) && Declared)
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Declared)
		{
			FString Name;
			if (Entry.IsValid() && Entry->TryGetString(Name))
			{
				Types.Add(Name);
			}
		}
	}
	return Types;
}

bool JsonValuesEqual(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
{
	if (!Left.IsValid() || !Right.IsValid() || Left->Type != Right->Type)
	{
		return false;
	}
	switch (Left->Type)
	{
	case EJson::String:
		return Left->AsString() == Right->AsString();
	case EJson::Number:
		return Left->AsNumber() == Right->AsNumber();
	case EJson::Boolean:
		return Left->AsBool() == Right->AsBool();
	case EJson::Null:
		return true;
	default:
		return false;
	}
}

FMcpSchemaViolationDetail MakeViolation(
	EMcpSchemaViolation Reason, const FString& Pointer, const FString& Message)
{
	FMcpSchemaViolationDetail Violation;
	Violation.Reason = Reason;
	Violation.Pointer = Pointer;
	Violation.Message = Message;
	return Violation;
}

bool CheckBounds(
	const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation)
{
	double Bound = 0.0;
	if (Value->Type == EJson::Number)
	{
		const double Number = Value->AsNumber();
		if (Schema->TryGetNumberField(TEXT("minimum"), Bound) && Number < Bound)
		{
			OutViolation = MakeViolation(EMcpSchemaViolation::Range, Pointer,
				FString::Printf(TEXT("%s must be >= %s"), *Pointer, *LexToString(Bound)));
			return false;
		}
		if (Schema->TryGetNumberField(TEXT("maximum"), Bound) && Number > Bound)
		{
			OutViolation = MakeViolation(EMcpSchemaViolation::Range, Pointer,
				FString::Printf(TEXT("%s must be <= %s"), *Pointer, *LexToString(Bound)));
			return false;
		}
	}
	if (Value->Type == EJson::String && Schema->TryGetNumberField(TEXT("maxLength"), Bound) &&
		Value->AsString().Len() > static_cast<int32>(Bound))
	{
		OutViolation = MakeViolation(EMcpSchemaViolation::Range, Pointer,
			FString::Printf(TEXT("%s must be at most %d characters"), *Pointer, static_cast<int32>(Bound)));
		return false;
	}
	if (Value->Type == EJson::Array)
	{
		const int32 Count = Value->AsArray().Num();
		if (Schema->TryGetNumberField(TEXT("minItems"), Bound) && Count < static_cast<int32>(Bound))
		{
			OutViolation = MakeViolation(EMcpSchemaViolation::Range, Pointer,
				FString::Printf(TEXT("%s must have at least %d item(s)"), *Pointer, static_cast<int32>(Bound)));
			return false;
		}
		if (Schema->TryGetNumberField(TEXT("maxItems"), Bound) && Count > static_cast<int32>(Bound))
		{
			OutViolation = MakeViolation(EMcpSchemaViolation::Range, Pointer,
				FString::Printf(TEXT("%s must have at most %d item(s)"), *Pointer, static_cast<int32>(Bound)));
			return false;
		}
	}
	return true;
}
}
