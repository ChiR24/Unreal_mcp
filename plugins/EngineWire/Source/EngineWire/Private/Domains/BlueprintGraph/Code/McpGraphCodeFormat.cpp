#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"

namespace McpGraphCode
{
namespace Emit
{
namespace FormatTables
{
struct FOp
{
    const TCHAR* TypeId;
    const TCHAR* Op;
};
// Epic's _TYPE_ID_TO_OP: the operator nodes, and the typed nodes a wildcard operator becomes.
const FOp Ops[] = {
    {TEXT("Utilities|Operators|Add"), TEXT("+")}, {TEXT("Utilities|Operators|Subtract"), TEXT("-")},
    {TEXT("Utilities|Operators|Multiply"), TEXT("*")}, {TEXT("Utilities|Operators|Divide"), TEXT("/")},
    {TEXT("Utilities|Operators|Modulo"), TEXT("%")}, {TEXT("Math|Boolean|XORBoolean"), TEXT("xor")},
    {TEXT("Utilities|Operators|Equal(==)"), TEXT("==")}, {TEXT("Utilities|Operators|NotEqual(!=)"), TEXT("!=")},
    {TEXT("Utilities|Operators|Less(<)"), TEXT("<")}, {TEXT("Utilities|Operators|LessEqual(<=)"), TEXT("<=")},
    {TEXT("Utilities|Operators|Greater(>)"), TEXT(">")}, {TEXT("Utilities|Operators|GreaterEqual(>=)"), TEXT(">=")},
    {TEXT("Math|Boolean|ANDBoolean"), TEXT("and")}, {TEXT("Math|Boolean|ORBoolean"), TEXT("or")},
    {TEXT("Math|Integer|int+int"), TEXT("+")}, {TEXT("Math|Integer|int-int"), TEXT("-")},
    {TEXT("Math|Integer|int*int"), TEXT("*")}, {TEXT("Math|Integer|int/int"), TEXT("/")},
    {TEXT("Math|Integer|int%int"), TEXT("%")}, {TEXT("Math|Integer|integer==integer"), TEXT("==")},
    {TEXT("Math|Integer|integer!=integer"), TEXT("!=")}, {TEXT("Math|Integer|integer<integer"), TEXT("<")},
    {TEXT("Math|Integer|integer<=integer"), TEXT("<=")}, {TEXT("Math|Integer|integer>integer"), TEXT(">")},
    {TEXT("Math|Integer|integer>=integer"), TEXT(">=")}, {TEXT("Math|Float|float+float"), TEXT("+")},
    {TEXT("Math|Float|float-float"), TEXT("-")}, {TEXT("Math|Float|float*float"), TEXT("*")},
    {TEXT("Math|Float|float/float"), TEXT("/")}, {TEXT("Math|Float|float<float"), TEXT("<")},
    {TEXT("Math|Float|float<=float"), TEXT("<=")}, {TEXT("Math|Float|float>float"), TEXT(">")},
    {TEXT("Math|Float|float>=float"), TEXT(">=")}, {TEXT("Math|Integer|Equal(Integer)"), TEXT("==")},
    {TEXT("Math|Integer|NotEqual(Integer)"), TEXT("!=")}, {TEXT("Math|Integer|Less(Integer)"), TEXT("<")},
    {TEXT("Math|Integer|LessEqual(Integer)"), TEXT("<=")}, {TEXT("Math|Integer|Greater(Integer)"), TEXT(">")},
    {TEXT("Math|Integer|GreaterEqual(Integer)"), TEXT(">=")}, {TEXT("Math|Float|Equal(Float)"), TEXT("==")},
    {TEXT("Math|Float|NotEqual(Float)"), TEXT("!=")}, {TEXT("Math|Float|Less(Float)"), TEXT("<")},
    {TEXT("Math|Float|LessEqual(Float)"), TEXT("<=")}, {TEXT("Math|Float|Greater(Float)"), TEXT(">")},
    {TEXT("Math|Float|GreaterEqual(Float)"), TEXT(">=")}};
// Make Literal nodes collapse to their value.
const TCHAR* const Literals[] = {TEXT("Math|Boolean|MakeLiteralBool"), TEXT("Math|Integer|MakeLiteralInt"),
                                 TEXT("Math|Float|MakeLiteralFloat"), TEXT("Utilities|String|MakeLiteralString")};
struct FComponent
{
    const TCHAR* TypeId;
    const TCHAR* Pin;
    const TCHAR* Attr;
};
const FComponent Components[] = {
    {TEXT("Math|Vector|BreakVector"), TEXT("X"), TEXT("x")}, {TEXT("Math|Vector|BreakVector"), TEXT("Y"), TEXT("y")},
    {TEXT("Math|Vector|BreakVector"), TEXT("Z"), TEXT("z")}, {TEXT("Math|Rotator|BreakRotator"), TEXT("Pitch"), TEXT("pitch")},
    {TEXT("Math|Rotator|BreakRotator"), TEXT("Yaw"), TEXT("yaw")}, {TEXT("Math|Rotator|BreakRotator"), TEXT("Roll"), TEXT("roll")},
    {TEXT("Math|Transform|BreakTransform"), TEXT("Location"), TEXT("location")},
    {TEXT("Math|Transform|BreakTransform"), TEXT("Rotation"), TEXT("rotation")},
    {TEXT("Math|Transform|BreakTransform"), TEXT("Scale"), TEXT("scale")}};

// What Python's int() or float() reads, as Epic's _format_pin_default asks: [sign] digits [.digits] [e[sign]digits].
bool IsNumeric(const FString& Value)
{
    const FString Text = Value.TrimStartAndEnd();
    int32 At = Text.Len() > 0 && (Text[0] == TEXT('+') || Text[0] == TEXT('-')) ? 1 : 0;
    const FString Word = Text.Mid(At).ToLower();
    if (Word == TEXT("inf") || Word == TEXT("infinity") || Word == TEXT("nan"))
    {
        return true;
    }
    bool bDigits = false;
    bool bDot = false;
    for (; At < Text.Len() && (FChar::IsDigit(Text[At]) || (Text[At] == TEXT('.') && !bDot)); ++At)
    {
        bDigits |= FChar::IsDigit(Text[At]);
        bDot |= Text[At] == TEXT('.');
    }
    if (bDigits && At < Text.Len() && (Text[At] == TEXT('e') || Text[At] == TEXT('E')))
    {
        At += At + 1 < Text.Len() && (Text[At + 1] == TEXT('+') || Text[At + 1] == TEXT('-')) ? 2 : 1;
        const int32 Start = At;
        while (At < Text.Len() && FChar::IsDigit(Text[At]))
        {
            ++At;
        }
        bDigits = At > Start;
    }
    return bDigits && At == Text.Len();
}

// Backslashes are escaped as well as quotes, so Epic's tokenizer reads the value back as it was.
FString Quote(const FString& Value)
{
    return TEXT("\"") + Value.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
}
} // namespace FormatTables

using namespace FormatTables;

bool SameText(const FString& A, const TCHAR* B)
{
    return A.Equals(B, ESearchCase::CaseSensitive);
}

const TCHAR* OperatorFor(const FString& TypeId)
{
    for (const FOp& Entry : Ops)
    {
        if (SameText(TypeId, Entry.TypeId))
        {
            return Entry.Op;
        }
    }
    return nullptr;
}

bool IsMakeLiteral(const FString& TypeId)
{
    for (const TCHAR* Literal : Literals)
    {
        if (SameText(TypeId, Literal))
        {
            return true;
        }
    }
    return false;
}

const TCHAR* ComponentFor(const FString& TypeId, const FString& PinName)
{
    for (const FComponent& Entry : Components)
    {
        if (SameText(TypeId, Entry.TypeId) && SameText(PinName, Entry.Pin))
        {
            return Entry.Attr;
        }
    }
    return nullptr;
}

FString FormatDefault(const FPin& Pin)
{
    if (Pin.bBool)
    {
        return Pin.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? TEXT("true") : TEXT("false");
    }
    if (!Pin.bText && Pin.Value.IsEmpty())
    {
        return TEXT("0");
    }
    return !Pin.bText && IsNumeric(Pin.Value) ? Pin.Value : Quote(Pin.Value);
}

FString FormatPinName(const FString& Name)
{
    for (const TCHAR Special : {TEXT(' '), TEXT('\t'), TEXT('\n'), TEXT('\r'), TEXT(';'), TEXT('"'), TEXT('('), TEXT(')')})
    {
        int32 Unused = 0;
        if (Name.FindChar(Special, Unused))
        {
            return Quote(Name);
        }
    }
    return Name;
}

FString InvertExpr(const FString& Expr)
{
    return Expr.StartsWith(TEXT("(not "), ESearchCase::CaseSensitive) && Expr.EndsWith(TEXT(")"))
        ? Expr.Mid(5, Expr.Len() - 6)
        : TEXT("(not ") + Expr + TEXT(")");
}

// Blueprint's LastIndex is inclusive, the text's range stop exclusive.
FString IncrementExpr(const FString& Expr)
{
    const FString Text = Expr.TrimStartAndEnd();
    const int32 Start = Text.Len() > 0 && (Text[0] == TEXT('+') || Text[0] == TEXT('-')) ? 1 : 0;
    bool bInteger = Text.Len() > Start;
    for (int32 At = Start; At < Text.Len(); ++At)
    {
        bInteger &= FChar::IsDigit(Text[At]);
    }
    return bInteger ? LexToString(FCString::Atoi64(*Text) + 1) : TEXT("(+ ") + Expr + TEXT(" 1)");
}

FString SanitizePinName(const FString& Name)
{
    FString Out = Name;
    for (TCHAR& Ch : Out.GetCharArray())
    {
        const bool bAscii = (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
        Ch = Ch == 0 || bAscii ? Ch : TEXT('_');
    }
    while (Out.StartsWith(TEXT("_")))
    {
        Out.RightChopInline(1);
    }
    while (Out.EndsWith(TEXT("_")))
    {
        Out.LeftChopInline(1);
    }
    return Out.IsEmpty() ? FString(TEXT("tmp")) : Out.ToLower();
}

bool IsDataOut(const FPin& Pin)
{
    return !Pin.bExec && !SameText(Pin.Name, TEXT("OutputDelegate"));
}

const FPin* FindPinNamed(const TArray<FPin>& Pins, const TCHAR* Name)
{
    return Pins.FindByPredicate([Name](const FPin& Pin) { return SameText(Pin.Name, Name); });
}

TArray<const FPin*> DataIns(const FNode& Node)
{
    TArray<const FPin*> Ins;
    for (const FPin& Pin : Node.Ins)
    {
        if (!Pin.bExec)
        {
            Ins.Add(&Pin);
        }
    }
    return Ins;
}

TArray<FForm> OrPlaceholder(TArray<FForm> Lines, const FState& State)
{
    if (Lines.Num() == 0)
    {
        Lines.Add(FForm{State.Pad() + TEXT("_"), FString(), {}, 0});
    }
    return Lines;
}

void FForm::Render(TArray<FLine>& Out) const
{
    Out.Add({Head, Tag});
    for (const FForm& Child : Children)
    {
        Child.Render(Out);
    }
    Out.Last().Text += FString::ChrN(Close, TEXT(')'));
}
} // namespace Emit
} // namespace McpGraphCode
