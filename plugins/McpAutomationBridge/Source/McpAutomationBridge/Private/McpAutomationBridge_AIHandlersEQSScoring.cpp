#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"

namespace McpAIHandlers
{
static EEnvTestScoreEquation::Type ParseEQSScoringEquationAI(const FString& Value)
{
    if (Value.Equals(TEXT("Square"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::Square;
    if (Value.Equals(TEXT("InverseLinear"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::InverseLinear;
    if (Value.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) return EEnvTestScoreEquation::Constant;
    return EEnvTestScoreEquation::Linear;
}

static EEnvTestFilterType::Type ParseEQSFilterTypeAI(const FString& Value)
{
    if (Value.Equals(TEXT("Minimum"), ESearchCase::IgnoreCase)) return EEnvTestFilterType::Minimum;
    if (Value.Equals(TEXT("Maximum"), ESearchCase::IgnoreCase)) return EEnvTestFilterType::Maximum;
    return EEnvTestFilterType::Range;
}


bool HandleConfigureEQSTestScoring(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_test_scoring");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_test_scoring"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        int32 TestIndex = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("testIndex"), 0));

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

        bool bConfiguredAnySetting = false;
        FString ScoringEquation;
        if (Settings->TryGetStringField(TEXT("scoringEquation"), ScoringEquation) && !ScoringEquation.IsEmpty())
        {
            TargetTest->ScoringEquation = ParseEQSScoringEquationAI(ScoringEquation);
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }

        FString FilterType;
        if (Settings->TryGetStringField(TEXT("filterType"), FilterType) && !FilterType.IsEmpty())
        {
            TargetTest->FilterType = ParseEQSFilterTypeAI(FilterType);
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }

        double NumericValue = 0.0;
        if (Settings->TryGetNumberField(TEXT("clampMin"), NumericValue))
        {
            TargetTest->ScoreClampMin.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->ClampMinType = EEnvQueryTestClamping::SpecifiedValue;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }
        if (Settings->TryGetNumberField(TEXT("clampMax"), NumericValue))
        {
            TargetTest->ScoreClampMax.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->ClampMaxType = EEnvQueryTestClamping::SpecifiedValue;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }
        if (Settings->TryGetNumberField(TEXT("floatMin"), NumericValue))
        {
            TargetTest->FloatValueMin.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->FilterType = EEnvTestFilterType::Range;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }
        if (Settings->TryGetNumberField(TEXT("floatMax"), NumericValue))
        {
            TargetTest->FloatValueMax.DefaultValue = static_cast<float>(NumericValue);
            TargetTest->FilterType = EEnvTestFilterType::Range;
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            bConfiguredAnySetting = true;
        }

        if (!bConfiguredAnySetting)
        {
            TargetTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
            TargetTest->ScoringEquation = EEnvTestScoreEquation::Linear;
            bConfiguredAnySetting = true;
        }

        Query->MarkPackageDirty();
        McpSafeAssetSave(Query);
        Result->SetNumberField(TEXT("optionIndex"), ResolvedOptionIndex);
        Result->SetNumberField(TEXT("optionTestIndex"), ResolvedTestIndex);
        Result->SetNumberField(TEXT("testIndex"), TestIndex);
        Result->SetStringField(TEXT("testClass"), TargetTest->GetClass()->GetName());
        Result->SetBoolField(TEXT("configured"), bConfiguredAnySetting);
        Result->SetStringField(TEXT("message"), TEXT("Test scoring configured"));

        McpHandlerUtils::AddVerification(Result, Query);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Scoring configured"), Result);
        return true;
    }

    // =========================================================================
    // 16.5 Perception System (5 actions)
    // =========================================================================

    return true;
}
}
#endif
