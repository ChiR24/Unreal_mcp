// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// The tokens of an assertion (McpPratt.h): names (letters, digits, _ - and /), quoted names, numbers, and the symbols
// .. == != <= >= < > ( ) and the dot between path parts.
namespace McpPratt
{
namespace Detail
{
enum class EKind : uint8
{
    Name,
    Quoted,
    Number,
    Symbol,
    End
};

struct FToken
{
    EKind Kind = EKind::End;
    FString Value;
    double Number = 0.0;
    int32 At = 0;
};

/** Whether Ch belongs in a name; a name starts with a letter, _ or /. */
bool IsNameChar(TCHAR Ch, bool bFirst);

/** Source as tokens, ending in an End token; false with OutError at the first character no token starts with. */
bool Lex(const FString& Source, TArray<FToken>& Out, FString& OutError);
} // namespace Detail
} // namespace McpPratt
