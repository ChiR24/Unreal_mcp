#include "Domains/Scripts/McpScript.h"

#include "Domains/Debug/Contracts/McpContracts.h"
#include "Editor.h"
#include "Foundation/Solvers/McpPratt.h"
#include "HAL/PlatformTime.h"

namespace McpScript
{
using McpGraphCode::FSExpr;

namespace ScriptRunParts
{
constexpr int32 MaxRange = 1000;

UWorld* JudgedWorld()
{
    return GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
}
} // namespace ScriptRunParts

using namespace ScriptRunParts;

bool FRunner::Fail(int32 Line, const FString& Code, const FString& Message)
{
    ErrorCode = Code;
    Error = Line > 0 ? FString::Printf(TEXT("Line %d: %s"), Line, *Message) : Message;
    return false;
}

bool FRunner::Run(const TArray<FSExpr>& Forms)
{
    return Stmts(Forms, 0, FNames());
}

bool FRunner::Stmts(const TArray<FSExpr>& Body, int32 From, FNames Names)
{
    for (int32 At = From; At < Body.Num() && !bReturned; ++At)
    {
        // Every statement, not only every call: loops of plain statements must not hold the game thread past it either.
        if (FPlatformTime::Seconds() > Limits.DeadlineSeconds)
        {
            return Fail(Body[At].Line, TEXT("SCRIPT_DEADLINE"), TEXT("the script ran past its deadline (deadlineMs); nothing further ran."));
        }
        if (!Stmt(Body[At], Names))
        {
            return false;
        }
    }
    return true;
}

// The forms McpScriptCheck.cpp accepted: their shapes are not checked again here.
bool FRunner::Stmt(const FSExpr& Form, FNames& Names)
{
    TSharedPtr<FJsonValue> Value;
    if (HeadIs(Form, TEXT("let")))
    {
        if (!Eval(Form.Items[2], Names, Value))
        {
            return false;
        }
        Names.Add(Form.Items[1].Text, Value);
        return true;
    }
    if (HeadIs(Form, TEXT("for")))
    {
        if (!Eval(Form.Items[2], Names, Value))
        {
            return false;
        }
        if (!Value.IsValid() || Value->Type != EJson::Array)
        {
            return Fail(Form.Line, TEXT("SCRIPT_TYPE"), FString::Printf(TEXT("(for %s ...) goes over a list, and this value is not one."), *Form.Items[1].Text));
        }
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            FNames Inner = Names;
            Inner.Add(Form.Items[1].Text, Item);
            if (!Stmts(Form.Items, 3, Inner))
            {
                return false;
            }
            if (bReturned)
            {
                break;
            }
        }
        return true;
    }
    if (HeadIs(Form, TEXT("if")))
    {
        if (!Eval(Form.Items[1], Names, Value))
        {
            return false;
        }
        const bool bElse = Form.Items.Num() > 3 && HeadIs(Form.Items.Last(), TEXT("else"));
        if (IsTrue(Value))
        {
            return Stmts(TArray<FSExpr>(Form.Items.GetData(), Form.Items.Num() - (bElse ? 1 : 0)), 2, Names);
        }
        return !bElse || Stmts(Form.Items.Last().Items, 1, Names);
    }
    if (HeadIs(Form, TEXT("expect")))
    {
        // Judged now, in execute's expect language; anything but ok stops the script, which then rolls back.
        TSharedPtr<McpPratt::FNode> Root;
        FString Why;
        if (!Eval(Form.Items[1], Names, Value))
        {
            return false;
        }
        const FString Assertion = Value.IsValid() && Value->Type == EJson::String ? Value->AsString() : FString();
        if (Assertion.IsEmpty() || !McpPratt::Parse(Assertion, Root, Why))
        {
            return Fail(Form.Line, TEXT("SCRIPT_INVALID"), Assertion.IsEmpty() ? FString(TEXT("(expect ...) takes assertion text.")) : FString::Printf(TEXT("'%s': %s"), *Assertion, *Why));
        }
        const McpContracts::FVerdict Verdict = McpContracts::Judge(Root, JudgedWorld(), nullptr);
        return Verdict.Verdict == TEXT("ok") ||
               Fail(Form.Line, TEXT("EXPECTATION_FAILED"), FString::Printf(TEXT("expected '%s': %s (%s)."), *Assertion, *Verdict.Verdict, *Verdict.Actual));
    }
    if (HeadIs(Form, TEXT("return")))
    {
        if (Form.Items.Num() == 2 && !Eval(Form.Items[1], Names, Value))
        {
            return false;
        }
        Returned = Value.IsValid() ? Value : MakeShared<FJsonValueNull>();
        bReturned = true;
        return true;
    }
    return Eval(Form, Names, Value);
}

bool FRunner::Pairs(const FSExpr& Form, const FNames& Names, TSharedPtr<FJsonObject>& Out)
{
    Out = MakeShared<FJsonObject>();
    for (int32 At = 1; At + 1 < Form.Items.Num(); At += 2)
    {
        TSharedPtr<FJsonValue> Value;
        if (!Eval(Form.Items[At + 1], Names, Value))
        {
            return false;
        }
        Out->SetField(Form.Items[At].Text.Mid(1), Value);
    }
    return true;
}

bool FRunner::Eval(const FSExpr& Expr, const FNames& Names, TSharedPtr<FJsonValue>& Out)
{
    if (Expr.Type != FSExpr::EType::List)
    {
        FString Why;
        return EvalAtom(Expr, Names, Out, Why) || Fail(Expr.Line, TEXT("SCRIPT_VALUE"), Why);
    }
    if (IsCall(Expr))
    {
        return Call(Expr, Names, Out);
    }
    if (Expr.Items.Num() == 0 || Expr.Items[0].Type != FSExpr::EType::Symbol)
    {
        return Fail(Expr.Line, TEXT("SCRIPT_INVALID"), TEXT("a list that is not a call or a form is not an expression."));
    }
    TArray<TSharedPtr<FJsonValue>> Args;
    const bool bShortCircuit = HeadIs(Expr, TEXT("and")) || HeadIs(Expr, TEXT("or"));
    for (int32 At = 1; At < Expr.Items.Num(); ++At)
    {
        // (and a b) / (or a b) stop at the first value that decides them.
        if (bShortCircuit && At == 2 && IsTrue(Args[0]) != HeadIs(Expr, TEXT("and")))
        {
            Out = MakeShared<FJsonValueBoolean>(IsTrue(Args[0]));
            return true;
        }
        if (!(HeadIs(Expr, TEXT("obj")) && At % 2 == 1) && !Eval(Expr.Items[At], Names, Args.AddDefaulted_GetRef()))
        {
            return false;
        }
    }
    const FString& Head = Expr.Items[0].Text;
    if (bShortCircuit || HeadIs(Expr, TEXT("not")))
    {
        Out = MakeShared<FJsonValueBoolean>(bShortCircuit ? IsTrue(Args.Last()) : !IsTrue(Args[0]));
        return true;
    }
    if (HeadIs(Expr, TEXT("list")))
    {
        Out = MakeShared<FJsonValueArray>(Args);
        return true;
    }
    if (HeadIs(Expr, TEXT("obj")))
    {
        TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
        for (int32 Index = 0; Index < Args.Num(); ++Index)
        {
            Object->SetField(Expr.Items[1 + Index * 2].Text.Mid(1), Args[Index]);
        }
        Out = MakeShared<FJsonValueObject>(Object);
        return true;
    }
    if (HeadIs(Expr, TEXT("range")))
    {
        double From = 0.0;
        double To = 0.0;
        const bool bNumbers = Args.Num() == 1 ? Args[0]->TryGetNumber(To) : Args[0]->TryGetNumber(From) && Args[1]->TryGetNumber(To);
        if (!bNumbers || To - From > MaxRange)
        {
            return Fail(Expr.Line, TEXT("SCRIPT_TYPE"), FString::Printf(TEXT("(range n) or (range from to) takes whole numbers, at most %d apart."), MaxRange));
        }
        TArray<TSharedPtr<FJsonValue>> Items;
        for (double Value = FMath::CeilToDouble(From); Value < To; Value += 1.0)
        {
            Items.Add(MakeShared<FJsonValueNumber>(Value));
        }
        Out = MakeShared<FJsonValueArray>(Items);
        return true;
    }
    if (!IsOperator(Expr) || Args.Num() != 2)
    {
        return Fail(Expr.Line, TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("(%s ...) is not an expression."), *Head));
    }
    Out = Apply(Head, Args[0], Args[1]);
    return Out.IsValid() || Fail(Expr.Line, TEXT("SCRIPT_TYPE"), FString::Printf(TEXT("(%s a b) takes two numbers here (+ also joins text; / never divides by 0)."), *Head));
}
} // namespace McpScript
