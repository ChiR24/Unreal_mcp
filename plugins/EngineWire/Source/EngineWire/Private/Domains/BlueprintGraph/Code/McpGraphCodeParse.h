#pragma once

#include "CoreMinimal.h"

// The S-expression reader of Epic's Blueprint DSL (tokenize and parse in blueprint_dsl.py, UE 5.8): ; comments to the end
// of the line, "strings" with \" and \\ escapes, true/false, integers, floats, and symbols, where a symbol keeps a
// balanced (...) it runs into (Equal(==), sin(degrees)) and :"Pin Name" is the keyword :Pin Name.
namespace McpGraphCode
{
struct FSExpr
{
    enum class EType : uint8
    {
        List,
        Symbol,
        String,
        Int,
        Float,
        Bool
    };
    EType Type = EType::List;
    // A symbol's or string's text, or a number as written.
    FString Text;
    bool bTrue = false;
    TArray<FSExpr> Items;
    // The line it starts on (1-based), and the node id of a "; n1a2b3c" comment ending that line, given to the first list
    // opened on the line (read_graph_as_code tags each statement so).
    int32 Line = 0;
    FString Tag;
};

/** The top-level forms of Text; false with OutError (and the line) on an unterminated string or unbalanced parens. */
bool ParseSExpr(const FString& Text, TArray<FSExpr>& OutForms, FString& OutError);
} // namespace McpGraphCode
