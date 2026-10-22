#include "Misc/AutomationTest.h"
#include "Domains/PCG/Execution/McpPCGExecution.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Components/BoxComponent.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Data/PCGBasePointData.h"
#include "Elements/PCGCreatePoints.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Subsystems/PCGEngineSubsystem.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
class FManualPCGBindingProbe final : public IAutomationLatentCommand
{
public:
    explicit FManualPCGBindingProbe(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FManualPCGBindingProbe()
    {
        if (Component.IsValid())
        {
            if (Component->IsGenerating() || Component->IsCleaningUp() || !NativeIdle())
            {
                Test->AddError(FString::Printf(TEXT("Preserve the owned binding fixture until native idle is established: %s"), *Component->GetPathName()));
                return;
            }
            Component->OnPCGGraphStartGeneratingDelegate.Remove(StartedHandle);
            Component->OnPCGGraphGeneratedDelegate.Remove(GeneratedHandle);
            Component->CleanupLocalImmediate(true);
            Component->DestroyComponent();
            Component.Reset();
        }
        if (Actor.IsValid()) Actor->Destroy();
    }
    bool Update() override
    {
        if (!bInitialized)
        {
            bInitialized = true;
            Deadline = FPlatformTime::Seconds() + 30.0;
            UWorld* World = McpHandlerUtils::GetEditorWorld();
            if (!World || World->WorldType != EWorldType::Editor || FString(FApp::GetProjectName()) != TEXT("PCGValidation"))
            {
                Test->AddError(TEXT("Manual binding probes require the owned PCGValidation editor."));
                return true;
            }
            WorldSubsystem = World->GetSubsystem<UPCGSubsystem>();
            EngineSubsystem = UPCGEngineSubsystem::Get();
            if (!Test->TestTrue(TEXT("Native subsystems start idle"), WorldSubsystem.IsValid() && EngineSubsystem.IsValid() && NativeIdle())) return true;
            FActorSpawnParameters Spawn;
            Spawn.Name = FName(*(TEXT("McpBindingFixture_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
            Spawn.ObjectFlags = RF_Transient;
            Actor = World->SpawnActor<AActor>(Spawn);
            if (!Test->TestTrue(TEXT("The exclusively owned actor was created"), Actor.IsValid())) return true;
            UBoxComponent* Bounds = NewObject<UBoxComponent>(Actor.Get(), NAME_None, RF_Transient);
            Bounds->SetBoxExtent(FVector(100));
            Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Actor->AddInstanceComponent(Bounds);
            Actor->SetRootComponent(Bounds);
            Bounds->RegisterComponent();
            Component.Reset(McpPCGHandlers::CreatePCGComponent(Actor.Get(), TEXT("ManualBindingComponent")));
            if (!Test->TestTrue(TEXT("The existing structured creation helper created a component"), Component.IsValid())) return true;
            const auto StartCount = Starts;
            const auto EndCount = Completions;
            StartedHandle = Component->OnPCGGraphStartGeneratingDelegate.AddLambda([StartCount](UPCGComponent*) { ++*StartCount; });
            GeneratedHandle = Component->OnPCGGraphGeneratedDelegate.AddLambda([EndCount](UPCGComponent*) { ++*EndCount; });
            if (!Test->TestTrue(TEXT("The fresh component starts activated and unpartitioned"), Component->bActivated && !Component->IsPartitioned())) return true;
            Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
            Component->bRegenerateInEditor = false;
            Graph.Reset(NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transactional));
            UPCGSettings* Base = nullptr;
            UPCGNode* Node = Graph->AddNodeOfType(UPCGCreatePointsSettings::StaticClass(), Base);
            auto* Points = Cast<UPCGCreatePointsSettings>(Base);
            const auto& Outputs = Graph->GetOutputNode()->GetInputPins();
            if (!Test->TestTrue(TEXT("A real generation-capable graph exists"), Node && Points && Outputs.Num() == 1 && Outputs[0])) return true;
            Points->PointsToCreate.SetNum(1);
            Points->PointsToCreate[0].Seed = 77;
            Points->bCullPointsOutsideVolume = false;
            if (!Test->TestTrue(TEXT("The native graph has one connected point stream"), Graph->AddEdge(Node, TEXT("Out"), Graph->GetOutputNode(), Outputs[0]->Properties.Label) != nullptr)) return true;
            GraphRevisionBeforeBinding = McpPCGHandlers::GetPCGGraphRevision(Graph.Get());
            if (!Test->TestFalse(TEXT("A complete authored graph revision exists before binding"), GraphRevisionBeforeBinding.IsEmpty())) return true;
            if (!Test->TestEqual(TEXT("Repeated reads before binding have a stable authored revision"), McpPCGHandlers::GetPCGGraphRevision(Graph.Get()), GraphRevisionBeforeBinding)) return true;
            Component->SetGraphLocal(Graph.Get());
            if (!Test->TestEqual(TEXT("Fresh native binding preserves the authored graph revision"), McpPCGHandlers::GetPCGGraphRevision(Graph.Get()), GraphRevisionBeforeBinding)) return true;
            if (!Test->TestTrue(TEXT("Binding did not schedule native work"), NativeIdle() && Component->GetGenerationTaskId() == InvalidPCGTaskId && !Component->IsGenerating() && !Component->IsCleaningUp())) return true;
            if (!Test->TestEqual(TEXT("Binding produced no datasets"), Component->GetGeneratedGraphOutput().TaggedData.Num(), 0)) return true;
            return false;
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            Test->AddError(TEXT("The native binding probe timed out; work is not assumed stopped."));
            return true;
        }
        if (!bRequestedGeneration)
        {
            if (!Test->TestTrue(TEXT("Binding remains idle on normal engine ticks"), NativeIdle() && Starts->load() == 0 && Completions->load() == 0 && !Component->IsGenerating() && !Component->IsCleaningUp())) return true;
            if (LastObservedFrame != GFrameCounter) { LastObservedFrame = GFrameCounter; ++ObservedFrames; }
            if (ObservedFrames < 5) return false;
            if (!Test->TestEqual(TEXT("Authored graph revision stays stable across manual binding frames"), McpPCGHandlers::GetPCGGraphRevision(Graph.Get()), GraphRevisionBeforeBinding)) return true;
            bRequestedGeneration = true;
            return !Test->TestTrue(TEXT("Only an explicit native generation request starts work"), Component->GenerateLocalGetTaskId(true) != InvalidPCGTaskId);
        }
        if (Completions->load() == 0 || Component->IsGenerating() || !NativeIdle()) return false;
        Test->TestEqual(TEXT("Exactly one explicit generation started"), Starts->load(), 1);
        Test->TestEqual(TEXT("Exactly one native completion was observed"), Completions->load(), 1);
        const auto& Data = Component->GetGeneratedGraphOutput().TaggedData;
        if (!Test->TestEqual(TEXT("The generation-capable fixture produced one root"), Data.Num(), 1)) return true;
        const auto* Points = Cast<UPCGBasePointData>(Data[0].Data.Get());
        Test->TestTrue(TEXT("Manual binding did not merely hide an invalid generation fixture"), Points && Points->GetNumPoints() == 1 && Points->GetConstSeedValueRange()[0] == 77);
        return true;
    }
private:
    bool NativeIdle() const
    {
        return WorldSubsystem.IsValid() && EngineSubsystem.IsValid() && !WorldSubsystem->IsAnyGraphCurrentlyExecuting() && !EngineSubsystem->IsAnyGraphCurrentlyExecuting();
    }
    FAutomationTestBase* Test;
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UPCGSubsystem> WorldSubsystem;
    TWeakObjectPtr<UPCGEngineSubsystem> EngineSubsystem;
    TStrongObjectPtr<UPCGComponent> Component;
    TStrongObjectPtr<UPCGGraph> Graph;
    TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> Starts = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(0);
    TSharedRef<std::atomic<int32>, ESPMode::ThreadSafe> Completions = MakeShared<std::atomic<int32>, ESPMode::ThreadSafe>(0);
    FDelegateHandle StartedHandle, GeneratedHandle;
    double Deadline = 0;
    FString GraphRevisionBeforeBinding;
    int32 ObservedFrames = 0;
    uint64 LastObservedFrame = MAX_uint64;
    bool bInitialized = false, bRequestedGeneration = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpManualPCGBindingTest, "EngineWire.PCG.Setup.ManualBindingDoesNotSchedule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpManualPCGBindingTest::RunTest(const FString& Parameters)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FManualPCGBindingProbe>(this));
    return true;
}
#endif
