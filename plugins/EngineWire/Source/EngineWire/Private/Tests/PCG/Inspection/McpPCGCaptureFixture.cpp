#include "Tests/PCG/Inspection/McpPCGCaptureFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
#include "Components/BoxComponent.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGTransformPoints.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/App.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"

namespace McpPCGCaptureTests
{
namespace
{
UPCGNode* AddLoadedNode(UPCGGraph* Graph, const TCHAR* ClassPath, UPCGSettings*& Settings)
{
    UClass* Class = FindObject<UClass>(nullptr, ClassPath);
    return Class ? Graph->AddNodeOfType(Class, Settings) : nullptr;
}

bool Connect(UPCGGraph* Graph, UPCGNode* From, const TCHAR* FromLabel, UPCGNode* To, const TCHAR* ToLabel)
{
    return From && To && Graph->AddEdge(From, FName(FromLabel), To, FName(ToLabel));
}
}

bool FFixture::Start(bool bNested)
{
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World || World->WorldType != EWorldType::Editor || FString(FApp::GetProjectName()) != TEXT("PCGValidation"))
    {
        Test->AddError(TEXT("Real capture fixtures require the owned PCGValidation editor world."));
        return false;
    }
    FActorSpawnParameters Spawn;
    Spawn.Name = FName(*(TEXT("McpCaptureFixture_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    Spawn.ObjectFlags = RF_Transient;
    AActor* OwnedActor = World->SpawnActor<AActor>(Spawn);
    if (!OwnedActor) return false;
    Actor = OwnedActor;
    UBoxComponent* Bounds = NewObject<UBoxComponent>(OwnedActor, NAME_None, RF_Transient);
    Bounds->SetBoxExtent(FVector(1000));
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
    UPCGSettings* Settings = nullptr;
    UPCGNode* Create = AddLoadedNode(Graph.Get(), TEXT("/Script/PCG.PCGCreatePointsSettings"), Settings);
    if (!Create || !Settings) return false;
    auto* Points = static_cast<UPCGCreatePointsSettings*>(Settings);
    Points->PointsToCreate.SetNum(2);
    Points->PointsToCreate[0].Transform.SetLocation(FVector(12.5, 3, 4));
    Points->PointsToCreate[0].Seed = 293;
    Points->PointsToCreate[1].Transform.SetLocation(FVector(-7, 8, 9));
    Points->PointsToCreate[1].Seed = -17;
    Points->bCullPointsOutsideVolume = false;
    UPCGGraph* TransformGraph = Graph.Get();
    if (bNested)
    {
        Child.Reset(NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient));
        TransformGraph = Child.Get();
    }
    Settings = nullptr;
    Transform = AddLoadedNode(TransformGraph, TEXT("/Script/PCG.PCGTransformPointsSettings"), Settings);
    if (!Transform.IsValid() || !Settings) return false;
    auto* TransformSettings = static_cast<UPCGTransformPointsSettings*>(Settings);
    TransformSettings->OffsetMin = TransformSettings->OffsetMax = FVector(11, 0, 0);
    TransformSettings->bRecomputeSeed = false;
    const auto& ChildOutputs = TransformGraph->GetOutputNode()->GetInputPins();
    if (ChildOutputs.Num() != 1 || !ChildOutputs[0]) return false;
    if (!TransformGraph->AddEdge(Transform.Get(), TEXT("Out"), TransformGraph->GetOutputNode(), ChildOutputs[0]->Properties.Label)) return false;
    if (bNested)
    {
        const UPCGPin* Input = Child->GetInputNode()->GetOutputPin(TEXT("In"));
        if (!Input || !Child->AddEdge(Child->GetInputNode(), Input->Properties.Label, Transform.Get(), TEXT("In"))) return false;
        Settings = nullptr;
        Subgraph = AddLoadedNode(Graph.Get(), TEXT("/Script/PCG.PCGSubgraphSettings"), Settings);
        if (!Subgraph.IsValid() || !Settings) return false;
        static_cast<UPCGSubgraphSettings*>(Settings)->SetSubgraph(Child.Get());
        Subgraph->UpdateAfterSettingsChangeDuringCreation();
        const auto& Outputs = Graph->GetOutputNode()->GetInputPins();
        if (Outputs.Num() != 1 || !Outputs[0] || !Connect(Graph.Get(), Create, TEXT("Out"), Subgraph.Get(), TEXT("In")) ||
            !Graph->AddEdge(Subgraph.Get(), ChildOutputs[0]->Properties.Label, Graph->GetOutputNode(), Outputs[0]->Properties.Label)) return false;
    }
    else if (!Connect(Graph.Get(), Create, TEXT("Out"), Transform.Get(), TEXT("In"))) return false;
    Component->SetGraph(Graph.Get());
    auto& Inspection = Component->GetExecutionState().GetInspection();
    if (!Test->TestFalse(TEXT("A newly owned component starts with no inspection observer"), Inspection.IsInspecting())) return false;
    // Two explicit acquisitions simulate a pre-existing observer and the coordinator's independent ownership.
    Inspection.EnableInspection();
    ++Acquisitions;
    Inspection.EnableInspection();
    ++Acquisitions;
    const auto Flag = Generated;
    GeneratedHandle = Component->OnPCGGraphGeneratedDelegate.AddLambda([Flag](UPCGComponent*) { Flag->store(true); });
    return Schedule();
}

bool FFixture::Schedule()
{
    if (!Component.IsValid() || Component->IsGenerating() || Component->IsCleaningUp()) return false;
    Generated->store(false);
    return Component->GenerateLocalGetTaskId(true) != InvalidPCGTaskId;
}

bool FFixture::Ready() const
{
    return Component.IsValid() && Generated->load() && !Component->IsGenerating() && !Component->IsCleaningUp();
}

void FFixture::ReleaseOne()
{
    if (Acquisitions > 0)
    {
        Component->GetExecutionState().GetInspection().DisableInspection();
        --Acquisitions;
    }
}

FFixture::~FFixture()
{
    if (Component.IsValid())
    {
        if (Component->IsGenerating() || Component->IsCleaningUp())
        {
            Test->AddError(TEXT("Capture fixture still has native work; preserve the owned actor for idle inspection, never force-stop."));
            return;
        }
        while (Acquisitions > 0) ReleaseOne();
        Component->OnPCGGraphGeneratedDelegate.Remove(GeneratedHandle);
        Component->CleanupLocalImmediate(true);
        Component->DestroyComponent();
        Component.Reset();
    }
    if (AActor* OwnedActor = Actor.Get()) OwnedActor->Destroy();
}

bool Visit(const FPCGGraphExecutionInspection& Inspection, const FPCGStack& Stack,
    TFunctionRef<void(const FPCGDataCollection&)> Visitor)
{
    check(IsInGameThread());
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    return Inspection.InspectData(Stack, Visitor);
#else
    if (const FPCGDataCollection* Data = Inspection.GetInspectionData(Stack))
    {
        Visitor(*Data);
        return true;
    }
    return false;
#endif
}
}
#endif
#endif
