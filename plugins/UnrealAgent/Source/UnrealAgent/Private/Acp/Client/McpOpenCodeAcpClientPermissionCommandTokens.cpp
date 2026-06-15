#include "Acp/Client/McpOpenCodeAcpClientPermissionCommandTokens.h"

namespace UnrealAgent::OpenCodeAcp::PermissionCommandTokens
{
namespace
{
int32 HexDigitValue(const TCHAR Character)
{
    if (Character >= TEXT('0') && Character <= TEXT('9'))
    {
        return Character - TEXT('0');
    }
    if (Character >= TEXT('a') && Character <= TEXT('f'))
    {
        return Character - TEXT('a') + 10;
    }
    if (Character >= TEXT('A') && Character <= TEXT('F'))
    {
        return Character - TEXT('A') + 10;
    }
    return INDEX_NONE;
}

void AppendAnsiCEscape(
    const FString& Command,
    int32& Index,
    FString& OutToken)
{
    if (Index + 1 >= Command.Len())
    {
        OutToken.AppendChar(TEXT('\\'));
        return;
    }
    const TCHAR Escape = Command[++Index];
    const TCHAR SimpleEscape =
        Escape == TEXT('a') ? TEXT('\a')
        : Escape == TEXT('b') ? TEXT('\b')
        : Escape == TEXT('e') || Escape == TEXT('E') ? TCHAR(27)
        : Escape == TEXT('f') ? TEXT('\f')
        : Escape == TEXT('n') ? TEXT('\n')
        : Escape == TEXT('r') ? TEXT('\r')
        : Escape == TEXT('t') ? TEXT('\t')
        : Escape == TEXT('v') ? TEXT('\v')
        : Escape;
    if (Escape != TEXT('x') && Escape != TEXT('u')
        && Escape != TEXT('U') && !(Escape >= TEXT('0') && Escape <= TEXT('7')))
    {
        OutToken.AppendChar(SimpleEscape);
        return;
    }

    int32 Value = 0;
    int32 Digits = 0;
    const int32 MaxDigits =
        Escape == TEXT('x') ? 2
        : Escape == TEXT('u') ? 4
        : Escape == TEXT('U') ? 8
        : 3;
    if (Escape >= TEXT('0') && Escape <= TEXT('7'))
    {
        Value = Escape - TEXT('0');
        Digits = 1;
    }
    while (Digits < MaxDigits && Index + 1 < Command.Len())
    {
        const int32 Digit = HexDigitValue(Command[Index + 1]);
        if (Digit == INDEX_NONE || (MaxDigits == 3 && Digit >= 8))
        {
            break;
        }
        Value = Value * (MaxDigits == 3 ? 8 : 16) + Digit;
        ++Digits;
        ++Index;
    }
    if (Digits == 0 || Value <= 0 || Value > 0x10ffff)
    {
        OutToken.AppendChar(TEXT('*'));
        return;
    }
    OutToken.AppendChar(static_cast<TCHAR>(Value));
}

void TokenizeQuotedCommandInternal(
    const FString& Command,
    TArray<FString>& OutTokens,
    const bool bPreserveSyntax)
{
    OutTokens.Reset();
    FString Current;
    TCHAR Quote = TEXT('\0');
    bool bAnsiCQuote = false;
    for (int32 Index = 0; Index < Command.Len(); ++Index)
    {
        const TCHAR Character = Command[Index];
        if (Quote != TEXT('\0'))
        {
            if (bPreserveSyntax)
            {
                Current.AppendChar(Character);
            }
            if (Character == Quote)
            {
                Quote = TEXT('\0');
                bAnsiCQuote = false;
            }
            else if (bAnsiCQuote && Character == TEXT('\\'))
            {
                AppendAnsiCEscape(Command, Index, Current);
            }
            else if (!bPreserveSyntax)
            {
                Current.AppendChar(Character);
            }
            continue;
        }
        if (Character == TEXT('\'') || Character == TEXT('"'))
        {
            if (bPreserveSyntax)
            {
                Current.AppendChar(Character);
            }
            else if (Current.EndsWith(TEXT("$")))
            {
                Current.LeftChopInline(1);
                bAnsiCQuote = Character == TEXT('\'');
            }
            Quote = Character;
            continue;
        }
        if (Character == TEXT('\\') && Index + 1 < Command.Len())
        {
            if (bPreserveSyntax)
            {
                Current.AppendChar(Character);
            }
            Current.AppendChar(Command[++Index]);
            continue;
        }
        if (FChar::IsWhitespace(Character))
        {
            if (!Current.IsEmpty())
            {
                OutTokens.Add(MoveTemp(Current));
                Current.Reset();
            }
            continue;
        }
        Current.AppendChar(Character);
    }
    if (!Current.IsEmpty())
    {
        OutTokens.Add(MoveTemp(Current));
    }
}
}

void TokenizeQuotedCommand(
    const FString& Command,
    TArray<FString>& OutTokens)
{
    TokenizeQuotedCommandInternal(Command, OutTokens, false);
}

void TokenizeQuotedCommandPreservingSyntax(
    const FString& Command,
    TArray<FString>& OutTokens)
{
    TokenizeQuotedCommandInternal(Command, OutTokens, true);
}
}
