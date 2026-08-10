#include "MCP/Primitives/McpPromptArgumentValidation.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "MCP/Resources/McpResourceUri.h"

namespace
{
	bool HasControlChar(const FString& Value)
	{
		for (const TCHAR Ch : Value)
		{
			if (Ch <= 0x1f || Ch == 0x7f)
			{
				return true;
			}
		}
		return false;
	}

	bool IsIdentifier(const FString& Value)
	{
		if (Value.IsEmpty() || !(FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_')))
		{
			return false;
		}
		for (int32 Index = 1; Index < Value.Len(); ++Index)
		{
			if (!(FChar::IsAlnum(Value[Index]) || Value[Index] == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsEngineVersion(const FString& Value)
	{
		FString Major;
		FString Minor;
		if (!Value.Split(TEXT("."), &Major, &Minor) || Major.IsEmpty() || Minor.IsEmpty() || Minor.Contains(TEXT(".")))
		{
			return false;
		}
		for (const TCHAR Ch : Major + Minor)
		{
			if (!FChar::IsDigit(Ch))
			{
				return false;
			}
		}
		return true;
	}

	bool Invalid(const FString& Name, const FString& Detail, FString& OutCode, FString& OutMsg)
	{
		OutCode = McpPromptErrorCodes::InvalidArgument;
		OutMsg = FString::Printf(TEXT("Argument \"%s\" %s"), *Name, *Detail);
		return false;
	}

	bool ValidateContentPath(const FString& Name, const FString& Value, FString& OutCode, FString& OutMsg)
	{
		if (Value.IsEmpty())
		{
			return Invalid(Name, TEXT("is empty"), OutCode, OutMsg);
		}
		if (HasControlChar(Value))
		{
			return Invalid(Name, TEXT("has control characters"), OutCode, OutMsg);
		}
		if (McpResourceUri::LooksLikeHostPath(Value) || Value.StartsWith(TEXT("~")))
		{
			return Invalid(Name, TEXT("is a host filesystem path"), OutCode, OutMsg);
		}
		TArray<FString> Segments;
		Value.ParseIntoArray(Segments, TEXT("/"), false);
		if (Segments.Contains(TEXT("..")))
		{
			return Invalid(Name, TEXT("contains path traversal"), OutCode, OutMsg);
		}
		if (!McpResourceUri::IsUnderContentRoot(Value))
		{
			return Invalid(Name, TEXT("must resolve under a UE content root (/Game, /Engine, /Script, /Temp, /Niagara)"), OutCode, OutMsg);
		}
		return true;
	}
}  // namespace

// Mirror SECRET_VALUE_PATTERN: PEM blocks, bearer tokens, JWTs, and long hex.
bool McpPromptValueLooksSecret(const FString& Value)
{
	if (Value.Contains(TEXT("-----BEGIN")) || Value.Contains(TEXT("Bearer ")))
	{
		return true;
	}
	if (Value.StartsWith(TEXT("eyJ")) && Value.Contains(TEXT(".")))
	{
		return true;
	}
	int32 HexRun = 0;
	for (const TCHAR Ch : Value)
	{
		const bool bHex = FChar::IsDigit(Ch) || (Ch >= TEXT('a') && Ch <= TEXT('f')) || (Ch >= TEXT('A') && Ch <= TEXT('F'));
		HexRun = bHex ? HexRun + 1 : 0;
		if (HexRun >= 40)
		{
			return true;
		}
	}
	return false;
}

bool McpValidatePromptArgument(const FMcpPromptArgumentSpec& Spec, const FString& Value, FString& OutCode, FString& OutMsg)
{
	if (Value.Len() > McpMaxPromptArgumentLength)
	{
		OutCode = McpPromptErrorCodes::ArgumentTooLong;
		OutMsg = FString::Printf(TEXT("Argument \"%s\" is %d chars, exceeding the %d limit"), *Spec.Name, Value.Len(), McpMaxPromptArgumentLength);
		return false;
	}
	if (Spec.Kind == TEXT("content-path"))
	{
		return ValidateContentPath(Spec.Name, Value, OutCode, OutMsg);
	}
	if (Spec.Kind == TEXT("object-path"))
	{
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT("."), false);
		if (Parts.Num() > 2 || Parts.Num() == 0)
		{
			return Invalid(Spec.Name, TEXT("is not a valid object path"), OutCode, OutMsg);
		}
		if (!ValidateContentPath(Spec.Name, Parts[0], OutCode, OutMsg))
		{
			return false;
		}
		if (Parts.Num() == 2 && !IsIdentifier(Parts[1]))
		{
			return Invalid(Spec.Name, TEXT("has an invalid object suffix"), OutCode, OutMsg);
		}
		return true;
	}
	if (Spec.Kind == TEXT("identifier"))
	{
		return IsIdentifier(Value) ? true : Invalid(Spec.Name, TEXT("is not a valid identifier"), OutCode, OutMsg);
	}
	if (Spec.Kind == TEXT("enum"))
	{
		return Spec.Allowed.Contains(Value)
			? true
			: Invalid(Spec.Name, FString::Printf(TEXT("must be one of: %s"), *FString::Join(Spec.Allowed, TEXT(", "))), OutCode, OutMsg);
	}
	if (Spec.Kind == TEXT("engine-version"))
	{
		return IsEngineVersion(Value) ? true : Invalid(Spec.Name, TEXT("is not a MAJOR.MINOR engine version"), OutCode, OutMsg);
	}
	if (Spec.Kind == TEXT("text"))
	{
		return HasControlChar(Value) ? Invalid(Spec.Name, TEXT("has control characters"), OutCode, OutMsg) : true;
	}
	return Invalid(Spec.Name, TEXT("has an unknown kind"), OutCode, OutMsg);
}
