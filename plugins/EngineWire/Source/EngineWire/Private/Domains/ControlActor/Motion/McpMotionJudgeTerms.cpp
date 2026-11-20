// McpMotionJudgeTerms.cpp — see header.
#include "Domains/ControlActor/Motion/McpMotionJudgeTerms.h"

namespace McpMotionJudgeTerms
{
bool ParseTerms(const FString& Expect, TArray<FTerm>& Out, FString& Error)
{
    TArray<FString> Parts;
    Expect.Replace(TEXT(" AND "), TEXT(" and ")).ParseIntoArray(Parts, TEXT(" and "), true);
    for (FString Part : Parts)
    {
        FTerm Term;
        Part.TrimStartAndEndInline();
        if (Part.StartsWith(TEXT("not "), ESearchCase::IgnoreCase))
        {
            Term.bNot = true;
            Part.RightChopInline(4);
            Part.TrimStartInline();
        }
        FString Left;
        FString Right;
        if (Part.Split(TEXT(" in "), &Left, &Right, ESearchCase::IgnoreCase))
        {
            FString Low;
            FString High;
            if (!Right.TrimStartAndEnd().Split(TEXT(".."), &Low, &High) || !Low.TrimStartAndEnd().IsNumeric() || !High.TrimStartAndEnd().IsNumeric())
            {
                Error = FString::Printf(TEXT("'%s': a range is low..high, two numbers (location.z in 0..10)."), *Part);
                return false;
            }
            Term.Op = TEXT("in");
            Term.Low = FCString::Atod(*Low.TrimStartAndEnd());
            Term.High = FCString::Atod(*High.TrimStartAndEnd());
        }
        else
        {
            for (const TCHAR* Op : {TEXT(">="), TEXT("<="), TEXT("=="), TEXT("!="), TEXT(">"), TEXT("<")})
            {
                if (Part.Split(Op, &Left, &Right))
                {
                    Term.Op = Op;
                    break;
                }
            }
            if (Term.Op.IsEmpty())
            {
                Error = FString::Printf(TEXT("'%s' compares nothing: write <value> <op> <number> with >, >=, <, <=, == or !=, or <value> in low..high."), *Part);
                return false;
            }
            Term.Value = Right.TrimStartAndEnd().TrimQuotes();
        }
        Term.Path = Left.TrimStartAndEnd();
        if (Term.Path.IsEmpty() || Term.Path.Contains(TEXT(" ")))
        {
            Error = FString::Printf(TEXT("'%s' names no value (location.z, velocity.x, rotation.yaw or a sampled property)."), *Part);
            return false;
        }
        Out.Add(Term);
    }
    if (Out.Num() == 0)
    {
        Error = TEXT("expect is empty.");
        return false;
    }
    return true;
}

TSharedPtr<FJsonValue> Read(const TSharedPtr<FJsonObject>& Sample, const TSharedPtr<FJsonValue>& Rotation, const FString& Path)
{
    FString Head;
    FString Axis;
    if (Path.Split(TEXT("."), &Head, &Axis) && (Head == TEXT("location") || Head == TEXT("velocity") || Head == TEXT("rotation")))
    {
        static const TMap<FString, int32> Index = {{TEXT("x"), 0}, {TEXT("y"), 1}, {TEXT("z"), 2}, {TEXT("pitch"), 0}, {TEXT("yaw"), 1}, {TEXT("roll"), 2}};
        const int32* At = Index.Find(Axis.ToLower());
        const TArray<TSharedPtr<FJsonValue>>* Vector = nullptr;
        const TSharedPtr<FJsonValue> Field = Head == TEXT("rotation") ? Rotation : Sample->TryGetField(Head);
        return At && Field.IsValid() && Field->TryGetArray(Vector) && Vector && Vector->IsValidIndex(*At) ? (*Vector)[*At] : nullptr;
    }
    const TSharedPtr<FJsonObject>* Properties = nullptr;
    return Sample->TryGetObjectField(TEXT("properties"), Properties) && Properties ? (*Properties)->TryGetField(Path) : nullptr;
}

ETri Eval(const FTerm& Term, const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        return ETri::Unknown;
    }
    FString Text;
    double Number = 0.0;
    bool bNumber = Value->TryGetNumber(Number);
    if (!bNumber && Value->TryGetString(Text) && Text.IsNumeric())
    {
        Number = FCString::Atod(*Text);
        bNumber = true;
    }
    bool bHolds = false;
    if (Term.Op == TEXT("in"))
    {
        if (!bNumber) return ETri::Unknown;
        bHolds = Number >= Term.Low && Number <= Term.High;
    }
    else if (bNumber && Term.Value.IsNumeric())
    {
        const double Want = FCString::Atod(*Term.Value);
        const bool bEqual = FMath::IsNearlyEqual(Number, Want, 1e-4 * FMath::Max(1.0, FMath::Abs(Want)));
        bHolds = Term.Op == TEXT("==") ? bEqual : Term.Op == TEXT("!=") ? !bEqual : Term.Op == TEXT(">") ? Number > Want
               : Term.Op == TEXT(">=") ? Number >= Want : Term.Op == TEXT("<") ? Number < Want : Number <= Want;
    }
    else if (Term.Op == TEXT("==") || Term.Op == TEXT("!="))
    {
        // TryGet only: AsString on an object or a list logs an engine error, which fails the whole reply.
        bool Flag = false;
        if (Value->TryGetBool(Flag)) Text = Flag ? TEXT("true") : TEXT("false");
        else if (!Value->TryGetString(Text)) return ETri::Unknown;
        bHolds = Text.Equals(Term.Value, ESearchCase::IgnoreCase) == (Term.Op == TEXT("=="));
    }
    else
    {
        return ETri::Unknown; // an order between things that are not numbers
    }
    return bHolds != Term.bNot ? ETri::True : ETri::False;
}
} // namespace McpMotionJudgeTerms
