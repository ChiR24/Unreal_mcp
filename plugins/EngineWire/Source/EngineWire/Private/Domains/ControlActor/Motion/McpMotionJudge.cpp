// McpMotionJudge.cpp — see header.
#include "Domains/ControlActor/Motion/McpMotionJudge.h"

#include "Domains/ControlActor/Motion/McpMotionJudgeTerms.h"

using namespace McpMotionJudgeTerms;

namespace
{
struct FTimeline
{
    TArray<TSharedPtr<FJsonObject>> Samples;
    TArray<TSharedPtr<FJsonValue>> Rotations; // the last rotation seen at each sample
    TArray<double> Times;
};

ETri All(const TArray<FTerm>& Terms, const FTimeline& Line, int32 At, const TSharedPtr<FJsonObject>& OutRead)
{
    ETri Total = ETri::True;
    for (const FTerm& Term : Terms)
    {
        const TSharedPtr<FJsonValue> Value = Read(Line.Samples[At], Line.Rotations[At], Term.Path);
        if (OutRead.IsValid() && Value.IsValid()) OutRead->SetField(Term.Path, Value);
        const ETri One = Eval(Term, Value);
        Total = Total == ETri::False || One == ETri::False ? ETri::False : Total == ETri::Unknown || One == ETri::Unknown ? ETri::Unknown : ETri::True;
    }
    return Total;
}

TSharedPtr<FJsonValue> Verdict(int32 Clause, const TCHAR* Word, double At, const TSharedPtr<FJsonObject>& Seen, const FString& Why)
{
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetNumberField(TEXT("clause"), Clause);
    Out->SetStringField(TEXT("verdict"), Word);
    if (At >= 0.0) Out->SetNumberField(TEXT("atSeconds"), At);
    if (Seen.IsValid() && Seen->Values.Num() > 0) Out->SetObjectField(TEXT("read"), Seen);
    if (!Why.IsEmpty()) Out->SetStringField(TEXT("why"), Why);
    return MakeShared<FJsonValueObject>(Out);
}

const TCHAR* Word(ETri Result) { return Result == ETri::True ? TEXT("ok") : Result == ETri::False ? TEXT("failed") : TEXT("unknown"); }

// One clause, judged over the samples of Line.
TSharedPtr<FJsonValue> JudgeClause(int32 Index, const TSharedPtr<FJsonObject>& Clause, const FTimeline& Line, bool bFull, const FString& Short)
{
    TArray<FTerm> Terms;
    FString Error;
    ParseTerms(Clause->GetStringField(TEXT("expect")), Terms, Error);
    FString When = TEXT("always");
    Clause->TryGetStringField(TEXT("when"), When);
    int32 First = 0;
    int32 Last = Line.Samples.Num() - 1;
    bool bCovered = bFull;
    const TSharedPtr<FJsonObject>* After = nullptr;
    if (Clause->TryGetObjectField(TEXT("after"), After) && After)
    {
        TArray<FTerm> AfterTerms;
        ParseTerms((*After)->GetStringField(TEXT("expect")), AfterTerms, Error);
        First = INDEX_NONE;
        for (int32 At = 0; At < Line.Samples.Num() && First == INDEX_NONE; ++At)
        {
            First = All(AfterTerms, Line, At, nullptr) == ETri::True ? At : INDEX_NONE;
        }
        if (First == INDEX_NONE)
        {
            return Verdict(Index, bFull ? TEXT("failed") : TEXT("unknown"), -1.0, nullptr,
                bFull ? FString::Printf(TEXT("after never held: %s"), *(*After)->GetStringField(TEXT("expect"))) : Short);
        }
        double Within = 0.0;
        if (Clause->TryGetNumberField(TEXT("withinSeconds"), Within))
        {
            const double End = Line.Times[First] + Within;
            bCovered = bFull || Line.Times.Last() >= End;
            while (Last > First && Line.Times[Last] > End) --Last;
        }
    }
    if (Line.Samples.Num() == 0 || Last < First)
    {
        return Verdict(Index, TEXT("unknown"), -1.0, nullptr, TEXT("no sample to judge"));
    }
    if (When == TEXT("at") || When == TEXT("atEnd"))
    {
        int32 At = Last;
        if (When == TEXT("at"))
        {
            const double Want = Clause->GetNumberField(TEXT("atSeconds")) + (After ? Line.Times[First] : 0.0);
            At = First;
            while (At < Last && Line.Times[At] < Want) ++At;
            if (Line.Times[At] < Want)
            {
                return Verdict(Index, TEXT("unknown"), -1.0, nullptr, bFull
                    ? FString::Printf(TEXT("atSeconds %.3g is past the last sample (%.3g s)"), Want, Line.Times[Last]) : Short);
            }
        }
        else if (!bCovered)
        {
            return Verdict(Index, TEXT("unknown"), -1.0, nullptr, Short);
        }
        const TSharedPtr<FJsonObject> Seen = MakeShared<FJsonObject>();
        const ETri Result = All(Terms, Line, At, Seen);
        return Verdict(Index, Word(Result), Line.Times[At], Seen, Result == ETri::Unknown ? TEXT("a value it names is in no sample") : FString());
    }
    // always: the first sample that breaks it decides; ever: the first that meets it. A run cut short decides only what
    // the samples it took already show.
    const bool bEver = When == TEXT("ever");
    bool bUnknown = false;
    for (int32 At = First; At <= Last; ++At)
    {
        const TSharedPtr<FJsonObject> Seen = MakeShared<FJsonObject>();
        const ETri Result = All(Terms, Line, At, Seen);
        bUnknown |= Result == ETri::Unknown;
        if ((bEver && Result == ETri::True) || (!bEver && Result == ETri::False))
        {
            return Verdict(Index, bEver ? TEXT("ok") : TEXT("failed"), Line.Times[At], Seen, FString());
        }
    }
    if (bUnknown) return Verdict(Index, TEXT("unknown"), -1.0, nullptr, TEXT("a value it names is in no sample"));
    if (!bCovered) return Verdict(Index, TEXT("unknown"), -1.0, nullptr, Short);
    return Verdict(Index, bEver ? TEXT("failed") : TEXT("ok"), -1.0, nullptr, FString());
}
} // namespace

namespace McpMotionJudge
{
bool Validate(const TSharedPtr<FJsonObject>& Payload, FString& OutError, FString& OutPointer)
{
    const TArray<TSharedPtr<FJsonValue>>* Clauses = nullptr;
    if (!Payload.IsValid() || !Payload->HasField(TEXT("judge")))
    {
        return true;
    }
    OutPointer = TEXT("/judge");
    if (!Payload->TryGetArrayField(TEXT("judge"), Clauses) || !Clauses || Clauses->Num() < 1 || Clauses->Num() > 16)
    {
        OutError = TEXT("judge is a list of 1-16 clauses, each {expect, when: always | ever | atEnd | at}.");
        return false;
    }
    for (int32 Index = 0; Index < Clauses->Num(); ++Index)
    {
        OutPointer = FString::Printf(TEXT("/judge/%d"), Index);
        const TSharedPtr<FJsonObject>* Clause = nullptr;
        FString Expect;
        FString When = TEXT("always");
        TArray<FTerm> Terms;
        if (!(*Clauses)[Index].IsValid() || !(*Clauses)[Index]->TryGetObject(Clause) || !Clause ||
            !(*Clause)->TryGetStringField(TEXT("expect"), Expect) || !ParseTerms(Expect, Terms, OutError))
        {
            OutError = OutError.IsEmpty() ? TEXT("each clause needs expect, such as \"location.z > 120\".") : OutError;
            return false;
        }
        (*Clause)->TryGetStringField(TEXT("when"), When);
        double Seconds = 0.0;
        if (When != TEXT("always") && When != TEXT("ever") && When != TEXT("atEnd") && When != TEXT("at"))
        {
            OutError = FString::Printf(TEXT("when '%s' is not always, ever, atEnd or at."), *When);
            return false;
        }
        if (When == TEXT("at") && (!(*Clause)->TryGetNumberField(TEXT("atSeconds"), Seconds) || Seconds < 0.0))
        {
            OutError = TEXT("when \"at\" needs atSeconds, the game time into the run (0 or more).");
            return false;
        }
        const TSharedPtr<FJsonObject>* After = nullptr;
        if ((*Clause)->HasField(TEXT("after")))
        {
            FString AfterExpect;
            TArray<FTerm> AfterTerms;
            if (!(*Clause)->TryGetObjectField(TEXT("after"), After) || !After || !(*After)->TryGetStringField(TEXT("expect"), AfterExpect) ||
                !ParseTerms(AfterExpect, AfterTerms, OutError))
            {
                OutError = OutError.IsEmpty() ? TEXT("after is {expect}: what must hold first.") : OutError;
                return false;
            }
        }
        if ((*Clause)->HasField(TEXT("withinSeconds")) && (!After || !(*Clause)->TryGetNumberField(TEXT("withinSeconds"), Seconds) || Seconds <= 0.0))
        {
            OutError = TEXT("withinSeconds (more than 0) bounds the window an after opens.");
            return false;
        }
    }
    return true;
}

bool Take(const TSharedPtr<FJsonObject>& Payload, TArray<TSharedPtr<FJsonValue>>& OutClauses, FString& OutError)
{
    FString Pointer;
    if (!Validate(Payload, OutError, Pointer))
    {
        OutError = FString::Printf(TEXT("%s: %s"), *Pointer, *OutError);
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Clauses = nullptr;
    OutClauses = Payload.IsValid() && Payload->TryGetArrayField(TEXT("judge"), Clauses) && Clauses ? *Clauses : TArray<TSharedPtr<FJsonValue>>();
    return true;
}

TArray<TSharedPtr<FJsonValue>> Judge(const TArray<TSharedPtr<FJsonValue>>& Clauses, const TArray<TSharedPtr<FJsonValue>>& Samples,
                                     const FString& EndedBecause)
{
    FTimeline Line;
    TSharedPtr<FJsonValue> Rotation;
    for (const TSharedPtr<FJsonValue>& Value : Samples)
    {
        const TSharedPtr<FJsonObject>* Sample = nullptr;
        double T = 0.0;
        if (!Value.IsValid() || !Value->TryGetObject(Sample) || !Sample || !(*Sample)->TryGetNumberField(TEXT("t"), T)) continue;
        Rotation = (*Sample)->HasField(TEXT("rotation")) ? (*Sample)->TryGetField(TEXT("rotation")) : Rotation;
        Line.Samples.Add(*Sample);
        Line.Rotations.Add(Rotation);
        Line.Times.Add(T);
    }
    const bool bFull = EndedBecause == TEXT("duration");
    const FString Short = FString::Printf(TEXT("the run ended early (%s) before this could be judged"), *EndedBecause);
    TArray<TSharedPtr<FJsonValue>> Out;
    for (int32 Index = 0; Index < Clauses.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Clause = nullptr;
        if (Clauses[Index].IsValid() && Clauses[Index]->TryGetObject(Clause) && Clause)
        {
            Out.Add(JudgeClause(Index, *Clause, Line, bFull, Short));
        }
    }
    return Out;
}

void AddTo(const TArray<TSharedPtr<FJsonValue>>& Clauses, const TArray<TSharedPtr<FJsonValue>>& Samples,
           const FString& EndedBecause, FJsonObject& Data)
{
    if (Clauses.Num() == 0)
    {
        return;
    }
    const TArray<TSharedPtr<FJsonValue>> Verdicts = Judge(Clauses, Samples, EndedBecause);
    FString Overall = TEXT("ok");
    for (const TSharedPtr<FJsonValue>& Verdict : Verdicts)
    {
        const FString One = Verdict->AsObject()->GetStringField(TEXT("verdict"));
        Overall = Overall == TEXT("failed") || One == TEXT("failed") ? TEXT("failed") : One == TEXT("unknown") ? TEXT("unknown") : Overall;
    }
    Data.SetArrayField(TEXT("judge"), Verdicts);
    Data.SetStringField(TEXT("judgeVerdict"), Overall);
}
} // namespace McpMotionJudge
