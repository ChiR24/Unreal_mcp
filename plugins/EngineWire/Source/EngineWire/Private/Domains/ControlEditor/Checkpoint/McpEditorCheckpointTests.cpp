#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Requests/McpUndoLedger.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"

namespace
{
FMcpCapturedResponse Run(UEngineWireSubsystem* Subsystem, const FString& Op)
{
    const FString Id = FString::Printf(TEXT("checkpoint-test-%s"), *Op);
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("op"), Op);
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    McpEditorCheckpoint::Handle(Subsystem, Id, Payload, nullptr);
    return FMcpResponseCaptureRegistry::Get().End(Id);
}

// One step that tags the probe, made inside a recorded request when bMcp, outside any request (the user) otherwise.
void Step(USceneComponent* Probe, const FName Tag, const bool bMcp)
{
    // The run_build request that runs this test may itself be recording: a user step is made outside any.
    McpUndoLedger::End();
    if (bMcp)
    {
        McpUndoLedger::Begin(TEXT("checkpoint-test-step"));
    }
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp Checkpoint Probe")));
        Probe->Modify();
        Probe->ComponentTags.Add(Tag);
    }
    if (bMcp)
    {
        McpUndoLedger::End();
    }
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpEditorCheckpointTest, "EngineWire.ControlEditor.RestorePoint.RollsBackOnlyMcpSteps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpEditorCheckpointTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UEngineWireSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!Subsystem || !GEditor->Trans)
    {
        AddError(TEXT("No EngineWire subsystem or undo buffer."));
        return false;
    }
    Run(Subsystem, TEXT("end")); // a checkpoint a previous run left open
    GEditor->Trans->Reset(FText::FromString(TEXT("Mcp checkpoint test")));
    USceneComponent* Probe = NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transactional);

    TestEqual(TEXT("rollback with none open is refused"), Run(Subsystem, TEXT("rollback")).ErrorCode, FString(TEXT("NO_RESTORE_POINT")));
    TestTrue(TEXT("begin opens it"), Run(Subsystem, TEXT("begin")).bSuccess);
    TestEqual(TEXT("a second begin is refused"), Run(Subsystem, TEXT("begin")).ErrorCode, FString(TEXT("RESTORE_POINT_OPEN")));

    Step(Probe, TEXT("McpA"), true);
    Step(Probe, TEXT("McpB"), true);
    const FMcpCapturedResponse Status = Run(Subsystem, TEXT("status"));
    const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
    TestTrue(TEXT("status lists the steps"), Status.Result.IsValid() && Status.Result->TryGetArrayField(TEXT("steps"), Steps) && Steps && Steps->Num() == 2);

    // A step the user made since: nothing is undone.
    Step(Probe, TEXT("User"), false);
    TestEqual(TEXT("a user step blocks the rollback"), Run(Subsystem, TEXT("rollback")).ErrorCode, FString(TEXT("USER_STEP_SINCE_RESTORE_POINT")));
    TestEqual(TEXT("the refusal undid nothing"), Probe->ComponentTags.Num(), 3);

    GEditor->UndoTransaction(); // the user takes theirs back
    const FMcpCapturedResponse Rolled = Run(Subsystem, TEXT("rollback"));
    TestTrue(TEXT("the rollback succeeds"), Rolled.bSuccess);
    TestEqual(TEXT("both MCP steps are undone"), Probe->ComponentTags.Num(), 0);

    TestTrue(TEXT("end closes it"), Run(Subsystem, TEXT("end")).bSuccess);
    GEditor->Trans->Reset(FText::FromString(TEXT("Mcp checkpoint test")));
    return true;
}
#endif
