#include "Domains/Scripts/McpScript.h"

namespace McpScript
{
using McpGraphCode::FSExpr;

namespace ScriptValueParts
{
const TCHAR* const Operators[] = {TEXT("=="), TEXT("!="), TEXT("<"), TEXT("<="), TEXT(">"), TEXT(">="),
                                  TEXT("+"),  TEXT("-"),  TEXT("*"), TEXT("/")};

// Equal by value: numbers by number, arrays and objects member by member.
bool Same(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    const EJson TypeA = A.IsValid() ? A->Type : EJson::Null;
    const EJson TypeB = B.IsValid() ? B->Type : EJson::Null;
    if (TypeA != TypeB)
    {
        return false;
    }
    switch (TypeA)
    {
    case EJson::Number: return A->AsNumber() == B->AsNumber();
    case EJson::String: return A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive);
    case EJson::Boolean: return A->AsBool() == B->AsBool();
    case EJson::Array:
    {
        const TArray<TSharedPtr<FJsonValue>>& ItemsA = A->AsArray();
        const TArray<TSharedPtr<FJsonValue>>& ItemsB = B->AsArray();
        for (int32 Index = 0; ItemsA.Num() == ItemsB.Num() && Index < ItemsA.Num(); ++Index)
        {
            if (!Same(ItemsA[Index], ItemsB[Index]))
            {
                return false;
            }
        }
        return ItemsA.Num() == ItemsB.Num();
    }
    case EJson::Object:
    {
        const auto& FieldsA = A->AsObject()->Values; // keyed by FString before 5.8, a shared string from 5.8
        const auto& FieldsB = B->AsObject()->Values;
        for (const auto& Field : FieldsA)
        {
            const TSharedPtr<FJsonValue>* Other = FieldsB.Find(Field.Key);
            if (!Other || !Same(Field.Value, *Other))
            {
                return false;
            }
        }
        return FieldsA.Num() == FieldsB.Num();
    }
    default: return true; // both null
    }
}

// A value as text, for + with a string: whole numbers without a decimal point (Crate_3, not Crate_3.0).
FString Text(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->Type == EJson::Null)
    {
        return TEXT("null");
    }
    if (Value->Type == EJson::Number)
    {
        const double Number = Value->AsNumber();
        return FMath::Abs(Number) < 1e15 && Number == FMath::RoundToDouble(Number)
            ? FString::Printf(TEXT("%lld"), static_cast<long long>(Number)) : FString::SanitizeFloat(Number);
    }
    return Value->Type == EJson::Boolean ? FString(Value->AsBool() ? TEXT("true") : TEXT("false")) : Value->AsString();
}

TSharedPtr<FJsonValue> ReadPath(const FString& Path, const FNames& Names, FString& OutError)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("."));
    const TSharedPtr<FJsonValue>* Root = Parts.Num() > 0 ? Names.Find(Parts[0]) : nullptr;
    if (!Root)
    {
        OutError = FString::Printf(TEXT("%s is not bound."), *Path);
        return nullptr;
    }
    TSharedPtr<FJsonValue> At = *Root;
    for (int32 Step = 1; Step < Parts.Num(); ++Step)
    {
        const FString& Part = Parts[Step];
        TSharedPtr<FJsonValue> Next;
        if (At.IsValid() && At->Type == EJson::Array && Part.IsNumeric())
        {
            const TArray<TSharedPtr<FJsonValue>>& Items = At->AsArray();
            const int32 Index = FCString::Atoi(*Part);
            Next = Items.IsValidIndex(Index) ? Items[Index] : nullptr;
        }
        else if (At.IsValid() && At->Type == EJson::Object)
        {
            Next = At->AsObject()->TryGetField(Part);
        }
        if (!Next.IsValid())
        {
            TArray<FString> Fields;
            if (At.IsValid() && At->Type == EJson::Object)
            {
                for (const auto& Field : At->AsObject()->Values)
                {
                    Fields.Emplace(*Field.Key);
                }
                Fields.Sort();
            }
            OutError = FString::Printf(TEXT("%s has no %s%s"), *FString::Join(TArray<FString>(Parts.GetData(), Step), TEXT(".")), *Part,
                                       Fields.Num() > 0 ? *FString::Printf(TEXT("; it has: %s."), *FString::Join(Fields, TEXT(", ")))
                                       : At.IsValid() && At->Type == EJson::Array
                                           ? *FString::Printf(TEXT(" (a list of %d)."), At->AsArray().Num()) : TEXT("."));
            return nullptr;
        }
        At = Next;
    }
    return At;
}
} // namespace ScriptValueParts

using namespace ScriptValueParts;

bool HeadIs(const FSExpr& Form, const TCHAR* Word)
{
    return Form.Type == FSExpr::EType::List && Form.Items.Num() > 0 && Form.Items[0].Type == FSExpr::EType::Symbol &&
           Form.Items[0].Text.Equals(Word, ESearchCase::CaseSensitive);
}

bool IsCall(const FSExpr& Form)
{
    if (Form.Type != FSExpr::EType::List || Form.Items.Num() == 0 || Form.Items[0].Type != FSExpr::EType::Symbol)
    {
        return false;
    }
    const FString& Head = Form.Items[0].Text;
    return Head.Contains(TEXT(".")) && !Head.StartsWith(TEXT(".")) && !Head.StartsWith(TEXT(":")) && !Head.Contains(TEXT("|"));
}

bool IsOperator(const FSExpr& Form)
{
    for (const TCHAR* Op : Operators)
    {
        if (HeadIs(Form, Op))
        {
            return true;
        }
    }
    return false;
}

bool EvalAtom(const FSExpr& Expr, const FNames& Names, TSharedPtr<FJsonValue>& Out, FString& OutError)
{
    switch (Expr.Type)
    {
    case FSExpr::EType::String: Out = MakeShared<FJsonValueString>(Expr.Text); return true;
    case FSExpr::EType::Int:
    case FSExpr::EType::Float: Out = MakeShared<FJsonValueNumber>(FCString::Atod(*Expr.Text)); return true;
    case FSExpr::EType::Bool: Out = MakeShared<FJsonValueBoolean>(Expr.bTrue); return true;
    case FSExpr::EType::Symbol:
        if (Expr.Text == TEXT("null"))
        {
            Out = MakeShared<FJsonValueNull>();
            return true;
        }
        Out = ReadPath(Expr.Text, Names, OutError);
        return Out.IsValid();
    default:
        OutError = TEXT("a value is a literal, null, a name or a path into one.");
        return false;
    }
}

TSharedPtr<FJsonValue> Apply(const FString& Op, const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (Op == TEXT("==") || Op == TEXT("!="))
    {
        return MakeShared<FJsonValueBoolean>(Same(A, B) == (Op == TEXT("==")));
    }
    const bool bText = (A.IsValid() && A->Type == EJson::String) || (B.IsValid() && B->Type == EJson::String);
    if (Op == TEXT("+") && bText)
    {
        return MakeShared<FJsonValueString>(Text(A) + Text(B));
    }
    double X = 0.0;
    double Y = 0.0;
    if (!A.IsValid() || !B.IsValid() || A->Type != EJson::Number || B->Type != EJson::Number || !A->TryGetNumber(X) || !B->TryGetNumber(Y))
    {
        return nullptr;
    }
    if (Op == TEXT("<")) return MakeShared<FJsonValueBoolean>(X < Y);
    if (Op == TEXT("<=")) return MakeShared<FJsonValueBoolean>(X <= Y);
    if (Op == TEXT(">")) return MakeShared<FJsonValueBoolean>(X > Y);
    if (Op == TEXT(">=")) return MakeShared<FJsonValueBoolean>(X >= Y);
    if (Op == TEXT("+")) return MakeShared<FJsonValueNumber>(X + Y);
    if (Op == TEXT("-")) return MakeShared<FJsonValueNumber>(X - Y);
    if (Op == TEXT("*")) return MakeShared<FJsonValueNumber>(X * Y);
    if (Y == 0.0) return nullptr;
    return MakeShared<FJsonValueNumber>(X / Y);
}

bool IsTrue(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return false;
    }
    switch (Value->Type)
    {
    case EJson::Boolean: return Value->AsBool();
    case EJson::Number: return Value->AsNumber() != 0.0;
    case EJson::String: return !Value->AsString().IsEmpty();
    case EJson::Array: return Value->AsArray().Num() > 0;
    case EJson::Object: return Value->AsObject()->Values.Num() > 0;
    default: return false;
    }
}
} // namespace McpScript
