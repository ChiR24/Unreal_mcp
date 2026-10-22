#include "Tests/PCG/McpPCGRootFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGAddTag.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "PCGPin.h"
#include "Misc/App.h"

namespace McpPCGRootTests
{
FFixture::~FFixture() { DestroyOwned(); }

bool FFixture::Start(int32 Count, const FString& Tag)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World || World->WorldType != EWorldType::Editor || FString(FApp::GetProjectName()) != TEXT("PCGValidation") || Count < 1 || Count > 257)
    {
        Test->AddError(TEXT("Real root-data fixtures require the owned PCGValidation editor world and a bounded dataset count."));
        return false;
    }
    UClass* CreateClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGCreatePointsSettings"));
    UClass* TagClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGAddTagSettings"));
    if (!CreateClass || !TagClass)
    {
        Test->AddError(TEXT("Native CreatePoints and AddTag settings must already be loaded."));
        return false;
    }
    FActorSpawnParameters Spawn;
    Spawn.Name = FName(*(TEXT("McpRootFixture_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Spawn.ObjectFlags = RF_Transient;
    AActor* OwnedActor = World->SpawnActor<AActor>(Spawn);
    if (!OwnedActor) return false;
    Actor = OwnedActor;
    UBoxComponent* Bounds = NewObject<UBoxComponent>(OwnedActor, NAME_None, RF_Transient);
    Bounds->SetBoxExtent(FVector(100));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OwnedActor->AddInstanceComponent(Bounds);
    OwnedActor->SetRootComponent(Bounds);
    Bounds->RegisterComponent();
    Component.Reset(NewObject<UPCGComponent>(OwnedActor, NAME_None, RF_Transient));
    Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    Component->bIsComponentPartitioned = false;
    OwnedActor->AddInstanceComponent(Component.Get());
    Component->RegisterComponent();
    Graph.Reset(NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient));
    const auto& OutputPins = Graph->GetOutputNode()->GetInputPins();
    if (OutputPins.Num() != 1 || !OutputPins[0]) return false;
    const FName BoundaryLabel = OutputPins[0]->Properties.Label;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UPCGSettings* CreateSettings = nullptr;
        UPCGSettings* TagSettings = nullptr;
        UPCGNode* CreateNode = Graph->AddNodeOfType(CreateClass, CreateSettings);
        UPCGNode* TagNode = Graph->AddNodeOfType(TagClass, TagSettings);
        if (!CreateNode || !TagNode || !CreateSettings || !TagSettings) return false;
        auto* Points = static_cast<UPCGCreatePointsSettings*>(CreateSettings);
        Points->PointsToCreate.SetNum(1);
        Points->PointsToCreate[0].Seed = Index + 41;
        Points->bCullPointsOutsideVolume = false;
        static_cast<UPCGAddTagSettings*>(TagSettings)->TagsToAdd = Tag;
        if (!Graph->AddEdge(CreateNode, TEXT("Out"), TagNode, TEXT("In")) ||
            !Graph->AddEdge(TagNode, TEXT("Out"), Graph->GetOutputNode(), BoundaryLabel)) return false;
    }
    Component->SetGraph(Graph.Get());
    const auto Flag = Generated;
    GeneratedHandle = Component->OnPCGGraphGeneratedDelegate.AddLambda([Flag](UPCGComponent*) { Flag->store(true); });
    return Regenerate();
}

bool FFixture::Regenerate()
{
    Generated->store(false);
    const FPCGTaskId Task = Component->GenerateLocalGetTaskId(true);
    bPending = Task != InvalidPCGTaskId;
    if (!bPending) Test->AddError(TEXT("Native fixture generation did not schedule; no completion is inferred."));
    return bPending;
}

bool FFixture::Ready() const
{
    return Component.IsValid() && Generated->load() && !Component->IsGenerating() && !Component->IsCleaningUp();
}

void FFixture::DestroyOwned()
{
    if (Component.IsValid())
    {
        if (Component->IsGenerating() || Component->IsCleaningUp())
        {
            Test->AddError(FString::Printf(TEXT("Native work remains active; preserve owned fixture for idle inspection: %s"), *Component->GetPathName()));
            return;
        }
        Component->OnPCGGraphGeneratedDelegate.Remove(GeneratedHandle);
        Component->CleanupLocalImmediate(true);
        Component->DestroyComponent();
        Component.Reset();
    }
    if (AActor* OwnedActor = Actor.Get()) OwnedActor->Destroy();
    Actor.Reset();
    Graph.Reset();
}

namespace
{
class FRootFixtureCommand final : public IAutomationLatentCommand
{
public:
    FRootFixtureCommand(FAutomationTestBase* InTest, TFunction<bool(FFixture&)> InBody, int32 InCount, const FString& InTag)
        : Test(InTest), Fixture(InTest), Body(MoveTemp(InBody)), Count(InCount), Tag(InTag) {}
    bool Update() override
    {
        if (!bStarted)
        {
            bStarted = true;
            Deadline = FPlatformTime::Seconds() + 150.0;
            if (!Fixture.Start(Count, Tag))
            {
                Test->AddError(TEXT("Owned native root fixture setup failed."));
                return true;
            }
        }
        if (FPlatformTime::Seconds() >= Deadline)
        {
            Test->AddError(TEXT("Owned native fixture observation timed out; active work is not assumed stopped."));
            return true;
        }
        if (Fixture.IsPending())
        {
            if (!Fixture.Ready()) return false;
            Fixture.FinishWait();
            if (!Test->TestEqual(TEXT("Native fixture produced the actual expected root dataset count"), Fixture.GetComponent()->GetGeneratedGraphOutput().TaggedData.Num(), Count)) return true;
        }
        return Body(Fixture);
    }
private:
    FAutomationTestBase* Test;
    FFixture Fixture;
    TFunction<bool(FFixture&)> Body;
    int32 Count;
    FString Tag;
    double Deadline = 0;
    bool bStarted = false;
};
}
void Run(FAutomationTestBase* Test, TFunction<bool(FFixture&)> Body, int32 Count, const FString& Tag)
{
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRootFixtureCommand>(Test, MoveTemp(Body), Count, Tag));
}
}
#endif
