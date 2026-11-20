// McpExecuteVerify.cpp — see header.
#include "MCP/Execute/Request/McpExecuteVerify.h"

#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

namespace
{
using McpExecuteVerify::EMatch;

EMatch Fold(EMatch Total, EMatch One)
{
    return Total == EMatch::Differs || One == EMatch::Differs ? EMatch::Differs
         : Total == EMatch::Unknown || One == EMatch::Unknown ? EMatch::Unknown
                                                             : EMatch::Same;
}

// One spelling of an engine value: no Class'...' wrapper, no enum scope (EComponentMobility::Movable), an object path
// without its repeated name (/Game/A/SM_A.SM_A), case aside. A read-back names a value its own way, not wrongly.
FString Spelling(FString Text)
{
    Text.TrimStartAndEndInline();
    int32 Quote = INDEX_NONE;
    if (Text.EndsWith(TEXT("'")) && Text.FindChar(TEXT('\''), Quote) && Quote < Text.Len() - 1)
    {
        Text = Text.Mid(Quote + 1, Text.Len() - Quote - 2);
    }
    int32 Scope = INDEX_NONE;
    if (!Text.StartsWith(TEXT("/")) && (Scope = Text.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd)) != INDEX_NONE)
    {
        Text.RightChopInline(Scope + 2);
    }
    int32 Dot = INDEX_NONE;
    int32 Slash = INDEX_NONE;
    if (Text.StartsWith(TEXT("/")) && Text.FindLastChar(TEXT('.'), Dot) && Text.FindLastChar(TEXT('/'), Slash) && Dot > Slash &&
        Text.Mid(Slash + 1, Dot - Slash - 1) == Text.Mid(Dot + 1))
    {
        Text.LeftInline(Dot);
    }
    return Text.ToLower();
}

// A list of numbers against an object holding them under one of the engine's component orders.
EMatch NumbersAgainstObject(const TArray<TSharedPtr<FJsonValue>>& Numbers, const TSharedPtr<FJsonObject>& Object)
{
    static const TArray<TArray<FString>> Orders = {
        {TEXT("X"), TEXT("Y"), TEXT("Z"), TEXT("W")}, {TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A")}, {TEXT("Pitch"), TEXT("Yaw"), TEXT("Roll")}};
    for (const TArray<FString>& Order : Orders)
    {
        if (Numbers.Num() > Order.Num() || Numbers.Num() < 2)
        {
            continue;
        }
        bool bAll = true;
        for (int32 Index = 0; Index < Numbers.Num() && bAll; ++Index)
        {
            bAll = Object->HasTypedField<EJson::Number>(Order[Index]);
        }
        if (!bAll)
        {
            continue;
        }
        EMatch Total = EMatch::Same;
        for (int32 Index = 0; Index < Numbers.Num(); ++Index)
        {
            Total = Fold(Total, McpExecuteVerify::Compare(Numbers[Index], Object->TryGetField(Order[Index])));
        }
        return Total;
    }
    return EMatch::Unknown;
}
} // namespace

namespace McpExecuteVerify
{
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
    const TSharedPtr<FJsonValue> Value = Options.IsValid() ? Options->TryGetField(TEXT("verify")) : nullptr;
    if (Value.IsValid() && Value->Type != EJson::Boolean)
    {
        OutError = McpValidationError(TEXT("INVALID_OPTIONS"), TEXT("options.verify must be true or false."), TEXT("/options/verify"));
        return false;
    }
    return true;
}

EMatch Compare(const TSharedPtr<FJsonValue>& Sent, const TSharedPtr<FJsonValue>& ReadBack)
{
    if (!Sent.IsValid() || !ReadBack.IsValid())
    {
        return EMatch::Unknown;
    }
    if (Sent->Type == EJson::Number && ReadBack->Type == EJson::Number)
    {
        const double A = Sent->AsNumber();
        const double B = ReadBack->AsNumber();
        return FMath::Abs(A - B) <= 1e-4 * FMath::Max(1.0, FMath::Max(FMath::Abs(A), FMath::Abs(B))) ? EMatch::Same : EMatch::Differs;
    }
    if (Sent->Type == EJson::Boolean && ReadBack->Type == EJson::Boolean)
    {
        return Sent->AsBool() == ReadBack->AsBool() ? EMatch::Same : EMatch::Differs;
    }
    if (Sent->Type == EJson::String && ReadBack->Type == EJson::String)
    {
        return Spelling(Sent->AsString()) == Spelling(ReadBack->AsString()) ? EMatch::Same : EMatch::Differs;
    }
    if (Sent->Type == EJson::Array && ReadBack->Type == EJson::Object)
    {
        return NumbersAgainstObject(Sent->AsArray(), ReadBack->AsObject());
    }
    if (Sent->Type == EJson::Array && ReadBack->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Left = Sent->AsArray();
        const TArray<TSharedPtr<FJsonValue>>& Right = ReadBack->AsArray();
        EMatch Total = Left.Num() == Right.Num() ? EMatch::Same : EMatch::Differs;
        for (int32 Index = 0; Total != EMatch::Differs && Index < Left.Num(); ++Index)
        {
            Total = Fold(Total, Compare(Left[Index], Right[Index]));
        }
        return Total;
    }
    if (Sent->Type == EJson::Object && ReadBack->Type == EJson::Object)
    {
        EMatch Total = EMatch::Same;
        for (const auto& Pair : Sent->AsObject()->Values)
        {
            Total = Fold(Total, Compare(Pair.Value, ReadBack->AsObject()->TryGetField(FString(*Pair.Key))));
        }
        return Total;
    }
    return EMatch::Unknown;
}

void Stamp(const TSharedPtr<FJsonObject>& Behavior, const TSharedPtr<FJsonObject>& Options,
           const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Data, const TSharedPtr<FJsonObject>& Receipt)
{
    const TSharedPtr<FJsonObject>* ReadBack = nullptr;
    const TSharedPtr<FJsonObject>* CompareMap = nullptr;
    bool bVerify = true;
    if (Options.IsValid()) Options->TryGetBoolField(TEXT("verify"), bVerify);
    if (!bVerify || !Behavior.IsValid() || !Params.IsValid() || !Data.IsValid() || !Receipt.IsValid() ||
        !Behavior->TryGetObjectField(TEXT("readBack"), ReadBack) || !ReadBack || !(*ReadBack)->TryGetObjectField(TEXT("compare"), CompareMap) || !CompareMap)
    {
        return;
    }
    int32 Compared = 0;
    TArray<TSharedPtr<FJsonValue>> Mismatches;
    for (const auto& Pair : (*CompareMap)->Values)
    {
        const FString Field(*Pair.Key);
        const FString Input = Pair.Value.IsValid() ? Pair.Value->AsString().RightChop(8) : FString(); // "$params."
        const TSharedPtr<FJsonValue> Sent = Params->TryGetField(Input);
        // A field the output contract does not declare is under details, where the reply's projection put it.
        TSharedPtr<FJsonValue> Back = Data->TryGetField(Field);
        const TSharedPtr<FJsonObject>* Details = nullptr;
        if (!Back.IsValid() && Data->TryGetObjectField(TEXT("details"), Details) && Details) Back = (*Details)->TryGetField(Field);
        const EMatch Verdict = Compare(Sent, Back);
        if (Verdict == EMatch::Unknown)
        {
            continue;
        }
        ++Compared;
        if (Verdict == EMatch::Differs)
        {
            const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("field"), Field);
            Row->SetField(TEXT("sent"), Sent);
            Row->SetField(TEXT("readBack"), Back);
            Mismatches.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    if (Compared == 0)
    {
        return;
    }
    const TSharedPtr<FJsonObject>* Existing = nullptr;
    TSharedPtr<FJsonObject> Block = Receipt->TryGetObjectField(TEXT("receipt"), Existing) && Existing ? *Existing : MakeShared<FJsonObject>();
    Block->SetBoolField(TEXT("verified"), Mismatches.Num() == 0);
    if (Mismatches.Num() > 0)
    {
        Block->SetArrayField(TEXT("mismatches"), Mismatches);
    }
    Receipt->SetObjectField(TEXT("receipt"), Block);
}
} // namespace McpExecuteVerify
