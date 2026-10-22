#pragma once

#include "Misc/AutomationTest.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
namespace McpPCGRootTests
{
class FFixture
{
public:
    explicit FFixture(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FFixture();
    bool Start(int32 Count, const FString& Tag);
    bool Regenerate();
    bool Ready() const;
    bool IsPending() const { return bPending; }
    void FinishWait() { bPending = false; }
    void DestroyOwned();
    UPCGComponent* GetComponent() const { return Component.Get(); }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AActor> Actor;
    TStrongObjectPtr<UPCGComponent> Component;
    TStrongObjectPtr<UPCGGraph> Graph;
    TSharedRef<std::atomic<bool>, ESPMode::ThreadSafe> Generated = MakeShared<std::atomic<bool>, ESPMode::ThreadSafe>(false);
    FDelegateHandle GeneratedHandle;
    bool bPending = false;
};
// Own actor/graph only; real native generation is observed on engine ticks, never manually pumped.
void Run(FAutomationTestBase* Test, TFunction<bool(FFixture&)> Body, int32 Count = 1, const FString& Tag = TEXT("owned"));
}
#endif
