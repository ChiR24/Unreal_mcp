#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"

namespace McpGraphCode
{
namespace ParseParts
{
// Reading, checking and running all recurse once per list level: a bound, so no text can overflow the game thread's stack.
constexpr int32 MaxDepth = 128;

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

// A read_graph_as_code tag: n and 6 or more hex digits.
bool IsNodeTag(const FString& Word)
{
    bool bHex = Word.Len() >= 7 && Word[0] == TEXT('n');
    for (int32 At = 1; bHex && At < Word.Len(); ++At)
    {
        bHex = FChar::IsHexDigit(Word[At]);
    }
    return bHex;
}

bool Tokenize(const FString& Text, TArray<FToken>& Out, TMap<int32, FString>& Tags, FString& OutError)
{
    int32 At = 0;
    int32 Line = 1;
    // Moves At to To, counting the line breaks passed (a string may hold some).
    const auto Advance = [&Text, &At, &Line](int32 To)
    {
        for (; At < To; ++At)
        {
            Line += Text[At] == TEXT('\n') ? 1 : 0;
        }
    };
    while (At < Text.Len())
    {
        const TCHAR Ch = Text[At];
        if (Ch == TEXT(';'))
        {
            int32 End = At;
            while (End < Text.Len() && Text[End] != TEXT('\n'))
            {
                ++End;
            }
            const FString Comment = Text.Mid(At + 1, End - At - 1).TrimStartAndEnd();
            FString Word = Comment;
            Comment.Split(TEXT(" "), &Word, nullptr);
            if (IsNodeTag(Word) && !Tags.Contains(Line))
            {
                Tags.Add(Line, Word);
            }
            Advance(End);
        }
        else if (Ch == TEXT('(') || Ch == TEXT(')'))
        {
            FToken& Token = Out.AddDefaulted_GetRef();
            Token.bParen = true;
            Token.Atom.Text = FString::Chr(Ch);
            Token.Atom.Line = Line;
            Advance(At + 1);
        }
        else if (Ch == TEXT(' ') || Ch == TEXT('\t') || Ch == TEXT('\n') || Ch == TEXT('\r'))
        {
            Advance(At + 1);
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
            Token.Atom.Line = Line;
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
                Advance(Close + 1);
                continue;
            }
            const bool bBool = Atom.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Atom.Equals(TEXT("false"), ESearchCase::IgnoreCase);
            Token.Atom.Type = bBool ? FSExpr::EType::Bool : NumberType(Atom);
            Token.Atom.bTrue = Atom.Equals(TEXT("true"), ESearchCase::IgnoreCase);
            Token.Atom.Text = Atom;
            Advance(End);
        }
    }
    return true;
}

bool IsClose(const FToken& Token)
{
    return Token.bParen && Token.Atom.Text == TEXT(")");
}

bool ReadForm(const TArray<FToken>& Tokens, int32& Pos, TMap<int32, FString>& Tags, FSExpr& Out, FString& OutError, int32 Depth = 0)
{
    if (!Tokens.IsValidIndex(Pos) || IsClose(Tokens[Pos]))
    {
        OutError = Tokens.IsValidIndex(Pos) ? FString::Printf(TEXT("Line %d: unexpected )"), Tokens[Pos].Atom.Line) : FString(TEXT("Unexpected end of input"));
        return false;
    }
    if (Tokens[Pos].bParen && Depth >= MaxDepth)
    {
        OutError = FString::Printf(TEXT("Line %d: lists nested deeper than %d"), Tokens[Pos].Atom.Line, MaxDepth);
        return false;
    }
    if (!Tokens[Pos].bParen)
    {
        Out = Tokens[Pos++].Atom;
        return true;
    }
    Out.Type = FSExpr::EType::List;
    Out.Line = Tokens[Pos++].Atom.Line;
    // The line's tag goes to the first list opened on it: the statement, not a call inside it.
    if (const FString* Tag = Tags.Find(Out.Line))
    {
        Out.Tag = *Tag;
        Tags.Remove(Out.Line);
    }
    while (Tokens.IsValidIndex(Pos))
    {
        if (IsClose(Tokens[Pos]))
        {
            ++Pos;
            return true;
        }
        if (!ReadForm(Tokens, Pos, Tags, Out.Items.AddDefaulted_GetRef(), OutError, Depth + 1))
        {
            return false;
        }
    }
    OutError = FString::Printf(TEXT("Line %d: unclosed parenthesis"), Out.Line);
    return false;
}
} // namespace ParseParts

bool ParseSExpr(const FString& Text, TArray<FSExpr>& OutForms, FString& OutError)
{
    TArray<ParseParts::FToken> Tokens;
    TMap<int32, FString> Tags;
    if (!ParseParts::Tokenize(Text, Tokens, Tags, OutError))
    {
        return false;
    }
    for (int32 Pos = 0; Pos < Tokens.Num();)
    {
        if (!ParseParts::ReadForm(Tokens, Pos, Tags, OutForms.AddDefaulted_GetRef(), OutError))
        {
            return false;
        }
    }
    return true;
}
} // namespace McpGraphCode
