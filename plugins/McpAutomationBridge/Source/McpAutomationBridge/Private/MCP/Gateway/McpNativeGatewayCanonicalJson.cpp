// McpNativeGatewayCanonicalJson.cpp — see header for the cross-language contract.

#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"

namespace
{
void AppendEscaped(const FString& Value, FString& Out)
{
	Out.AppendChar(TEXT('"'));
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		const TCHAR Ch = Value[Index];
		if (Ch == TEXT('"'))
		{
			Out.Append(TEXT("\\\""));
		}
		else if (Ch == TEXT('\\'))
		{
			Out.Append(TEXT("\\\\"));
		}
		else if (Ch >= 0x20 && Ch <= 0x7e)
		{
			Out.AppendChar(Ch);
		}
		else
		{
			// JSON.stringify escapes per UTF-16 code unit, so an astral character
			// must emit a surrogate PAIR. TCHAR is UTF-16 on Windows (already a
			// pair) but UTF-32 on Linux/Mac, where the pair has to be derived or
			// the two surfaces disagree on any non-BMP character a client sends.
			const uint32 Code = static_cast<uint32>(Ch);
			if (Code <= 0xffff)
			{
				Out.Append(FString::Printf(TEXT("\\u%04x"), Code));
			}
			else
			{
				const uint32 Adjusted = Code - 0x10000;
				Out.Append(FString::Printf(TEXT("\\u%04x\\u%04x"),
					0xd800 + (Adjusted >> 10), 0xdc00 + (Adjusted & 0x3ff)));
			}
		}
	}
	Out.AppendChar(TEXT('"'));
}

bool AppendValue(const TSharedPtr<FJsonValue>& Value, FString& Out);

bool AppendObject(const TSharedPtr<FJsonObject>& Object, FString& Out)
{
	if (!Object.IsValid())
	{
		Out.Append(TEXT("{}"));
		return true;
	}
	TArray<FString> Keys;
	Keys.Reserve(Object->Values.Num());
	for (const auto& Pair : Object->Values)
	{
		Keys.Add(Pair.Key);
	}
	// UE FString relational operators are case-INsensitive; discovery output is
	// diffed against JSON.stringify key order, which is by code unit.
	Keys.Sort([](const FString& L, const FString& R) { return L.Compare(R, ESearchCase::CaseSensitive) < 0; });

	Out.AppendChar(TEXT('{'));
	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (Index > 0) Out.AppendChar(TEXT(','));
		AppendEscaped(Keys[Index], Out);
		Out.AppendChar(TEXT(':'));
		if (!AppendValue(Object->Values[Keys[Index]], Out)) return false;
	}
	Out.AppendChar(TEXT('}'));
	return true;
}

bool AppendValue(const TSharedPtr<FJsonValue>& Value, FString& Out)
{
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		Out.Append(TEXT("null"));
		return true;
	}
	switch (Value->Type)
	{
	case EJson::Boolean:
		Out.Append(Value->AsBool() ? TEXT("true") : TEXT("false"));
		return true;
	case EJson::String:
		AppendEscaped(Value->AsString(), Out);
		return true;
	case EJson::Number:
	{
		const double Number = Value->AsNumber();
		if (Number != FMath::TruncToDouble(Number)) return false;
		Out.Append(FString::Printf(TEXT("%lld"), static_cast<int64>(Number)));
		return true;
	}
	case EJson::Array:
	{
		Out.AppendChar(TEXT('['));
		const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Index > 0) Out.AppendChar(TEXT(','));
			if (!AppendValue(Items[Index], Out)) return false;
		}
		Out.AppendChar(TEXT(']'));
		return true;
	}
	case EJson::Object:
		return AppendObject(Value->AsObject(), Out);
	default:
		return false;
	}
}
}

bool McpCanonicalJson(const TSharedPtr<FJsonValue>& Value, FString& OutJson)
{
	OutJson.Reset();
	return AppendValue(Value, OutJson);
}

bool McpCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson)
{
	OutJson.Reset();
	return AppendObject(Object, OutJson);
}

int32 McpCanonicalByteLength(const FString& CanonicalJson)
{
	return CanonicalJson.Len();
}
