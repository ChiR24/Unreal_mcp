// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpPratt.h"

#include "Algo/AllOf.h"
#include "Containers/ArrayView.h"
#include "Foundation/Solvers/McpPrattLex.h"

namespace McpPratt
{
namespace
{
using Detail::EKind;
using Detail::FToken;
using Detail::IsNameChar;

const TCHAR* const Relations[] = {TEXT("on"), TEXT("inside"), TEXT("nextTo"), TEXT("against"), TEXT("facing"), TEXT("near")};
const TCHAR* const Predicates[] = {TEXT("exists"), TEXT("hidden"), TEXT("visible"), TEXT("compiles")};
const TCHAR* const Comparisons[] = {TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">="), TEXT("<"), TEXT(">")};

class FParser
{
public:
    TArray<FToken> Tokens;
    int32 Pos = 0;
    FString Error;

    // or binds loosest, then and; both group left to right.
    TSharedPtr<FNode> Expression(int32 MinPower)
    {
        TSharedPtr<FNode> Left = Unary();
        for (int32 Power = Word(TEXT("or")) ? 1 : Word(TEXT("and")) ? 2 : 0; Left && Power >= FMath::Max(MinPower, 1);
             Power = Word(TEXT("or")) ? 1 : Word(TEXT("and")) ? 2 : 0)
        {
            const FString Op = Power == 1 ? TEXT("or") : TEXT("and");
            ++Pos;
            TSharedPtr<FNode> Right = Expression(Power + 1);
            Left = Right ? Node(Op, {Left, Right}) : nullptr;
        }
        return Left;
    }

    // The end token once a failed parse has stepped past it.
    const FToken& Peek() const { return Tokens[FMath::Min(Pos, Tokens.Num() - 1)]; }
    bool Word(const TCHAR* Text) const { return Peek().Kind == EKind::Name && Peek().Value.Equals(Text, ESearchCase::IgnoreCase); }
    bool Symbol(const TCHAR* Text) const { return Peek().Kind == EKind::Symbol && Peek().Value == Text; }

    TSharedPtr<FNode> Fail(const FString& Expected)
    {
        if (Error.IsEmpty())
        {
            const FToken& Token = Peek();
            Error = FString::Printf(TEXT("expected %s at character %d, found %s"), *Expected, Token.At + 1,
                                    Token.Kind == EKind::End ? TEXT("the end") : *FString::Printf(TEXT("'%s'"), *Token.Value));
        }
        return nullptr;
    }

    static TSharedPtr<FNode> Node(const FString& Op, const TArray<TSharedPtr<FNode>>& Args)
    {
        TSharedPtr<FNode> Out = MakeShared<FNode>();
        Out->Op = Op;
        Out->Args = Args;
        return Out;
    }

private:
    TSharedPtr<FNode> Unary()
    {
        if (Word(TEXT("not")))
        {
            ++Pos;
            TSharedPtr<FNode> Inner = Unary();
            return Inner ? Node(TEXT("not"), {Inner}) : nullptr;
        }
        if (Symbol(TEXT("(")))
        {
            ++Pos;
            TSharedPtr<FNode> Inner = Expression(0);
            if (Inner && !Symbol(TEXT(")")))
            {
                return Fail(TEXT("')'"));
            }
            Pos += Inner ? 1 : 0;
            return Inner;
        }
        return Clause();
    }

    // A relation, comparison or predicate over operands, or noWarnings.
    TSharedPtr<FNode> Clause()
    {
        if (Word(TEXT("noWarnings")))
        {
            ++Pos;
            return Node(TEXT("noWarnings"), {});
        }
        TSharedPtr<FNode> Subject = Operand();
        if (!Subject)
        {
            return nullptr;
        }
        // The canonical spelling of the keyword or symbol next, when it is one of Words.
        const auto Keyword = [this](TArrayView<const TCHAR* const> Words) -> const TCHAR* {
            for (const TCHAR* Candidate : Words)
            {
                if (Word(Candidate) || Symbol(Candidate))
                {
                    return Candidate;
                }
            }
            return nullptr;
        };
        if (const TCHAR* Relation = Keyword(MakeArrayView(Relations)))
        {
            ++Pos;
            TSharedPtr<FNode> Object = Operand();
            TSharedPtr<FNode> Out = Object ? Node(Relation, {Subject, Object}) : nullptr;
            if (Out && Word(TEXT("within")))
            {
                ++Pos;
                TSharedPtr<FNode> Distance = Number();
                Out = Distance ? Node(Relation, {Subject, Object, Distance}) : nullptr;
            }
            return Out;
        }
        if (const TCHAR* Predicate = Keyword(MakeArrayView(Predicates)))
        {
            ++Pos;
            return Node(Predicate, {Subject});
        }
        if (Word(TEXT("in")))
        {
            ++Pos;
            TSharedPtr<FNode> Low = Number();
            if (Low && !Symbol(TEXT("..")))
            {
                return Fail(TEXT("'..' between the two ends of the range"));
            }
            Pos += Low ? 1 : 0;
            TSharedPtr<FNode> High = Low ? Number() : nullptr;
            return High ? Node(TEXT("in"), {Subject, Low, High}) : nullptr;
        }
        if (const TCHAR* Comparison = Keyword(MakeArrayView(Comparisons)))
        {
            ++Pos;
            TSharedPtr<FNode> Value = Operand();
            return Value ? Node(Comparison, {Subject, Value}) : nullptr;
        }
        return Fail(TEXT("a relation (on, inside, nextTo, facing, near), a comparison (== != < <= > >= in) or a predicate (exists, "
                         "hidden, visible, compiles)"));
    }

    TSharedPtr<FNode> Number()
    {
        if (Peek().Kind != EKind::Number)
        {
            return Fail(TEXT("a number"));
        }
        TSharedPtr<FNode> Out = MakeShared<FNode>();
        Out->bNumber = true;
        Out->Number = Tokens[Pos++].Number;
        return Out;
    }

    // A name or a dotted path (Lamp.LightComponent.Intensity), or a number.
    TSharedPtr<FNode> Operand()
    {
        if (Peek().Kind == EKind::Number)
        {
            return Number();
        }
        TSharedPtr<FNode> Out = MakeShared<FNode>();
        do
        {
            Pos += Out->Path.Num() > 0 ? 1 : 0; // the dot
            if (Peek().Kind != EKind::Name && Peek().Kind != EKind::Quoted)
            {
                return Fail(Out->Path.Num() > 0 ? TEXT("a name after '.'") : TEXT("a name"));
            }
            Out->Path.Add(Tokens[Pos++].Value);
        } while (Symbol(TEXT(".")));
        return Out;
    }
};

FString Segment(const FString& Text)
{
    const bool bPlain = Text.Len() > 0 && IsNameChar(Text[0], true) &&
        Algo::AllOf(Text, [](TCHAR Ch) { return IsNameChar(Ch, false); });
    return bPlain ? Text : FString::Printf(TEXT("\"%s\""), *Text.Replace(TEXT("\""), TEXT("\\\"")));
}
} // namespace

bool Parse(const FString& Source, TSharedPtr<FNode>& OutRoot, FString& OutError)
{
    FParser Parser;
    if (!Detail::Lex(Source, Parser.Tokens, OutError))
    {
        return false;
    }
    OutRoot = Parser.Expression(0);
    if (OutRoot && Parser.Peek().Kind != EKind::End)
    {
        OutRoot = Parser.Fail(TEXT("and, or or the end"));
    }
    OutError = Parser.Error;
    return OutRoot.IsValid();
}

FString Print(const TSharedPtr<FNode>& Node)
{
    if (!Node.IsValid())
    {
        return FString();
    }
    if (Node->Op.IsEmpty())
    {
        if (Node->bNumber)
        {
            return FString::Printf(TEXT("%g"), Node->Number);
        }
        TArray<FString> Parts;
        for (const FString& Part : Node->Path)
        {
            Parts.Add(Segment(Part));
        }
        return FString::Join(Parts, TEXT("."));
    }
    TArray<FString> Parts = {Node->Op};
    for (const TSharedPtr<FNode>& Arg : Node->Args)
    {
        Parts.Add(Print(Arg));
    }
    return FString::Printf(TEXT("(%s)"), *FString::Join(Parts, TEXT(" ")));
}
} // namespace McpPratt
