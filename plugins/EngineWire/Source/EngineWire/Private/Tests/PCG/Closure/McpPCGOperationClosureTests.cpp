#include "Misc/AutomationTest.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "PCGManagedResource.h"
#include "Engine/Engine.h"
namespace
{
TSharedPtr<McpPCGHandlers::FPCGOperation> BeginClosureOperation(UEngineWireSubsystem* Bridge, UPCGComponent* Component)
{
    const FString Request = FGuid::NewGuid().ToString();
    auto& Origins = FMcpRequestOriginRegistry::Get();
    Origins.Record(Request, ERequestOrigin::NativeHTTP);
    Origins.RecordPrincipal(Request, TEXT("pcg-closeout-test"));
    FString Error;
    auto Op = McpPCGHandlers::BeginPCGOperation(Bridge, Request, nullptr, Component, TEXT("cleanup"), false, false, 60, Error);
    Origins.Forget(Request);
    return Op;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGObserverShutdownTest, "EngineWire.PCG.Closeout.ObserverShutdownDetachesOnlyOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGObserverShutdownTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    TStrongObjectPtr<UEngineWireSubsystem> Owner(NewObject<UEngineWireSubsystem>(GEngine));
    TStrongObjectPtr<UEngineWireSubsystem> Other(NewObject<UEngineWireSubsystem>(GEngine));
    TStrongObjectPtr<UPCGComponent> A(NewObject<UPCGComponent>()), B(NewObject<UPCGComponent>());
    auto First = BeginClosureOperation(Owner.Get(), A.Get());
    auto Second = BeginClosureOperation(Other.Get(), B.Get());
    if (!TestTrue(TEXT("Both isolated operations are admitted"), First.IsValid() && Second.IsValid()))
    {
        ShutdownPCGOperations(Owner.Get()); ShutdownPCGOperations(Other.Get()); return false;
    }
    StartPCGOperationObserver(First);
    StartPCGOperationObserver(Second);
    TestTrue(TEXT("The first observer owns a ticker"), First->Ticker.IsValid());
    ShutdownPCGOperations(Owner.Get());
    TestFalse(TEXT("Owner ticker is detached"), First->Ticker.IsValid());
    TestFalse(TEXT("Owner observation is detached"), First->bWatching);
    TestFalse(TEXT("Owner lease is released"), HasActivePCGOperation(A.Get()));
    TestNull(TEXT("Owner records do not survive subsystem teardown"), FindPCGOperation(First->Id, TEXT("pcg-closeout-test"), First->ComponentPath).Get());
    A->OnPCGGraphGeneratedDelegate.Broadcast(A.Get());
    TestFalse(TEXT("A late event cannot invoke the removed delegate"), First->bGenerated.load());
    TestFalse(TEXT("Observation teardown never requests native cancellation"), First->bCancelRequested);
    TestTrue(TEXT("A different subsystem keeps its observation"), Second->bWatching && Second->Ticker.IsValid() && HasActivePCGOperation(B.Get()));
    B->OnPCGGraphGeneratedDelegate.Broadcast(B.Get());
    TestTrue(TEXT("The other subsystem still receives its native event"), Second->bGenerated.load());
    ShutdownPCGOperations(Other.Get());
    ShutdownPCGOperations(Owner.Get());
    TestFalse(TEXT("Other ticker is detached and repeated teardown is harmless"), Second->Ticker.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGManagedInventoryBoundsTest, "EngineWire.PCG.Closeout.ManagedInventoryRefusesOversizedReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGManagedInventoryBoundsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGComponent> Source(NewObject<UPCGComponent>());
    auto* Resource = NewObject<UPCGManagedComponentList>(Source.Get());
    Resource->GeneratedComponents.SetNum(FPCGReadBudget::MaxObjects + 1);
    Source->AddToManagedResources(Resource);
    TestNull(TEXT("An oversized native reference list publishes no partial inventory"), ReadPCGManagedResults(Source.Get(), 0, 1, 0).Get());
    Resource->GeneratedComponents.Reset();
    auto Empty = ReadPCGManagedResults(Source.Get(), MAX_int32, 100, 0);
    if (!TestTrue(TEXT("A small native inventory remains readable"), Empty.IsValid())) return false;
    TestEqual(TEXT("Exact complete resource count is retained"), Empty->GetIntegerField(TEXT("managedResourceCount")), 1);
    TestEqual(TEXT("The off-end component page is empty"), Empty->GetArrayField(TEXT("components")).Num(), 0);
    TestFalse(TEXT("The component overflow-offset page has no more rows"), Empty->GetBoolField(TEXT("componentsTruncated")));
    TestFalse(TEXT("The actor overflow-offset page has no more rows"), Empty->GetBoolField(TEXT("actorsTruncated")));
    TestFalse(TEXT("The output overflow-offset page has no more rows"), Empty->GetBoolField(TEXT("outputTruncated")));
    return true;
}
#endif
