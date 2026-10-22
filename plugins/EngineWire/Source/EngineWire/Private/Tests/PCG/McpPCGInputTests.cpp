#include "Misc/AutomationTest.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "EngineWireSubsystem.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
namespace
{
struct FInputBudgetFixture
{
    TStrongObjectPtr<UPCGGraph> Graph{NewObject<UPCGGraph>(GetTransientPackage())};
    TStrongObjectPtr<UPCGComponent> Component{NewObject<UPCGComponent>(GetTransientPackage())};
    TSharedPtr<McpPCGHandlers::FPCGOperation> Operation;
    FString Revision;
    double Deadline = FPlatformTime::Seconds() + 5.0;
};

class FWaitInputBudget final : public IAutomationLatentCommand
{
public:
    FWaitInputBudget(FAutomationTestBase* InTest, TSharedPtr<FInputBudgetFixture> InFixture)
        : Test(InTest), Fixture(MoveTemp(InFixture)) {}
    virtual bool Update() override
    {
        using namespace McpPCGHandlers;
        if (!Fixture->Operation->bFinished && FPlatformTime::Seconds() < Fixture->Deadline) return false;
        Test->TestTrue(TEXT("Bounded input failure ends observation"), Fixture->Operation->bFinished);
        Test->TestEqual(TEXT("Incomplete instance capture is not misreported as an authored change"), Fixture->Operation->State, FString(TEXT("input_unavailable")));
        Test->TestEqual(TEXT("Incomplete instance capture has the snapshot refusal code"), Fixture->Operation->ErrorCode, FString(TEXT("PCG_SNAPSHOT_UNAVAILABLE")));
        auto* Bag = Fixture->Component->GetGraphInstance()->GetMutableUserParametersStruct_Unsafe();
        const auto* Desc = Bag->FindPropertyDescByName(TEXT("Values"));
        FScriptArrayHelper Values(CastFieldChecked<FArrayProperty>(Desc->CachedProperty),
            Desc->CachedProperty->ContainerPtrToValuePtr<void>(Bag->GetMutableValue().GetMemory()));
        Values.EmptyValues();
        Test->TestEqual(TEXT("Instance-only fixture does not mutate source graph parameters"), GetPCGGraphRevision(Fixture->Graph.Get()), Fixture->Revision);
        Test->TestFalse(TEXT("Reducing the instance restores complete input capture"), GetPCGInstanceRevision(Fixture->Component.Get()).IsEmpty());
        if (!Fixture->Operation->bFinished) FinishPCGOperation(Fixture->Operation, TEXT("failed"), TEXT("Test-owned cleanup."), TEXT("TEST_CLEANUP"));
        Fixture->Operation->Finished = FPlatformTime::Seconds() - 1810.0;
        FindPCGOperation(Fixture->Operation->Id, TEXT("pcg-input-budget-test"), Fixture->Operation->ComponentPath);
        return true;
    }
private:
    FAutomationTestBase* Test;
    TSharedPtr<FInputBudgetFixture> Fixture;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGInstanceBudgetTest, "EngineWire.PCG.Execution.InstanceBudgetFailureIsNotInputChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGInstanceBudgetTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UEngineWireSubsystem* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Native test editor subsystem"), Bridge)) return false;
    auto Fixture = MakeShared<FInputBudgetFixture>();
    FPropertyBagPropertyDesc Desc(FName(TEXT("Values")), EPropertyBagContainerType::Array, EPropertyBagPropertyType::Double);
    Desc.ID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Graph schema is created on the test-owned graph"), Fixture->Graph->AddUserParameters({Desc}) == EPropertyBagAlterationResult::Success)) return false;
    Fixture->Component->SetGraphLocal(Fixture->Graph.Get());
    if (!TestNotNull(TEXT("Owned native graph instance"), Fixture->Component->GetGraphInstance())) return false;
    Fixture->Revision = GetPCGGraphRevision(Fixture->Graph.Get());
    const FString RequestId = FGuid::NewGuid().ToString();
    auto& Origins = FMcpRequestOriginRegistry::Get();
    Origins.Record(RequestId, ERequestOrigin::NativeHTTP);
    Origins.RecordPrincipal(RequestId, TEXT("pcg-input-budget-test"));
    FString Error;
    Fixture->Operation = BeginPCGOperation(Bridge, RequestId, nullptr, Fixture->Component.Get(), TEXT("generate"), false, false, 30, Error);
    Origins.Forget(RequestId);
    if (!TestTrue(TEXT("Complete test inputs admit observation"), Fixture->Operation.IsValid())) return false;
    // The engine explicitly exposes this mutable API for tests; no private memory offsets or native work are used.
    auto* Bag = Fixture->Component->GetGraphInstance()->GetMutableUserParametersStruct_Unsafe();
    const FProperty* Property = Bag->FindPropertyDescByName(TEXT("Values"))->CachedProperty;
    FScriptArrayHelper Values(CastFieldChecked<FArrayProperty>(Property), Property->ContainerPtrToValuePtr<void>(Bag->GetMutableValue().GetMemory()));
    Values.AddValues(FPCGReadBudget::MaxValues);
    TestFalse(TEXT("Source graph still has a complete revision"), GetPCGGraphRevision(Fixture->Graph.Get()).IsEmpty());
    TestTrue(TEXT("Oversized instance refuses a partial revision"), GetPCGInstanceRevision(Fixture->Component.Get()).IsEmpty());
    StartPCGOperationObserver(Fixture->Operation);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitInputBudget(this, Fixture));
    return true;
}
#endif
