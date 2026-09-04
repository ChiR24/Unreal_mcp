// McpAutomationBridge_AIHandlersEnvQueryInfo.cpp — get_ai_info {queryPath} payload.
//
// Dogfood #76: the EQS branch only echoed the query name. Serialize the options
// (generator + tests) so a client can see what the query actually does.
#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"

namespace
{
FString McpTestPurposeName(int32 Purpose)
{
    switch (Purpose)
    {
    case 0: return TEXT("Filter");
    case 1: return TEXT("Score");
    case 2: return TEXT("FilterAndScore");
    default: return FString::FromInt(Purpose);
    }
}

FString McpTestFilterTypeName(int32 FilterType)
{
    switch (FilterType)
    {
    case 0: return TEXT("Minimum");
    case 1: return TEXT("Maximum");
    case 2: return TEXT("Range");
    case 3: return TEXT("Match");
    default: return FString::FromInt(FilterType);
    }
}
} // namespace

void McpSerializeEnvQueryInfo(UEnvQuery* Query, const TSharedPtr<FJsonObject>& Out)
{
    if (!Query || !Out.IsValid())
    {
        return;
    }
    Out->SetStringField(TEXT("queryName"), Query->GetName());
    Out->SetStringField(TEXT("queryPath"), Query->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Options;
    int32 TotalTests = 0;
    int32 OptionIndex = 0;
    for (UEnvQueryOption* Option : Query->GetOptions())
    {
        TSharedPtr<FJsonObject> OptionJson = MakeShared<FJsonObject>();
        OptionJson->SetNumberField(TEXT("index"), OptionIndex++);
        if (Option && Option->Generator)
        {
            OptionJson->SetStringField(TEXT("generatorClass"), Option->Generator->GetClass()->GetName());
            OptionJson->SetStringField(TEXT("generatorClassPath"), Option->Generator->GetClass()->GetPathName());
            OptionJson->SetStringField(TEXT("optionName"), Option->Generator->OptionName);
            OptionJson->SetStringField(TEXT("itemType"), Option->Generator->ItemType ? Option->Generator->ItemType->GetName() : TEXT(""));
        }
        TArray<TSharedPtr<FJsonValue>> Tests;
        if (Option)
        {
            for (UEnvQueryTest* Test : Option->Tests)
            {
                if (!Test)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> TestJson = MakeShared<FJsonObject>();
                TestJson->SetStringField(TEXT("testClass"), Test->GetClass()->GetName());
                TestJson->SetStringField(TEXT("testClassPath"), Test->GetClass()->GetPathName());
                TestJson->SetStringField(TEXT("purpose"), McpTestPurposeName(static_cast<int32>(Test->TestPurpose)));
                TestJson->SetStringField(TEXT("filterType"), McpTestFilterTypeName(static_cast<int32>(Test->FilterType)));
                Tests.Add(MakeShared<FJsonValueObject>(TestJson));
                ++TotalTests;
            }
        }
        OptionJson->SetNumberField(TEXT("testCount"), Tests.Num());
        OptionJson->SetArrayField(TEXT("tests"), Tests);
        Options.Add(MakeShared<FJsonValueObject>(OptionJson));
    }
    Out->SetNumberField(TEXT("optionCount"), Options.Num());
    Out->SetNumberField(TEXT("testCount"), TotalTests);
    Out->SetArrayField(TEXT("options"), Options);
}
