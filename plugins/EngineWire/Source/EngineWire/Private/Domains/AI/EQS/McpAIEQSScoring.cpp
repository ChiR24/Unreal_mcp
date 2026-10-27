#include "Domains/AI/McpAIContext.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"

namespace McpAIHandlers
{
// An unknown name used to become Linear or Range; false now, and the call is refused.
static bool ParseEQSScoringEquationAI(const FString& Value, EEnvTestScoreEquation::Type& Out)
{
    static const TPair<const TCHAR*, EEnvTestScoreEquation::Type> Names[] = {
        {TEXT("Linear"), EEnvTestScoreEquation::Linear}, {TEXT("Square"), EEnvTestScoreEquation::Square},
        {TEXT("InverseLinear"), EEnvTestScoreEquation::InverseLinear}, {TEXT("SquareRoot"), EEnvTestScoreEquation::SquareRoot},
        {TEXT("Constant"), EEnvTestScoreEquation::Constant}};
    for (const auto& Name : Names)
    {
        if (Value.Equals(Name.Key, ESearchCase::IgnoreCase))
        {
            Out = Name.Value;
            return true;
        }
    }
    return false;
}

static bool ParseEQSFilterTypeAI(const FString& Value, EEnvTestFilterType::Type& Out)
{
    static const TPair<const TCHAR*, EEnvTestFilterType::Type> Names[] = {
        {TEXT("Minimum"), EEnvTestFilterType::Minimum}, {TEXT("Maximum"), EEnvTestFilterType::Maximum},
        {TEXT("Range"), EEnvTestFilterType::Range}, {TEXT("Match"), EEnvTestFilterType::Match}};
    for (const auto& Name : Names)
    {
        if (Value.Equals(Name.Key, ESearchCase::IgnoreCase))
        {
            Out = Name.Value;
            return true;
        }
    }
    return false;
}


// Implements the "configure_test_scoring" action.
bool HandleConfigureEQSTestScoring(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    int32 TestIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("testIndex"), 0));

    if (TestIndex < 0)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("testIndex must be zero or greater"),
                            TEXT("INVALID_PARAMS"));
        return true;
    }

    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UEnvQueryTest* TargetTest = nullptr;
    int32 ResolvedOptionIndex = INDEX_NONE;
    int32 ResolvedTestIndex = INDEX_NONE;
    int32 FlatIndex = 0;
    auto& Options = Query->GetOptionsMutable();
    for (int32 OptionIndex = 0; OptionIndex < Options.Num(); ++OptionIndex)
    {
        UEnvQueryOption* Option = Options[OptionIndex];
        if (!Option)
        {
            continue;
        }

        for (int32 OptionTestIndex = 0; OptionTestIndex < Option->Tests.Num(); ++OptionTestIndex)
        {
            UEnvQueryTest* Test = Option->Tests[OptionTestIndex];
            if (!Test)
            {
                continue;
            }

            if (FlatIndex == TestIndex)
            {
                TargetTest = Test;
                ResolvedOptionIndex = OptionIndex;
                ResolvedTestIndex = OptionTestIndex;
                break;
            }
            ++FlatIndex;
        }

        if (TargetTest)
        {
            break;
        }
    }

    if (!TargetTest)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("EQS test index %d was not found on query: %s"), TestIndex, *QueryPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    const TSharedPtr<FJsonObject>* TestSettings = nullptr;
    Payload->TryGetObjectField(TEXT("testSettings"), TestSettings);
    const TSharedPtr<FJsonObject>& Settings = (TestSettings && TestSettings->IsValid()) ? *TestSettings : Payload;

    // Every name is checked before the test changes.
    FString ScoringEquation;
    FString FilterType;
    EEnvTestScoreEquation::Type Equation = EEnvTestScoreEquation::Linear;
    EEnvTestFilterType::Type Filter = EEnvTestFilterType::Range;
    const bool bHasEquation = Settings->TryGetStringField(TEXT("scoringEquation"), ScoringEquation) && !ScoringEquation.IsEmpty();
    const bool bHasFilter = Settings->TryGetStringField(TEXT("filterType"), FilterType) && !FilterType.IsEmpty();
    if ((bHasEquation && !ParseEQSScoringEquationAI(ScoringEquation, Equation)) || (bHasFilter && !ParseEQSFilterTypeAI(FilterType, Filter)))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("scoringEquation is Linear, Square, InverseLinear, SquareRoot or Constant; "
            "filterType is Minimum, Maximum, Range or Match."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // A call that set nothing used to switch the test to Linear scoring and answer configured.
    if (!bHasEquation && !bHasFilter && !Settings->HasField(TEXT("clampMin")) && !Settings->HasField(TEXT("clampMax"))
        && !Settings->HasField(TEXT("floatMin")) && !Settings->HasField(TEXT("floatMax")))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Nothing to set: send scoringEquation, filterType, clampMin, clampMax, "
            "floatMin or floatMax (in testSettings or at the top level)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TargetTest->Modify();
    TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
    if (bHasEquation)
    {
        TargetTest->ScoringEquation = Equation;
    }
    if (bHasFilter)
    {
        TargetTest->FilterType = Filter;
    }
    double NumericValue = 0.0;
    if (Settings->TryGetNumberField(TEXT("clampMin"), NumericValue))
    {
        TargetTest->ScoreClampMin.DefaultValue = static_cast<float>(NumericValue);
        TargetTest->ClampMinType = EEnvQueryTestClamping::SpecifiedValue;
    }
    if (Settings->TryGetNumberField(TEXT("clampMax"), NumericValue))
    {
        TargetTest->ScoreClampMax.DefaultValue = static_cast<float>(NumericValue);
        TargetTest->ClampMaxType = EEnvQueryTestClamping::SpecifiedValue;
    }
    // A float range filters by Range; an explicit filterType wins.
    if (Settings->TryGetNumberField(TEXT("floatMin"), NumericValue))
    {
        TargetTest->FloatValueMin.DefaultValue = static_cast<float>(NumericValue);
        TargetTest->FilterType = bHasFilter ? Filter : EEnvTestFilterType::Range;
    }
    if (Settings->TryGetNumberField(TEXT("floatMax"), NumericValue))
    {
        TargetTest->FloatValueMax.DefaultValue = static_cast<float>(NumericValue);
        TargetTest->FilterType = bHasFilter ? Filter : EEnvTestFilterType::Range;
    }

    Query->MarkPackageDirty();
    Result->SetNumberField(TEXT("optionIndex"), ResolvedOptionIndex);
    Result->SetNumberField(TEXT("optionTestIndex"), ResolvedTestIndex);
    Result->SetNumberField(TEXT("testIndex"), TestIndex);
    Result->SetStringField(TEXT("testClass"), TargetTest->GetClass()->GetName());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Query));

    McpHandlerUtils::AddVerification(Result, Query);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scoring configured"), Result);
    return true;
}
}
