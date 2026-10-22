#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Subsystems/PCGEngineSubsystem.h"

namespace
{
class FWaitSpawnerNativeIdle : public IAutomationLatentCommand
{
public:
    FWaitSpawnerNativeIdle(UPCGEngineSubsystem* InSubsystem, TSharedRef<bool> InDone, FAutomationTestBase* InTest)
        : Subsystem(InSubsystem), Done(InDone), Test(InTest), Deadline(FPlatformTime::Seconds() + 10.0) {}
    virtual bool Update() override
    {
        if (Subsystem.IsValid() && *Done && !Subsystem->IsAnyGraphCurrentlyExecuting()) return true;
        if (!Subsystem.IsValid() || FPlatformTime::Seconds() >= Deadline)
        {
            Test->AddError(TEXT("The native generic idle probe did not settle; no manual executor tick or force cancellation was attempted."));
            return true;
        }
        return false;
    }
private:
    TWeakObjectPtr<UPCGEngineSubsystem> Subsystem;
    TSharedRef<bool> Done;
    FAutomationTestBase* Test;
    double Deadline;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSpawnerBusyTest, "EngineWire.PCG.SpawnerTypes.ScheduledNativeWorkRefusesTypeMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSpawnerBusyTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UPCGEngineSubsystem* Subsystem = UPCGEngineSubsystem::Get();
    if (!TestTrue(TEXT("Initialized native engine subsystem"), Subsystem && Subsystem->GetGraphCompiler())) return false;
    if (!TestFalse(TEXT("No preexisting engine work is adopted by this probe"), Subsystem->IsAnyGraphCurrentlyExecuting())) return false;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Created = nullptr;
    Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Created);
    auto* Settings = Cast<UPCGStaticMeshSpawnerSettings>(Created);
    if (!TestNotNull(TEXT("Owned transient spawner"), Settings)) return false;
    UPCGMeshSelectorBase* Original = Settings->MeshSelectorParameters;
    const FString Before = GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("Complete source revision"), Before.IsEmpty())) return false;
    const bool bDirty = Graph->GetOutermost()->IsDirty();
    const TSharedRef<bool> Done = MakeShared<bool>(false);
    const FPCGTaskId Task = Subsystem->ScheduleGeneric(FPCGScheduleGenericParams([Done](FPCGContext*) { *Done = true; return true; }, nullptr));
    if (!TestTrue(TEXT("One native, context-free task scheduled"), Task != InvalidPCGTaskId)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FWaitSpawnerNativeIdle(Subsystem, Done, this));
    TestTrue(TEXT("Scheduled work counts as native busy before execution"), Subsystem->IsAnyGraphCurrentlyExecuting());
    auto Fields = MakeShared<FJsonObject>();
    Fields->SetStringField(TEXT("MeshSelectorType"), TEXT("PCGMeshSelectorByAttribute"));
    FString Error;
    int32 Applied = 0;
    TestFalse(TEXT("Type mutation refused during actual native scheduled work"), ApplySettingsObject(Settings, Fields, Error, Applied));
    TestTrue(TEXT("Refusal explains the idle requirement"), Error.Contains(TEXT("idle")));
    TestEqual(TEXT("Original selector preserved"), Settings->MeshSelectorParameters.Get(), Original);
    TestEqual(TEXT("Graph revision preserved"), GetPCGGraphRevision(Graph.Get()), Before);
    TestEqual(TEXT("Package dirty state preserved"), Graph->GetOutermost()->IsDirty(), bDirty);
    return true;
}
#endif
