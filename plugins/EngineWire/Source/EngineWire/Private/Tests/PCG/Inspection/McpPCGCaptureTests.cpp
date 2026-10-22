#include "Tests/PCG/Inspection/McpPCGCaptureFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
#include "Data/PCGBasePointData.h"
#include "PCGPin.h"
#include "UObject/Package.h"

namespace McpPCGCaptureTests
{
namespace
{
bool CheckPoints(FAutomationTestBase* Test, const FPCGDataCollection& Data, double Offset)
{
    if (!Test->TestEqual(TEXT("Actual captured pin has exactly one dataset"), Data.TaggedData.Num(), 1)) return false;
    const UPCGBasePointData* Points = Cast<UPCGBasePointData>(Data.TaggedData[0].Data.Get());
    if (!Test->TestNotNull(TEXT("Actual captured dataset is native point data"), Points) ||
        !Test->TestEqual(TEXT("Actual captured pin preserves two points"), Points->GetNumPoints(), 2)) return false;
    const auto Transforms = Points->GetConstTransformValueRange();
    const auto Seeds = Points->GetConstSeedValueRange();
    bool bOk = Test->TestEqual(TEXT("First captured coordinate"), Transforms[0].GetLocation(), FVector(12.5 + Offset, 3, 4));
    bOk &= Test->TestEqual(TEXT("Second captured coordinate"), Transforms[1].GetLocation(), FVector(-7 + Offset, 8, 9));
    bOk &= Test->TestEqual(TEXT("First seed survives the transform"), Seeds[0], 293);
    bOk &= Test->TestEqual(TEXT("Second seed survives the transform"), Seeds[1], -17);
    return bOk;
}

bool FindExecuted(FAutomationTestBase* Test, const FFixture& Fixture, FPCGStack& OutStack)
{
    auto& Inspection = Fixture.GetComponent()->GetExecutionState().GetInspection();
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    const auto Executed = Inspection.GetExecutedNodeStacks(Fixture.GetTransform());
#else
    // The public UE 5.7 API copies the full map; this spike owns a fixed tiny graph, not an arbitrary production graph.
    const auto All = Inspection.GetExecutedNodeStacks();
    if (!Test->TestTrue(TEXT("Known tiny fixture has a bounded native executed-node inventory"), All.Num() <= 12)) return false;
    const auto* Found = All.Find(TObjectKey<const UPCGNode>(Fixture.GetTransform()));
    if (!Test->TestNotNull(TEXT("Actual transform execution is present"), Found)) return false;
    const auto& Executed = *Found;
#endif
    if (!Test->TestEqual(TEXT("Transform has one actual execution context"), Executed.Num(), 1)) return false;
    for (const auto& Entry : Executed)
    {
        OutStack = Entry.Stack;
        if (!Test->TestTrue(TEXT("Native CPU execution/prepare metrics are finite and nonnegative"),
            FMath::IsFinite(Entry.Timer.ExecutionTime) && Entry.Timer.ExecutionTime >= 0 &&
            FMath::IsFinite(Entry.Timer.PrepareDataTime) && Entry.Timer.PrepareDataTime >= 0)) return false;
        Test->AddInfo(FString::Printf(TEXT("Observed actual CPU total %.17g; zero timers alone are not cache-hit proof."), Entry.Timer.TotalTime()));
    }
    bool bOk = Test->TestTrue(TEXT("Actual stack belongs to the exact component"), OutStack.HasObject(Fixture.GetComponent()));
    bOk &= Test->TestTrue(TEXT("Actual stack contains the root graph"), OutStack.HasObject(Fixture.GetGraph()));
    bOk &= Test->TestEqual(TEXT("Stack's current graph is the actual graph containing the transform"),
        OutStack.GetGraphForCurrentFrame(), static_cast<const UPCGGraph*>(Fixture.GetChild() ? Fixture.GetChild() : Fixture.GetGraph()));
    bOk &= Test->TestEqual(TEXT("Nested stack depth is native, not synthesized"), OutStack.GetNumGraphLevels(), Fixture.GetChild() ? uint32(2) : uint32(1));
    if (Fixture.GetChild()) bOk &= Test->TestTrue(TEXT("Nested stack includes the exact parent subgraph node"), OutStack.HasObject(Fixture.GetSubgraph()));
    return bOk;
}

FPCGStack PinStack(const FPCGStack& Stack, UPCGNode* Node, const UPCGPin* Pin)
{
    FPCGStack Result = Stack;
    Result.PushFrame(Node);
    Result.PushFrame(Pin);
    return Result;
}

bool CheckCaptured(FAutomationTestBase* Test, const FFixture& Fixture, FPCGStack& OutStack)
{
    if (!FindExecuted(Test, Fixture, OutStack)) return false;
    UPCGNode* Node = Fixture.GetTransform();
    const UPCGPin* Input = Node->GetInputPin(TEXT("In"));
    const UPCGPin* Output = Node->GetOutputPin(TEXT("Out"));
    if (!Input || !Output) return false;
    auto& Inspection = Fixture.GetComponent()->GetExecutionState().GetInspection();
    const FString GraphRevision = McpPCGHandlers::GetPCGGraphRevision(Fixture.GetGraph());
    const FString InstanceRevision = McpPCGHandlers::GetPCGInstanceRevision(Fixture.GetComponent());
    if (!Test->TestFalse(TEXT("Graph read-invariance fingerprint is present"), GraphRevision.IsEmpty()) ||
        !Test->TestFalse(TEXT("Instance read-invariance fingerprint is present"), InstanceRevision.IsEmpty())) return false;
    const bool bGraphDirty = Fixture.GetGraph()->GetOutermost()->IsDirty();
    bool bValues = true;
    bool bOk = Test->TestTrue(TEXT("Real transform input pin was captured"), Visit(Inspection, PinStack(OutStack, Node, Input),
        [&](const FPCGDataCollection& Data) { bValues &= CheckPoints(Test, Data, 0); }));
    bOk &= Test->TestTrue(TEXT("Real transform output pin was captured"), Visit(Inspection, PinStack(OutStack, Node, Output),
        [&](const FPCGDataCollection& Data) { bValues &= CheckPoints(Test, Data, 11); }));
    bOk &= CheckPoints(Test, Fixture.GetComponent()->GetGeneratedGraphOutput(), 11);
    bOk &= Test->TestTrue(TEXT("Native node execution is separately reported"), Inspection.WasNodeExecuted(Node, OutStack));
    bOk &= Test->TestTrue(TEXT("Native produced-data state is separately reported"), Inspection.HasNodeProducedData(Node, OutStack));
    bOk &= Test->TestEqual(TEXT("Pin reads preserve graph revision"), McpPCGHandlers::GetPCGGraphRevision(Fixture.GetGraph()), GraphRevision);
    bOk &= Test->TestEqual(TEXT("Pin reads preserve instance revision"), McpPCGHandlers::GetPCGInstanceRevision(Fixture.GetComponent()), InstanceRevision);
    bOk &= Test->TestEqual(TEXT("Pin reads preserve package dirty state"), Fixture.GetGraph()->GetOutermost()->IsDirty(), bGraphDirty);
    return bOk && bValues;
}

class FCaptureCommand final : public IAutomationLatentCommand
{
public:
    FCaptureCommand(FAutomationTestBase* InTest, bool bInNested) : Test(InTest), Fixture(InTest), bNested(bInNested) {}
    bool Update() override
    {
        if (Phase == 0)
        {
            Deadline = FPlatformTime::Seconds() + 150;
            if (!Test->TestTrue(TEXT("Owned real capture fixture schedules"), Fixture.Start(bNested))) return true;
            Phase = 1;
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            Test->AddError(TEXT("Native capture observation timed out; active fixture is preserved, not force-stopped."));
            return true;
        }
        if (!Fixture.Ready()) return false;
        auto& Inspection = Fixture.GetComponent()->GetExecutionState().GetInspection();
        FPCGStack Stack;
        if (Phase == 1)
        {
            if (!CheckCaptured(Test, Fixture, Stack)) return true;
            Fixture.ReleaseOne();
            if (!Test->TestTrue(TEXT("Releasing one acquisition preserves the other observer"), Inspection.IsInspecting())) return true;
            if (!CheckCaptured(Test, Fixture, Stack)) return true;
            if (!Test->TestTrue(TEXT("Repeat generation schedules through the same native lifecycle"), Fixture.Schedule())) return true;
            Phase = 2;
            return false;
        }
        if (Phase == 2)
        {
            if (!CheckCaptured(Test, Fixture, Stack)) return true;
            const FPCGStack Output = PinStack(Stack, Fixture.GetTransform(), Fixture.GetTransform()->GetOutputPin(TEXT("Out")));
            Fixture.ReleaseOne();
            Test->TestFalse(TEXT("Final balanced release restores the initial disabled state"), Inspection.IsInspecting());
            Test->TestFalse(TEXT("Final release removes real retained output pin data"), Visit(Inspection, Output, [](const FPCGDataCollection&) {}));
            Test->TestTrue(TEXT("Capture absence is not confused with node non-execution"), Inspection.WasNodeExecuted(Fixture.GetTransform(), Stack));
            if (!Test->TestTrue(TEXT("No-capture native generation schedules"), Fixture.Schedule())) return true;
            Phase = 3;
            return false;
        }
        if (!FindExecuted(Test, Fixture, Stack)) return true;
        const FPCGStack Output = PinStack(Stack, Fixture.GetTransform(), Fixture.GetTransform()->GetOutputPin(TEXT("Out")));
        Test->TestFalse(TEXT("Generation without an acquisition does not retain pin data"), Visit(Inspection, Output, [](const FPCGDataCollection&) {}));
        Test->TestFalse(TEXT("Reading absent capture does not enable inspection"), Inspection.IsInspecting());
        CheckPoints(Test, Fixture.GetComponent()->GetGeneratedGraphOutput(), 11);
        return true;
    }
private:
    FAutomationTestBase* Test;
    FFixture Fixture;
    bool bNested;
    int32 Phase = 0;
    double Deadline = 0;
};
}

void RunRealCapture(FAutomationTestBase* Test, bool bNested)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCaptureCommand>(Test, bNested));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGMainCaptureTest, "EngineWire.PCG.InspectionCapture.RealMainGraphAndOwnedRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGMainCaptureTest::RunTest(const FString& Parameters)
{
    McpPCGCaptureTests::RunRealCapture(this, false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSubgraphCaptureTest, "EngineWire.PCG.InspectionCapture.RealSubgraphAndMissingCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSubgraphCaptureTest::RunTest(const FString& Parameters)
{
    McpPCGCaptureTests::RunRealCapture(this, true);
    return true;
}
#endif
#endif
