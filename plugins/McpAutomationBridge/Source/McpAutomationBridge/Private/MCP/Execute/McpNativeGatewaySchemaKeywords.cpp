#include "MCP/Execute/McpNativeGatewaySchemaKeywords.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "MCP/Execute/Request/McpNativeGatewayParameterHints.h"
// McpNativeGatewaySchemaKeywords.cpp — per-keyword semantics for the canonical
// Draft-2020-12 subset. The document traversal that applies these lives in
// McpNativeGatewaySchemaValidation.cpp.


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
	TEXT("requiredOneOf"),
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
			if (Entry.IsValid() && McpHandlerUtils::TryGetJsonValueString(Entry, Name))
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

bool CheckRequiredOneOf(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation)
{
	const TArray<TSharedPtr<FJsonValue>>* RequiredOneOf = nullptr;
	if (!Schema.IsValid() ||
		!Schema->TryGetArrayField(TEXT("requiredOneOf"), RequiredOneOf) || !RequiredOneOf)
	{
		return true;
	}
	TArray<FString> GroupNames;
	bool bAnyPresent = false;
	for (const TSharedPtr<FJsonValue>& GroupValue : *RequiredOneOf)
	{
		FString Name;
		if (GroupValue.IsValid() && McpHandlerUtils::TryGetJsonValueString(GroupValue, Name))
		{
			GroupNames.Add(Name);
			bAnyPresent = bAnyPresent || Object->HasField(Name);
		}
	}
	if (!bAnyPresent)
	{
		OutViolation = MakeViolation(EMcpSchemaViolation::RequiredOneOf,
			JoinPointer(Pointer, TEXT("requiredOneOf")),
				FString::Printf(TEXT("At least one of [%s] must be provided"),
					*FString::Join(GroupNames, TEXT(", "))));
			return false;
		}
	return true;
}

FString DescribeAllowedValues(const TArray<TSharedPtr<FJsonValue>>& Allowed, int32 MaxNames)
{
	TArray<FString> Names;
	for (const TSharedPtr<FJsonValue>& Candidate : Allowed)
	{
		Names.Add(McpHandlerUtils::JsonValueToString(Candidate));
		if (Names.Num() >= MaxNames)
		{
			break;
		}
	}
	FString Text = FString::Join(Names, TEXT(" | "));
	if (Allowed.Num() > Names.Num())
	{
		Text += FString::Printf(TEXT(" | ... (%d more)"), Allowed.Num() - Names.Num());
	}
	return Text;
}

// "Undeclared parameter 'x'" named the one spelling that does NOT work and
// nothing that does, so every wrong guess cost a describe round trip for a list
// the validator already held. Byte-for-byte identical to the TypeScript
// gateway's describeUndeclaredParameter: case-sensitive sort, related names
// ranked ahead of the rest (see McpParameterHints), at most 24 names listed.
FString DescribeUndeclaredParameter(const FString& Key, const TSharedPtr<FJsonObject>& Properties)
{
	TArray<FString> Declared;
	if (Properties.IsValid())
	{
		for (const auto& Entry : Properties->Values) { Declared.Add(*Entry.Key); }
	}
	if (Declared.Num() == 0)
	{
		return FString::Printf(TEXT("Undeclared parameter '%s' (this action declares no parameters)"), *Key);
	}
	Declared.Sort([](const FString& A, const FString& B) { return A.Compare(B, ESearchCase::CaseSensitive) < 0; });
	FString Hint;
	TArray<FString> Ranked = McpParameterHints::RankParameterNames(Key, Declared, Hint);
	const int32 ListedCount = FMath::Min(Ranked.Num(), 24);
	const FString More = Ranked.Num() > ListedCount ? FString::Printf(TEXT(" and %d more"), Ranked.Num() - ListedCount) : FString();
	Ranked.SetNum(ListedCount);
	return FString::Printf(TEXT("Undeclared parameter '%s' (%sallowed: %s%s)"), *Key, *Hint, *FString::Join(Ranked, TEXT(", ")), *More);
}

}
