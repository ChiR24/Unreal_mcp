#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"

namespace McpGraphCode
{
namespace ParseParts
{
struct FToken
{
    // ( or ), in Atom.Text.
    bool bParen = false;
    FSExpr Atom;
};

int32 LineAt(const FString& Text, int32 At)
{
    int32 Line = 1;
    for (int32 Each = 0; Each < At && Each < Text.Len(); ++Each)
    {
        Line += Text[Each] == TEXT('\n') ? 1 : 0;
    }
    return Line;
}

// The closing quote of the string opened at Open, or past the end when it never closes; a backslash skips one character.
int32 StringEnd(const FString& Text, int32 Open)
{
    int32 At = Open + 1;
    while (At < Text.Len() && Text[At] != TEXT('"'))
    {
        At += Text[At] == TEXT('\\') ? 2 : 1;
    }
    return At;
}

FString Unescape(const FString& Raw)
{
    return Raw.Replace(TEXT("\\\""), TEXT("\"")).Replace(TEXT("\\\\"), TEXT("\\"));
}

bool IsBreak(TCHAR Ch)
{
    return Ch == TEXT(' ') || Ch == TEXT('\t') || Ch == TEXT('\n') || Ch == TEXT('\r') || Ch == TEXT(';') || Ch == TEXT('"') || Ch == TEXT(')');
}

// What Python's int() and then float() accept, as Epic's tokenizer tries them.
FSExpr::EType NumberType(const FString& Atom)
{
    int32 At = Atom.Len() > 0 && (Atom[0] == TEXT('+') || Atom[0] == TEXT('-')) ? 1 : 0;
    const FString Word = Atom.Mid(At).ToLower();
    if (Word == TEXT("inf") || Word == TEXT("infinity") || Word == TEXT("nan"))
    {
        return FSExpr::EType::Float;
    }
    int32 Digits = 0;
    bool bDot = false;
    for (; At < Atom.Len() && (FChar::IsDigit(Atom[At]) || (Atom[At] == TEXT('.') && !bDot)); ++At)
    {
        Digits += FChar::IsDigit(Atom[At]) ? 1 : 0;
        bDot |= Atom[At] == TEXT('.');
    }
    bool bExponent = false;
    if (Digits > 0 && At < Atom.Len() && (Atom[At] == TEXT('e') || Atom[At] == TEXT('E')))
    {
        At += At + 1 < Atom.Len() && (Atom[At + 1] == TEXT('+') || Atom[At + 1] == TEXT('-')) ? 2 : 1;
        const int32 Start = At;
        while (At < Atom.Len() && FChar::IsDigit(Atom[At]))
        {
            ++At;
        }
        bExponent = true;
        Digits = At > Start ? Digits : 0;
    }
    if (Digits == 0 || At != Atom.Len())
    {
        return FSExpr::EType::Symbol;
    }
    return bDot || bExponent ? FSExpr::EType::Float : FSExpr::EType::Int;
}

bool Tokenize(const FString& Text, TArray<FToken>& Out, FString& OutError)
{
    int32 At = 0;
    while (At < Text.Len())
    {
        const TCHAR Ch = Text[At];
        if (Ch == TEXT(';'))
        {
            while (At < Text.Len() && Text[At] != TEXT('\n'))
            {
                ++At;
            }
        }
        else if (Ch == TEXT('(') || Ch == TEXT(')'))
        {
            FToken& Token = Out.AddDefaulted_GetRef();
            Token.bParen = true;
            Token.Atom.Text = FString::Chr(Ch);
            ++At;
        }
        else if (Ch == TEXT(' ') || Ch == TEXT('\t') || Ch == TEXT('\n') || Ch == TEXT('\r'))
        {
            ++At;
        }
        else
        {
            const bool bString = Ch == TEXT('"');
            int32 End = At;
            while (!bString && End < Text.Len() && !IsBreak(Text[End]))
            {
                if (Text[End] == TEXT('('))
                {
                    // A balanced (...) belongs to the symbol: Equal(==), sin(degrees).
                    for (int32 Depth = 0; End < Text.Len();)
                    {
                        Depth += Text[End] == TEXT('(') ? 1 : Text[End] == TEXT(')') ? -1 : 0;
                        ++End;
                        if (Depth == 0)
                        {
                            break;
                        }
                    }
                    break;
                }
                ++End;
            }
            const FString Atom = Text.Mid(At, End - At);
            // A string, or a keyword quoted after its colon (:"Pin Name").
            const int32 Open = bString ? At : Atom == TEXT(":") && End < Text.Len() && Text[End] == TEXT('"') ? End : INDEX_NONE;
            FToken& Token = Out.AddDefaulted_GetRef();
            if (Open != INDEX_NONE)
            {
                const int32 Close = StringEnd(Text, Open);
                if (Close >= Text.Len())
                {
                    OutError = FString::Printf(TEXT("Line %d: unterminated string%s"), LineAt(Text, Open), bString ? TEXT(" literal") : TEXT(" in quoted keyword"));
                    return false;
                }
                Token.Atom.Type = bString ? FSExpr::EType::String : FSExpr::EType::Symbol;
                Token.Atom.Text = (bString ? FString() : FString(TEXT(":"))) + Unescape(Text.Mid(Open + 1, Close - Open - 1));
                At = Close + 1;
                continue;
            }
            const bool bBool = Atom.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Atom.Equals(TEXT("false"), ESearchCase::IgnoreCase);
            Token.Atom.Type = bBool ? FSExpr::EType::Bool : NumberType(Atom);
            Token.Atom.bTrue = Atom.Equals(TEXT("true"), ESearchCase::IgnoreCase);
            Token.Atom.Text = Atom;
            At = End;
        }
    }
    return true;
}

bool IsClose(const FToken& Token)
{
    return Token.bParen && Token.Atom.Text == TEXT(")");
}

bool ReadForm(const TArray<FToken>& Tokens, int32& Pos, FSExpr& Out, FString& OutError)
{
    if (!Tokens.IsValidIndex(Pos) || IsClose(Tokens[Pos]))
    {
        OutError = Tokens.IsValidIndex(Pos) ? TEXT("Unexpected )") : TEXT("Unexpected end of input");
        return false;
    }
    if (!Tokens[Pos].bParen)
    {
        Out = Tokens[Pos++].Atom;
        return true;
    }
    ++Pos;
    Out.Type = FSExpr::EType::List;
    while (Tokens.IsValidIndex(Pos))
    {
        if (IsClose(Tokens[Pos]))
        {
            ++Pos;
            return true;
        }
        if (!ReadForm(Tokens, Pos, Out.Items.AddDefaulted_GetRef(), OutError))
        {
            return false;
        }
    }
    OutError = TEXT("Unclosed parenthesis");
    return false;
}
} // namespace ParseParts

bool ParseSExpr(const FString& Text, TArray<FSExpr>& OutForms, FString& OutError)
{
    TArray<ParseParts::FToken> Tokens;
    if (!ParseParts::Tokenize(Text, Tokens, OutError))
    {
        return false;
    }
    for (int32 Pos = 0; Pos < Tokens.Num();)
    {
        if (!ParseParts::ReadForm(Tokens, Pos, OutForms.AddDefaulted_GetRef(), OutError))
        {
            return false;
        }
    }
    return true;
}
} // namespace McpGraphCode
