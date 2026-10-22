#pragma once

#include "Misc/AutomationTest.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "PCGGraphExecutionInspection.h"
#include "UObject/StrongObjectPtr.h"

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
namespace McpPCGCaptureTests
{
class FFixture
{
public:
    explicit FFixture(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FFixture();
    bool Start(bool bNested);
    bool Schedule();
    bool Ready() const;
    void ReleaseOne();
    UPCGComponent* GetComponent() const { return Component.Get(); }
    UPCGNode* GetTransform() const { return Transform.Get(); }
    UPCGGraph* GetGraph() const { return Graph.Get(); }
    UPCGGraph* GetChild() const { return Child.Get(); }
    UPCGNode* GetSubgraph() const { return Subgraph.Get(); }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AActor> Actor;
    TStrongObjectPtr<UPCGComponent> Component;
    TStrongObjectPtr<UPCGGraph> Graph, Child;
    TWeakObjectPtr<UPCGNode> Transform, Subgraph;
    TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Generated = MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
    FDelegateHandle GeneratedHandle;
    int32 Acquisitions = 0;
};

// No native collections or ranges escape the synchronous visitor; UE 5.7 reads require native idle on GT.
bool Visit(const FPCGGraphExecutionInspection& Inspection, const FPCGStack& Stack,
    TFunctionRef<void(const FPCGDataCollection&)> Visitor);
}
#endif
#endif
