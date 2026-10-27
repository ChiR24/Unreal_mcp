#include "Domains/Scripts/McpScript.h"

#include "Foundation/Solvers/McpPratt.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

namespace McpScript
{
using McpGraphCode::FSExpr;

namespace ScriptCheckParts
{
const TCHAR* const Expressions = TEXT("a call (capability.id :param value ...), (not a), (and a b), (or a b), a comparison "
                                      "(== a b) or (< a b), arithmetic (+ a b), (range n), (list ...), (obj :key value ...), "
                                      "a literal or a name");

bool IsKey(const FSExpr& Item)
{
    return Item.Type == FSExpr::EType::Symbol && Item.Text.StartsWith(TEXT(":")) && Item.Text.Len() > 1;
}

bool IsPlainName(const FSExpr& Item)
{
    return Item.Type == FSExpr::EType::Symbol && !Item.Text.IsEmpty() && !Item.Text.Contains(TEXT(".")) &&
           !Item.Text.StartsWith(TEXT(":")) && Item.Text != TEXT("null");
}

TArray<FString> ParamNames(const TSharedPtr<FJsonObject>* Properties)
{
    TArray<FString> Names;
    if (!Properties)
    {
        return Names;
    }
    for (const auto& Property : (*Properties)->Values)
    {
        const FString Name(*Property.Key); // keyed by FString before 5.8, a shared string from 5.8
        if (Name != TEXT("action"))
        {
            Names.Add(Name);
        }
    }
    Names.Sort();
    return Names;
}

struct FChecker
{
    const TSet<FString>& Allowed;
    FString& Error;
    FString& Code;

    bool Fail(int32 Line, const TCHAR* InCode, const FString& Message)
    {
        Code = InCode;
        Error = FString::Printf(TEXT("Line %d: %s"), Line, *Message);
        return false;
    }

    bool Stmts(const TArray<FSExpr>& Body, int32 From, TSet<FString> Bound)
    {
        for (int32 At = From; At < Body.Num(); ++At)
        {
            if (!Stmt(Body[At], Bound))
            {
                return false;
            }
        }
        return true;
    }

    bool Name(const FSExpr& Symbol, const TSet<FString>& Bound)
    {
        FString Root = Symbol.Text;
        Symbol.Text.Split(TEXT("."), &Root, nullptr);
        if (Symbol.Text == TEXT("null") || Bound.Contains(Root))
        {
            return true;
        }
        if (IsKey(Symbol))
        {
            return Fail(Symbol.Line, TEXT("SCRIPT_INVALID"),
                        FString::Printf(TEXT("%s names a parameter: it goes before its value in a call or (obj ...)."), *Symbol.Text));
        }
        TArray<FString> Known = Bound.Array();
        Known.Sort();
        return Fail(Symbol.Line, TEXT("SCRIPT_UNBOUND_NAME"), FString::Printf(TEXT("%s is not bound here; bound: %s."), *Root,
                    Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("nothing yet")));
    }

    bool CallForm(const FSExpr& Form, const TSet<FString>& Bound);
    bool Expr(const FSExpr& Form, const TSet<FString>& Bound);
    bool Stmt(const FSExpr& Form, TSet<FString>& Bound);
};

bool FChecker::CallForm(const FSExpr& Form, const TSet<FString>& Bound)
{
    const FString& Head = Form.Items[0].Text;
    const FString Id = ResolveCapability(Head);
    const FMcpCapabilityRecord* Record = Id.IsEmpty() ? nullptr : FMcpCanonicalRecordIndex::Get().FindById(Id);
    if (!Record)
    {
        return Fail(Form.Line, TEXT("UNKNOWN_CAPABILITY"), FString::Printf(TEXT("%s is no capability; search finds the id a step names."), *Head));
    }
    FString StepCode;
    const FString Why = StepRefusal(Id, Allowed, StepCode);
    if (!Why.IsEmpty())
    {
        return Fail(Form.Line, *StepCode, Why);
    }
    const TSharedPtr<FJsonObject>* Properties = nullptr;
    if (Record->InputSchema.IsValid())
    {
        Record->InputSchema->TryGetObjectField(TEXT("properties"), Properties);
    }
    TSet<FString> Sent;
    for (int32 At = 1; At < Form.Items.Num(); At += 2)
    {
        const FSExpr& Key = Form.Items[At];
        const FString Param = Key.Text.Mid(1);
        if (!IsKey(Key) || At + 1 >= Form.Items.Num())
        {
            return Fail(Key.Line, TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("(%s :param value ...) takes :param and value pairs."), *Head));
        }
        if (Param == TEXT("action") || Param == TEXT("subAction") || !Properties || !(*Properties)->HasField(Param))
        {
            return Fail(Key.Line, TEXT("UNDECLARED_PARAMETER"), FString::Printf(TEXT("%s takes no parameter %s; it takes: %s."), *Id, *Param,
                        *FString::Join(ParamNames(Properties), TEXT(", "))));
        }
        if (Sent.Contains(Param))
        {
            return Fail(Key.Line, TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("%s is given twice."), *Key.Text));
        }
        Sent.Add(Param);
        if (!Expr(Form.Items[At + 1], Bound))
        {
            return false;
        }
    }
    // A folded family's selector is left to the gateway: an old name pins it (find_by_name: findBy name), and the
    // parameters sent can imply it.
    const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
    if (Record->InputSchema.IsValid() && Record->InputSchema->TryGetArrayField(TEXT("required"), Required))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Required)
        {
            const FString Param = Value.IsValid() ? Value->AsString() : FString();
            if (!Param.IsEmpty() && Param != TEXT("action") && Param != Record->DispatchBySelector && !Sent.Contains(Param))
            {
                return Fail(Form.Line, TEXT("MISSING_REQUIRED_PARAMETER"), FString::Printf(TEXT("%s needs :%s."), *Id, *Param));
            }
        }
    }
    return true;
}

bool FChecker::Expr(const FSExpr& Form, const TSet<FString>& Bound)
{
    if (Form.Type == FSExpr::EType::Symbol)
    {
        return Name(Form, Bound);
    }
    if (Form.Type != FSExpr::EType::List)
    {
        return true; // a literal
    }
    if (IsCall(Form))
    {
        return CallForm(Form, Bound);
    }
    const int32 Args = Form.Items.Num() - 1;
    const bool bObj = HeadIs(Form, TEXT("obj"));
    const bool bShape = HeadIs(Form, TEXT("not")) ? Args == 1
                      : (HeadIs(Form, TEXT("and")) || HeadIs(Form, TEXT("or")) || IsOperator(Form)) ? Args == 2
                      : HeadIs(Form, TEXT("range")) ? (Args == 1 || Args == 2)
                      : bObj ? Args % 2 == 0
                      : HeadIs(Form, TEXT("list"));
    if (!bShape)
    {
        const FString Head = Form.Items.Num() > 0 && Form.Items[0].Type == FSExpr::EType::Symbol ? Form.Items[0].Text : FString(TEXT("()"));
        return Fail(Form.Line, TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("(%s ...) is not an expression with %d argument(s); an expression is %s."),
                    *Head, Args, Expressions));
    }
    for (int32 At = 1; At < Form.Items.Num(); ++At)
    {
        const bool bKey = bObj && At % 2 == 1;
        if (bKey ? !IsKey(Form.Items[At]) : !Expr(Form.Items[At], Bound))
        {
            return bKey ? Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(obj :key value ...) takes :key and value pairs.")) : false;
        }
    }
    return true;
}

bool FChecker::Stmt(const FSExpr& Form, TSet<FString>& Bound)
{
    if (HeadIs(Form, TEXT("let")))
    {
        if (Form.Items.Num() != 3 || !IsPlainName(Form.Items[1]))
        {
            return Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(let name expr) binds one plain name (no dots) to a value."));
        }
        if (!Expr(Form.Items[2], Bound))
        {
            return false;
        }
        Bound.Add(Form.Items[1].Text);
        return true;
    }
    if (HeadIs(Form, TEXT("for")))
    {
        if (Form.Items.Num() < 4 || !IsPlainName(Form.Items[1]))
        {
            return Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(for item list stmt ...) goes over a list with a plain name and a statement."));
        }
        TSet<FString> Inner = Bound;
        Inner.Add(Form.Items[1].Text);
        return Expr(Form.Items[2], Bound) && Stmts(Form.Items, 3, Inner);
    }
    if (HeadIs(Form, TEXT("if")))
    {
        const bool bElse = Form.Items.Num() > 3 && HeadIs(Form.Items.Last(), TEXT("else"));
        if (Form.Items.Num() < 3 || HeadIs(Form.Items[2], TEXT("else")))
        {
            return Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(if cond stmt ... (else stmt ...)) needs a condition and a statement."));
        }
        const TArray<FSExpr> Then(Form.Items.GetData(), Form.Items.Num() - (bElse ? 1 : 0));
        return Expr(Form.Items[1], Bound) && Stmts(Then, 2, Bound) && (!bElse || Stmts(Form.Items.Last().Items, 1, Bound));
    }
    if (HeadIs(Form, TEXT("expect")))
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Why;
        if (Form.Items.Num() != 2)
        {
            return Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(expect \"assertion\") takes one assertion text, in execute's expect language."));
        }
        // Text made while running ((+ c.label " on Floor")) is parsed when it runs.
        if (Form.Items[1].Type != FSExpr::EType::String)
        {
            return Expr(Form.Items[1], Bound);
        }
        return McpPratt::Parse(Form.Items[1].Text, Root, Why) ||
               Fail(Form.Line, TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("'%s': %s"), *Form.Items[1].Text, *Why));
    }
    if (HeadIs(Form, TEXT("return")))
    {
        return Form.Items.Num() > 2 ? Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(return value) returns one value."))
                                    : Form.Items.Num() == 1 || Expr(Form.Items[1], Bound);
    }
    if (HeadIs(Form, TEXT("else")))
    {
        return Fail(Form.Line, TEXT("SCRIPT_INVALID"), TEXT("(else ...) goes last inside an (if ...)."));
    }
    return Expr(Form, Bound);
}
} // namespace ScriptCheckParts

using namespace ScriptCheckParts;

bool Check(const TArray<FSExpr>& Forms, const TSet<FString>& Allowed, FString& OutError, FString& OutCode)
{
    FChecker Checker{Allowed, OutError, OutCode};
    return Checker.Stmts(Forms, 0, TSet<FString>());
}
} // namespace McpScript
