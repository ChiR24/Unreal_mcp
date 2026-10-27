// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpPrattLex.h"

#include "Algo/Find.h"

namespace McpPratt
{
namespace Detail
{
bool IsNameChar(TCHAR Ch, bool bFirst)
{
    return FChar::IsAlpha(Ch) || Ch == TEXT('_') || Ch == TEXT('/') || (!bFirst && (FChar::IsDigit(Ch) || Ch == TEXT('-')));
}

bool Lex(const FString& Source, TArray<FToken>& Out, FString& OutError)
{
    for (int32 At = 0; At < Source.Len();)
    {
        const TCHAR Ch = Source[At];
        const TCHAR Next = At + 1 < Source.Len() ? Source[At + 1] : TEXT('\0');
        if (FChar::IsWhitespace(Ch))
        {
            ++At;
            continue;
        }
        FToken& Token = Out.AddDefaulted_GetRef();
        Token.At = At;
        if (Ch == TEXT('"'))
        {
            Token.Kind = EKind::Quoted;
            for (++At; At < Source.Len() && Source[At] != TEXT('"'); ++At)
            {
                At += Source[At] == TEXT('\\') && At + 1 < Source.Len() ? 1 : 0;
                Token.Value.AppendChar(Source[At]);
            }
            if (At++ >= Source.Len())
            {
                OutError = FString::Printf(TEXT("the quote opened at character %d is never closed"), Token.At + 1);
                return false;
            }
        }
        else if (FChar::IsDigit(Ch) || (Ch == TEXT('-') && FChar::IsDigit(Next)))
        {
            int32 End = At + 1;
            while (End < Source.Len() && (FChar::IsDigit(Source[End]) ||
                                          (Source[End] == TEXT('.') && End + 1 < Source.Len() && FChar::IsDigit(Source[End + 1]))))
            {
                ++End;
            }
            Token.Kind = EKind::Number;
            Token.Value = Source.Mid(At, End - At);
            Token.Number = FCString::Atod(*Token.Value);
            At = End;
        }
        else if (IsNameChar(Ch, true))
        {
            int32 End = At + 1;
            while (End < Source.Len() && IsNameChar(Source[End], false))
            {
                ++End;
            }
            Token.Kind = EKind::Name;
            Token.Value = Source.Mid(At, End - At);
            At = End;
        }
        else
        {
            static const TCHAR* const Symbols[] = {TEXT(".."), TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">="), TEXT("<"), TEXT(">"),
                                                   TEXT("("), TEXT(")"), TEXT(".")};
            const TCHAR* const* Found = Algo::FindByPredicate(Symbols, [&Source, At](const TCHAR* Symbol) {
                return FCString::Strncmp(*Source + At, Symbol, FCString::Strlen(Symbol)) == 0;
            });
            if (!Found)
            {
                OutError = FString::Printf(TEXT("'%c' at character %d is not part of an assertion"), Ch, At + 1);
                return false;
            }
            Token.Kind = EKind::Symbol;
            Token.Value = *Found;
            At += Token.Value.Len();
        }
    }
    Out.AddDefaulted_GetRef().At = Source.Len();
    return true;
}
} // namespace Detail
} // namespace McpPratt
