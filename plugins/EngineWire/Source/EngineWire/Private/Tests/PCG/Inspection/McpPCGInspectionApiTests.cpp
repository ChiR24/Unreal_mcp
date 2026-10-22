#include "Misc/AutomationTest.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "PCGGraphExecutionInspection.h"
#include "Data/PCGPointArrayData.h"
#include "PCGPin.h"
#include "Utils/PCGExtraCapture.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
namespace
{
bool VisitOwnedInspection(const FPCGGraphExecutionInspection& Inspection, const FPCGStack& Stack,
    TFunctionRef<void(const FPCGDataCollection&)> Visitor)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    return Inspection.InspectData(Stack, Visitor);
#else
    // This test is synchronous on a newly owned fixture; no native regeneration or cache mutation can interleave.
    if (const FPCGDataCollection* Data = Inspection.GetInspectionData(Stack))
    {
        Visitor(*Data);
        return true;
    }
    return false;
#endif
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGInspectionOwnershipProbe, "EngineWire.PCG.InspectionApi.BalancedOwnedReferenceCounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGInspectionOwnershipProbe::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(GetTransientPackage()));
    auto& Inspection = Component->GetExecutionState().GetInspection();
    TestFalse(TEXT("A newly owned fixture starts without another inspection acquisition"), Inspection.IsInspecting());
    int32 Acquired = 0;
    ON_SCOPE_EXIT { while (Acquired > 0) { Inspection.DisableInspection(); --Acquired; } };
    Inspection.EnableInspection();
    ++Acquired;
    TestTrue(TEXT("Public execution-state acquisition enables inspection"), Inspection.IsInspecting());
    Inspection.EnableInspection();
    ++Acquired;
    Inspection.DisableInspection();
    --Acquired;
    TestTrue(TEXT("Releasing one acquisition preserves the other owner's acquisition"), Inspection.IsInspecting());
    Inspection.DisableInspection();
    --Acquired;
    TestFalse(TEXT("Final balanced release restores the owned fixture's initial state"), Inspection.IsInspecting());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGInspectionDataProbe, "EngineWire.PCG.InspectionApi.SyntheticPinStorageAndTimerLinkage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGInspectionDataProbe::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(GetTransientPackage()));
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    Component->SetGraph(Graph.Get());
    UClass* Class = McpPCGHandlers::ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Native fixture class is available"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
    if (!TestNotNull(TEXT("Owned fixture node exists"), Node)) return false;
    if (!TestTrue(TEXT("Fixture node has actual native input/output pins"), !Node->GetInputPins().IsEmpty() && !Node->GetOutputPins().IsEmpty())) return false;
    auto& Inspection = Component->GetExecutionState().GetInspection();
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Data->SetNumPoints(1);
    Data->GetSeedValueRange()[0] = -313;
    FPCGDataCollection Input, Output;
    auto& In = Input.TaggedData.Emplace_GetRef();
    In.Data = Data.Get();
    In.Pin = Node->GetInputPins()[0]->Properties.Label;
    auto& Out = Output.TaggedData.Emplace_GetRef();
    Out.Data = Data.Get();
    Out.Pin = Node->GetOutputPins()[0]->Properties.Label;
    FPCGStack Stack;
    Stack.PushFrame(Component.Get());
    Stack.PushFrame(Graph.Get());
    FPCGStack PinStack = Stack;
    PinStack.PushFrame(Node);
    PinStack.PushFrame(Node->GetOutputPins()[0]);
    Inspection.EnableInspection();
    bool bAcquired = true;
    ON_SCOPE_EXIT { if (bAcquired) Inspection.DisableInspection(); };
    PCGUtils::FCallTime Timer;
    Timer.PrepareDataStartTime = 1.0;
    Timer.PrepareDataEndTime = 1.25;
    Timer.ExecutionStartTime = 1.25;
    Timer.ExecutionEndTime = 2.0;
    Timer.PrepareDataTime = 0.125;
    Timer.ExecutionTime = 0.5;
    // Only this bounded test-owned collection computes native CRCs; production data readers never do so.
    Input.ComputeCrcs(false);
    Output.ComputeCrcs(false);
    // Synthetic public storage proves API linkage/semantics only, never actual graph execution or profiler availability.
    Inspection.StoreInspectionData(&Stack, Node, &Timer, Input, Output, false);
    bool bVisited = false;
    TestTrue(TEXT("Version-appropriate public pin read finds stored data"), VisitOwnedInspection(Inspection, PinStack, [&](const FPCGDataCollection& Collection)
    {
        bVisited = true;
        if (!TestEqual(TEXT("Exactly one owned synthetic dataset is stored"), Collection.TaggedData.Num(), 1)) return;
        const auto* Points = Cast<UPCGBasePointData>(Collection.TaggedData[0].Data.Get());
        if (TestNotNull(TEXT("Stored data preserves its actual point type"), Points))
            TestEqual(TEXT("Stored point value is exact"), Points->GetConstSeedValueRange()[0], -313);
    }));
    TestTrue(TEXT("The visitor actually ran"), bVisited);
    TestTrue(TEXT("The exact node/stack was recorded"), Inspection.WasNodeExecuted(Node, Stack));
    TestTrue(TEXT("Output availability is explicit"), Inspection.HasNodeProducedData(Node, Stack));
    const auto Executed = Inspection.GetExecutedNodeStacks();
    TestEqual(TEXT("Only the bounded owned node is enumerated"), Executed.Num(), 1);
    const auto* Nodes = Executed.Find(TObjectKey<const UPCGNode>(Node));
    if (!TestNotNull(TEXT("Owned node's timer entry exists"), Nodes)) return false;
    TestEqual(TEXT("Only one owned stack exists"), Nodes->Num(), 1);
    for (const auto& Entry : *Nodes)
    {
        TestEqual(TEXT("Native timer preserves execution CPU time"), Entry.Timer.ExecutionTime, 0.5);
        TestEqual(TEXT("Native timer total is CPU execution plus prepare, not wall time"), Entry.Timer.TotalTime(), 0.625);
        TestEqual(TEXT("Native timer wall time is a separately defined duration"), Entry.Timer.TotalWallTime(), 1.0);
    }
    const auto Tree = Component->GetExecutionState().GetExtraCapture().CalculateCallTreeInfo(Component.Get(), Stack);
    TestTrue(TEXT("Public call-tree calculation links on a bounded owned state"), FMath::IsFinite(Tree.CallTime.ExecutionTime));
    Inspection.DisableInspection();
    bAcquired = false;
    TestFalse(TEXT("Final release restores disabled state"), Inspection.IsInspecting());
    TestFalse(TEXT("Final release removes this fixture's retained pin data"), VisitOwnedInspection(Inspection, PinStack, [](const FPCGDataCollection&) {}));
    return true;
}
#endif
#endif
