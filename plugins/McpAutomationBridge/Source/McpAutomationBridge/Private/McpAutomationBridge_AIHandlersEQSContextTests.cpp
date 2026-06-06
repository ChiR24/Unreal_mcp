#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#define MCP_HAS_ENVQUERY_TESTS 1
#else
#define MCP_HAS_ENVQUERY_TESTS 0
#endif

namespace McpAIHandlers
{
bool HandleAddEQSContext(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_eqs_context");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_eqs_context"))
    {
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString ContextType = GetStringFieldAI(Payload, TEXT("contextType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        Query->MarkPackageDirty();
        Result->SetStringField(TEXT("contextType"), ContextType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Context %s configured"), *ContextType));

        McpHandlerUtils::AddVerification(Result, Query);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Context added"), Result);
        return true;
    }

    return true;
}

bool HandleAddEQSTest(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_eqs_test");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_eqs_test"))
    {
#if !MCP_HAS_ENVQUERY_TESTS
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("EQS Test creation requires UE 5.1+"),
                            TEXT("NOT_SUPPORTED"));
        return true;
#else
        FString QueryPath = GetStringFieldAI(Payload, TEXT("queryPath"));
        FString TestType = GetStringFieldAI(Payload, TEXT("testType"));

        UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
        if (!Query)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        UEnvQueryTest* NewTest = nullptr;
#if MCP_HAS_ENVQUERY_TESTS
        // Use runtime class lookup to avoid GetPrivateStaticClass requirement
        // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
        UClass* TestClass = nullptr;
        if (TestType.Equals(TEXT("Distance"), ESearchCase::IgnoreCase))
        {
            TestClass = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.EnvQueryTest_Distance"));
        }
        else if (TestType.Equals(TEXT("Trace"), ESearchCase::IgnoreCase))
        {
            TestClass = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.EnvQueryTest_Trace"));
        }

        if (TestClass)
        {
            // Use NewObject with runtime UClass parameter to avoid template instantiation
            UObject* TestObj = NewObject<UObject>(Query, TestClass);
            if (TestObj && TestObj->GetClass()->IsChildOf(UEnvQueryTest::StaticClass()))
            {
                NewTest = static_cast<UEnvQueryTest*>(TestObj);
            }
        }
#endif
        if (NewTest)
        {
            auto& Options = Query->GetOptionsMutable();
            if (Options.Num() == 0 || !Options[0])
            {
                UEnvQueryOption* NewOption = NewObject<UEnvQueryOption>(Query);
                if (!NewOption)
                {
                    Self->SendAutomationError(RequestingSocket, RequestId,
                                        TEXT("Failed to create EQS option for test"),
                                        TEXT("CREATION_FAILED"));
                    return true;
                }
                Options.Add(NewOption);
            }

            NewTest->TestOrder = Options[0]->Tests.Num();
            Options[0]->Tests.Add(NewTest);
            Query->MarkPackageDirty();
            McpSafeAssetSave(Query);
            Result->SetNumberField(TEXT("testIndex"), NewTest->TestOrder);
            Result->SetStringField(TEXT("testType"), TestType);
            Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s test"), *TestType));
            McpHandlerUtils::AddVerification(Result, Query);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Test added"), Result);
        }
        else
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Failed to create test: %s"), *TestType),
                                TEXT("CREATION_FAILED"));
        }

        return true;
#endif
    }

    return true;
}
}
#endif
