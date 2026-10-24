#include "Domains/ControlEditor/Checkpoint/McpEditorCheckpoint.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Core/Journal/McpChangeJournal.h"
#include "Core/Journal/McpChangeJournalState.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Requests/McpUndoLedger.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "EngineWireSubsystem.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpEditorCheckpointCleanupTest, "EngineWire.ControlEditor.RestorePoint.CleansUpOnlyItsOwnAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// An asset made while a request through this server runs is offered for deletion; one made outside any request (by the
// user, or found on disk) is listed but never offered. Both are made in memory, never saved, and taken out of the
// registry and the journal again.
bool FMcpEditorCheckpointCleanupTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpChangeJournal;
    UEngineWireSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!Subsystem)
    {
        AddError(TEXT("No EngineWire subsystem."));
        return false;
    }
    // The run_build request that runs this test is running: set it aside so the first asset is made outside any.
    FJournalState& State = JournalState();
    const int32 Depth = State.RequestDepth;
    const FString RequestId = State.RequestId;
    const FString Action = State.Action;
    State.RequestDepth = 0;
    State.RequestId.Reset();
    State.Action.Reset();
    Run(Subsystem, TEXT("end"));
    Run(Subsystem, TEXT("begin"));
    const auto Make = [](const TCHAR* Name)
    {
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/_McpScratch/%s"), Name));
        UPhysicalMaterial* Asset = NewObject<UPhysicalMaterial>(Package, Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
        return Asset;
    };
    UPhysicalMaterial* UserAsset = Make(TEXT("McpCheckpointUserAsset"));
    BeginRequest(TEXT("checkpoint-cleanup-test"), TEXT("control_editor restore_point_test"));
    UPhysicalMaterial* OwnAsset = Make(TEXT("McpCheckpointOwnAsset"));
    EndRequest();

    const FMcpCapturedResponse Status = Run(Subsystem, TEXT("status"));
    const TArray<TSharedPtr<FJsonValue>>* Created = nullptr;
    const TSharedPtr<FJsonObject>* Cleanup = nullptr;
    const TSharedPtr<FJsonObject>* Params = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
    if (Status.Result.IsValid())
    {
        Status.Result->TryGetArrayField(TEXT("assetsCreated"), Created);
        if (Status.Result->TryGetObjectField(TEXT("cleanupCall"), Cleanup) && Cleanup && (*Cleanup)->TryGetObjectField(TEXT("params"), Params) && Params)
        {
            (*Params)->TryGetArrayField(TEXT("assetPaths"), Paths);
        }
    }
    const auto Holds = [](const TArray<TSharedPtr<FJsonValue>>* List, const TCHAR* Path)
    {
        return List && List->ContainsByPredicate([Path](const TSharedPtr<FJsonValue>& Value) { return Value.IsValid() && Value->AsString() == Path; });
    };
    TestTrue(TEXT("both are listed as created"), Holds(Created, TEXT("/Game/_McpScratch/McpCheckpointUserAsset")) &&
                                                 Holds(Created, TEXT("/Game/_McpScratch/McpCheckpointOwnAsset")));
    TestTrue(TEXT("the one made in a request is offered for deletion"), Holds(Paths, TEXT("/Game/_McpScratch/McpCheckpointOwnAsset")));
    TestFalse(TEXT("the one made outside any request is not"), Holds(Paths, TEXT("/Game/_McpScratch/McpCheckpointUserAsset")));
    Run(Subsystem, TEXT("end"));

    for (UPhysicalMaterial* Asset : {UserAsset, OwnAsset})
    {
        FAssetRegistryModule::AssetDeleted(Asset);
        Asset->GetOutermost()->SetDirtyFlag(false);
        Asset->ClearFlags(RF_Public | RF_Standalone);
        Asset->MarkAsGarbage();
        Asset->GetOutermost()->MarkAsGarbage();
    }
    State.Entries.RemoveAll([](const FJournalEntry& Kept) { return Kept.Path.StartsWith(TEXT("/Game/_McpScratch/McpCheckpoint")); });
    Reindex(State);
    State.RequestDepth = Depth;
    State.RequestId = RequestId;
    State.Action = Action;
    return true;
}
#endif
