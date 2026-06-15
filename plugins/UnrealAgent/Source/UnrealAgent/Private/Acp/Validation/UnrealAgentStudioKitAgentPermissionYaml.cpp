#include "Acp/Validation/UnrealAgentStudioKitAgentPermissionYaml.h"

namespace UnrealAgent::Validation::Yaml
{
FString UnquoteScalar(FString Value)
{
    Value.TrimStartAndEndInline();
    if (Value.Len() >= 2
        && ((Value[0] == TEXT('"') && Value[Value.Len() - 1] == TEXT('"'))
            || (Value[0] == TEXT('\'') && Value[Value.Len() - 1] == TEXT('\''))))
    {
        Value = Value.Mid(1, Value.Len() - 2);
    }
    return Value.TrimStartAndEnd();
}

int32 CountIndent(const FString& Line)
{
    int32 Indent = 0;
    while (Indent < Line.Len() && FChar::IsWhitespace(Line[Indent]))
    {
        ++Indent;
    }
    return Indent;
}

bool SplitField(const FString& Text, FString& OutKey, FString& OutValue)
{
    TCHAR Quote = TEXT('\0');
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Character = Text[Index];
        if (Quote == TEXT('\0') && (Character == TEXT('"') || Character == TEXT('\'')))
        {
            Quote = Character;
        }
        else if (Quote == Character)
        {
            Quote = TEXT('\0');
        }
        else if (Quote == TEXT('\0') && Character == TEXT(':'))
        {
            OutKey = UnquoteScalar(Text.Left(Index));
            OutValue = Text.Mid(Index + 1).TrimStartAndEnd();
            return !OutKey.IsEmpty();
        }
    }
    return false;
}

bool HasUnsupportedKeySyntax(const FString& Text)
{
    TCHAR Quote = TEXT('\0');
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Character = Text[Index];
        if (Quote == TEXT('\0') && (Character == TEXT('"') || Character == TEXT('\'')))
        {
            Quote = Character;
        }
        else if (Quote == Character)
        {
            Quote = TEXT('\0');
        }
        else if (Quote == TEXT('\0') && Character == TEXT(':'))
        {
            const FString RawKey = Text.Left(Index).TrimStartAndEnd();
            if (RawKey.IsEmpty() || RawKey.Contains(TEXT("\\")))
            {
                return true;
            }
            if ((RawKey.StartsWith(TEXT("\"")) && RawKey.EndsWith(TEXT("\"")))
                || (RawKey.StartsWith(TEXT("'")) && RawKey.EndsWith(TEXT("'"))))
            {
                return false;
            }
            if (RawKey == TEXT("<<"))
            {
                return true;
            }
            const TCHAR First = RawKey[0];
            return First == TEXT('!') || First == TEXT('&') || First == TEXT('*')
                || First == TEXT('>') || First == TEXT('|');
        }
    }
    return true;
}

bool HasUnsupportedReferenceSyntax(const FString& Text)
{
    TCHAR Quote = TEXT('\0');
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Character = Text[Index];
        if (Quote == TEXT('\0') && (Character == TEXT('"') || Character == TEXT('\'')))
        {
            Quote = Character;
            continue;
        }
        if (Quote == Character)
        {
            Quote = TEXT('\0');
            continue;
        }
        if (Quote != TEXT('\0'))
        {
            continue;
        }
        if (Character == TEXT('#'))
        {
            break;
        }
        const bool bTokenStart = Index == 0
            || FChar::IsWhitespace(Text[Index - 1])
            || Text[Index - 1] == TEXT(':')
            || Text[Index - 1] == TEXT(',')
            || Text[Index - 1] == TEXT('{')
            || Text[Index - 1] == TEXT('[');
        if (bTokenStart
            && (Character == TEXT('&')
                || Character == TEXT('*')
                || Character == TEXT('!')))
        {
            return true;
        }
        if (Character == TEXT('<')
            && Index + 1 < Text.Len()
            && Text[Index + 1] == TEXT('<')
            && bTokenStart)
        {
            return true;
        }
    }
    return false;
}

FString StripComment(const FString& Text)
{
    TCHAR Quote = TEXT('\0');
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Character = Text[Index];
        if (Quote == TEXT('\0') && (Character == TEXT('"') || Character == TEXT('\'')))
        {
            Quote = Character;
        }
        else if (Quote == Character)
        {
            Quote = TEXT('\0');
        }
        else if (Quote == TEXT('\0') && Character == TEXT('#'))
        {
            return Text.Left(Index).TrimStartAndEnd();
        }
    }
    return Text.TrimStartAndEnd();
}

bool ContainsAllowValue(const FString& Text)
{
    FString Normalized = StripComment(Text).ToLower();
    for (const TCHAR Character : FString(TEXT("\"'{}[],:")))
    {
        Normalized.ReplaceCharInline(Character, TEXT(' '));
    }
    TArray<FString> Tokens;
    Normalized.ParseIntoArrayWS(Tokens);
    return Tokens.Contains(TEXT("allow"));
}

bool HasUnsupportedScalarSyntax(const FString& Text)
{
    const FString Trimmed = Text.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return false;
    }
    if (Trimmed.Contains(TEXT("\\")))
    {
        return true;
    }
    if (HasUnsupportedReferenceSyntax(Trimmed))
    {
        return true;
    }
    const TCHAR First = Trimmed[0];
    if (First == TEXT('>') || First == TEXT('|') || First == TEXT('&')
        || First == TEXT('*') || First == TEXT('!'))
    {
        return true;
    }
    return (First == TEXT('"') && !Trimmed.EndsWith(TEXT("\"")))
        || (First == TEXT('\'') && !Trimmed.EndsWith(TEXT("'")));
}

void SplitFlowFields(const FString& Text, TArray<FString>& OutFields)
{
    FString Current;
    TCHAR Quote = TEXT('\0');
    int32 Depth = 0;
    for (const TCHAR Character : Text)
    {
        if (Quote == TEXT('\0') && (Character == TEXT('"') || Character == TEXT('\'')))
        {
            Quote = Character;
        }
        else if (Quote == Character)
        {
            Quote = TEXT('\0');
        }
        else if (Quote == TEXT('\0') && (Character == TEXT('{') || Character == TEXT('[')))
        {
            ++Depth;
        }
        else if (Quote == TEXT('\0') && (Character == TEXT('}') || Character == TEXT(']')))
        {
            --Depth;
        }
        if (Quote == TEXT('\0') && Depth == 0 && Character == TEXT(','))
        {
            OutFields.Add(Current);
            Current.Reset();
        }
        else
        {
            Current.AppendChar(Character);
        }
    }
    OutFields.Add(Current);
}
}
