#include "Domains/AI/McpAIContext.h"

#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "UObject/UnrealType.h"

namespace
{
// The query's test at a flat index counted across its options, as configure_test_scoring counts them.
UEnvQueryTest* FindEQSTest(UEnvQuery* Query, int32 FlatIndex)
{
    for (UEnvQueryOption* Option : Query->GetOptionsMutable())
    {
        if (!Option)
        {
            continue;
        }
        for (UEnvQueryTest* Test : Option->Tests)
        {
            if (Test && FlatIndex-- == 0)
            {
                return Test;
            }
        }
    }
    return nullptr;
}

// A class found by its AIModule name (EnvQueryContext_Querier) or by path, when it derives from BaseName.
// StaticClass() is not exported for these AIModule classes, so they are looked up.
UClass* FindAIModuleClass(const FString& Name, const TCHAR* BaseName)
{
    const UClass* Base = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), BaseName));
    UClass* Class = Name.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.%s"), *Name));
    Class = Class ? Class : ResolveClassByName(Name);
    return Class && Base && Class->IsChildOf(Base) && !Class->HasAnyClassFlags(CLASS_Abstract) ? Class : nullptr;
}
}

namespace McpAIHandlers
{
// Implements the "add_eqs_context" action: a context reaches a query through the generator or test that reads it (the
// center of a grid, what a distance is measured to), so this sets that property. It used to mark the query dirty and
// answer "Context added".
bool HandleAddEQSContext(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const auto Fail = [&](const FString& Message, const TCHAR* Code)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    const FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    const FString ContextType = GetJsonStringField(Payload, TEXT("contextType"));
    const FString PropertyName = GetJsonStringField(Payload, TEXT("contextProperty"));
    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        return Fail(FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath), TEXT("NOT_FOUND"));
    }

    const bool bCustom = !ContextType.Equals(TEXT("Querier"), ESearchCase::IgnoreCase) && !ContextType.Equals(TEXT("Item"), ESearchCase::IgnoreCase);
    UClass* ContextClass = FindAIModuleClass(bCustom ? GetJsonStringField(Payload, TEXT("contextClass")) : TEXT("EnvQueryContext_") + ContextType, TEXT("EnvQueryContext"));
    if (!ContextClass)
    {
        return Fail(TEXT("contextType is Querier or Item, or Custom with contextClass naming a context class (a Blueprint based on EnvQueryContext_BlueprintBase)."), TEXT("INVALID_ARGUMENT"));
    }

    // The test at testIndex, else the generator of option optionIndex (default 0).
    UObject* Target = nullptr;
    double Index = 0.0;
    if (Payload->TryGetNumberField(TEXT("testIndex"), Index))
    {
        Target = FindEQSTest(Query, static_cast<int32>(Index));
    }
    else
    {
        Payload->TryGetNumberField(TEXT("optionIndex"), Index);
        auto& Options = Query->GetOptionsMutable();
        const int32 OptionIndex = static_cast<int32>(Index);
        if (Options.IsValidIndex(OptionIndex) && Options[OptionIndex])
        {
            UEnvQueryGenerator* Generator = Options[OptionIndex]->Generator;
            Target = Generator;
        }
    }
    if (!Target)
    {
        return Fail(FString::Printf(TEXT("%s has no %s at index %d; add one with %s first."), *QueryPath,
            Payload->HasField(TEXT("testIndex")) ? TEXT("test") : TEXT("generator"), static_cast<int32>(Index),
            Payload->HasField(TEXT("testIndex")) ? TEXT("add_eqs_test") : TEXT("add_eqs_generator")), TEXT("NOT_FOUND"));
    }

    // contextProperty, else the first property that takes a context class.
    const UClass* ContextBase = FindObject<UClass>(nullptr, TEXT("/Script/AIModule.EnvQueryContext"));
    FClassProperty* Property = nullptr;
    TArray<FString> Names;
    for (TFieldIterator<FClassProperty> It(Target->GetClass()); It; ++It)
    {
        if (!It->MetaClass || !It->MetaClass->IsChildOf(ContextBase))
        {
            continue;
        }
        Names.Add(It->GetName());
        if (!Property && (PropertyName.IsEmpty() || It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase)))
        {
            Property = *It;
        }
    }
    if (!Property)
    {
        return Fail(FString::Printf(TEXT("%s has no context property%s; its context properties: %s."), *Target->GetClass()->GetName(),
            PropertyName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" named %s"), *PropertyName),
            Names.Num() > 0 ? *FString::Join(Names, TEXT(", ")) : TEXT("none")), TEXT("NO_CONTEXT_PROPERTY"));
    }

    Target->Modify();
    Property->SetObjectPropertyValue_InContainer(Target, ContextClass);
    Query->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("target"), Target->GetClass()->GetName());
    Result->SetStringField(TEXT("contextProperty"), Property->GetName());
    Result->SetStringField(TEXT("contextClass"), ContextClass->GetPathName());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Query));
    McpHandlerUtils::AddVerification(Result, Query);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%s of %s now reads %s"), *Property->GetName(), *Target->GetClass()->GetName(), *ContextClass->GetName()), Result);
    return true;
}

// Implements the "add_eqs_test" action: any engine test by name (EnvQueryTest_<testType>); only Distance and Trace were
// built, and the other listed types failed.
bool HandleAddEQSTest(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString QueryPath = GetJsonStringField(Payload, TEXT("queryPath"));
    const FString TestType = GetJsonStringField(Payload, TEXT("testType"));
    UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *QueryPath);
    if (!Query)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("EQS Query not found: %s"), *QueryPath), TEXT("NOT_FOUND"));
        return true;
    }
    UClass* TestClass = FindAIModuleClass(TEXT("EnvQueryTest_") + TestType, TEXT("EnvQueryTest"));
    if (!TestClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("No EQS test %s; use Distance, Dot, GameplayTags, Overlap, "
            "Pathfinding, PathfindingBatch, Project, Random or Trace."), *TestType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    Query->Modify();
    auto& Options = Query->GetOptionsMutable();
    if (Options.Num() == 0 || !Options[0])
    {
        Options.Add(NewObject<UEnvQueryOption>(Query));
    }
    UEnvQueryTest* NewTest = CastChecked<UEnvQueryTest>(NewObject<UObject>(Query, TestClass));
    NewTest->TestOrder = Options[0]->Tests.Num();
    Options[0]->Tests.Add(NewTest);
    Query->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("testIndex"), NewTest->TestOrder);
    Result->SetStringField(TEXT("testType"), TestType);
    Result->SetStringField(TEXT("testClass"), TestClass->GetName());
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Query));
    McpHandlerUtils::AddVerification(Result, Query);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Test added"), Result);
    return true;
}
}
