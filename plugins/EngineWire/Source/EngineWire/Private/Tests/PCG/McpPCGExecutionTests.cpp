#include "Misc/AutomationTest.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
namespace
{
constexpr int32 TestActiveLimit = 64;
constexpr int32 TestCompletedLimit = 128;
constexpr double TestCompletedTtl = 1800.0;
const FString TestPrincipal = TEXT("pcg-operation-store-automation-test");

TSharedPtr<McpPCGHandlers::FPCGOperation> BeginTestOperation(UPCGComponent* Component, FString& OutError)
{
    const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    FMcpRequestOriginRegistry& Origins = FMcpRequestOriginRegistry::Get();
    Origins.Record(RequestId, ERequestOrigin::NativeHTTP);
    Origins.RecordPrincipal(RequestId, TestPrincipal);
    auto Operation = McpPCGHandlers::BeginPCGOperation(nullptr, RequestId, nullptr, Component,
        TEXT("generate"), false, false, 60.0, OutError);
    Origins.Forget(RequestId);
    return Operation;
}

void FinishTestOperation(const TSharedPtr<McpPCGHandlers::FPCGOperation>& Operation)
{
    if (Operation.IsValid() && !Operation->bFinished)
    {
        McpPCGHandlers::FinishPCGOperation(Operation, TEXT("failed"), TEXT("Automation-test cleanup."), TEXT("TEST_CLEANUP"));
    }
}

void ExpireTestOperations(const TArray<TSharedPtr<McpPCGHandlers::FPCGOperation>>& Operations)
{
    const double ExpiredAt = FPlatformTime::Seconds() - TestCompletedTtl - 10.0;
    for (const auto& Operation : Operations)
    {
        if (Operation.IsValid() && Operation->bFinished && !Operation->bWatching)
        {
            Operation->Finished = ExpiredAt;
        }
    }
    if (Operations.Num() > 0)
    {
        // Find invokes the store's normal lazy purge; no global reset or test-only store access.
        McpPCGHandlers::FindPCGOperation(FGuid::NewGuid().ToString(), TestPrincipal, TEXT("/Transient/unused"));
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGOperationActiveLimitTest, "EngineWire.PCG.Execution.ActiveLimitAndPrincipalOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGOperationActiveLimitTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TArray<TStrongObjectPtr<UPCGComponent>> Components;
    TArray<TSharedPtr<FPCGOperation>> Operations;
    bool bCapacityReachedBeforeLimit = false;

    for (int32 Index = 0; Index < TestActiveLimit; ++Index)
    {
        TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(GetTransientPackage()));
        if (!TestNotNull(TEXT("Test component"), Component.Get())) break;
        FString Error;
        auto Operation = BeginTestOperation(Component.Get(), Error);
        if (!Operation.IsValid())
        {
            if (Error.Contains(TEXT("bounded operation capacity is full")))
            {
                AddWarning(TEXT("Pre-existing active PCG work prevented an isolated 64-slot capacity check; only test-owned operations were released."));
                bCapacityReachedBeforeLimit = true;
            }
            else TestTrue(TEXT("Each test-owned component admits a native operation"), Operation.IsValid());
            break;
        }
        Components.Add(MoveTemp(Component));
        Operations.Add(Operation);
    }

    if (!bCapacityReachedBeforeLimit && Operations.Num() == TestActiveLimit)
    {
        TestTrue(TEXT("All 64 active slots are occupied by this test"), Operations.Num() == TestActiveLimit);
        TestTrue(TEXT("Owner can find its active operation"), FindPCGOperation(Operations[0]->Id, TestPrincipal, Operations[0]->ComponentPath).IsValid());
        TestNull(TEXT("A different principal cannot find an active operation"), FindPCGOperation(Operations[0]->Id, TEXT("other-principal"), Operations[0]->ComponentPath).Get());
        TestNull(TEXT("The owner cannot find the operation under another component path"), FindPCGOperation(Operations[0]->Id, TestPrincipal, Operations[0]->ComponentPath + TEXT("_other")).Get());
        TestTrue(TEXT("The owner can find its active component lease"), FindActivePCGOperation(Components[0].Get(), TestPrincipal) == Operations[0]);
        TestNull(TEXT("A different principal cannot find the active component lease"), FindActivePCGOperation(Components[0].Get(), TEXT("other-principal")).Get());

        TStrongObjectPtr<UPCGComponent> OverflowComponent(NewObject<UPCGComponent>(GetTransientPackage()));
        FString Error;
        auto Overflow = BeginTestOperation(OverflowComponent.Get(), Error);
        TestNull(TEXT("The 65th active operation is refused"), Overflow.Get());
        TestTrue(TEXT("Capacity refusal is reported"), Error.Contains(TEXT("bounded operation capacity is full")));
    }

    for (const auto& Operation : Operations) FinishTestOperation(Operation);
    ExpireTestOperations(Operations);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGOperationCompletedRetentionTest, "EngineWire.PCG.Execution.CompletedRetentionAndPrincipalOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGOperationCompletedRetentionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(GetTransientPackage()));
    if (!TestNotNull(TEXT("Test component"), Component.Get())) return false;

    TArray<TSharedPtr<FPCGOperation>> Operations;
    Operations.Reserve(TestCompletedLimit + 1);
    for (int32 Index = 0; Index < TestCompletedLimit; ++Index)
    {
        FString Error;
        auto Operation = BeginTestOperation(Component.Get(), Error);
        if (!Operation.IsValid())
        {
            if (Error.Contains(TEXT("bounded operation capacity is full")))
            {
                AddWarning(TEXT("Pre-existing active PCG work prevented the completed-retention check; only test-owned operations were released."));
            }
            else TestTrue(TEXT("A completed test operation can be admitted"), Operation.IsValid());
            break;
        }
        Operations.Add(Operation);
        FinishTestOperation(Operation);
    }

    if (Operations.Num() == TestCompletedLimit)
    {
        TestTrue(TEXT("At the 128-entry boundary, the oldest test operation remains"),
            FindPCGOperation(Operations[0]->Id, TestPrincipal, Operations[0]->ComponentPath).IsValid());
        TestTrue(TEXT("The newest operation remains at the 128-entry boundary"),
            FindPCGOperation(Operations[TestCompletedLimit - 1]->Id, TestPrincipal, Operations[TestCompletedLimit - 1]->ComponentPath).IsValid());
        TestNull(TEXT("A different principal cannot read a retained completion"),
            FindPCGOperation(Operations[1]->Id, TEXT("other-principal"), Operations[1]->ComponentPath).Get());
        TestNull(TEXT("A retained completion is bound to its original component path"),
            FindPCGOperation(Operations[1]->Id, TestPrincipal, Operations[1]->ComponentPath + TEXT("_other")).Get());

        FString Error;
        auto Overflow = BeginTestOperation(Component.Get(), Error);
        TestTrue(TEXT("The 129th completed test operation can be admitted"), Overflow.IsValid());
        if (Overflow.IsValid())
        {
            Operations.Add(Overflow);
            FinishTestOperation(Overflow);
            TestNull(TEXT("The 129th completion evicts the oldest operation"),
                FindPCGOperation(Operations[0]->Id, TestPrincipal, Operations[0]->ComponentPath).Get());
            for (int32 Index = 1; Index <= TestCompletedLimit; ++Index)
            {
                TestTrue(TEXT("The newest 128 completed operations remain queryable"),
                    FindPCGOperation(Operations[Index]->Id, TestPrincipal, Operations[Index]->ComponentPath).IsValid());
            }
        }
    }

    if (Operations.Num() > 0)
    {
        auto TtlOperation = Operations.Last();
        const double Now = FPlatformTime::Seconds();
        TtlOperation->Finished = Now - (TestCompletedTtl - 10.0);
        TestTrue(TEXT("A completed operation younger than the TTL remains retained"),
            FindPCGOperation(TtlOperation->Id, TestPrincipal, TtlOperation->ComponentPath).IsValid());
        TtlOperation->Finished = FPlatformTime::Seconds() - (TestCompletedTtl + 10.0);
        TestNull(TEXT("A completed operation older than the TTL expires on lookup"),
            FindPCGOperation(TtlOperation->Id, TestPrincipal, TtlOperation->ComponentPath).Get());
    }

    ExpireTestOperations(Operations);
    return true;
}
#endif
