#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Elements/PCGCreatePoints.h"
#include "Data/PCGPointArrayData.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataRevisionGuardTest, "EngineWire.PCG.DataSnapshot.RevisionGuardDiagnostics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataRevisionGuardTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    UPCGSettings* Base = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGCreatePointsSettings::StaticClass(), Base);
    auto* Settings = Cast<UPCGCreatePointsSettings>(Base);
    if (!TestTrue(TEXT("A real native settings fixture exists"), Node && Settings)) return false;
    Component->bRegenerateInEditor = false;
    Component->SetGraphLocal(Graph.Get());
    if (!TestEqual(TEXT("The isolated component has the intended graph"), Component->GetGraph(), Graph.Get())) return false;
    const TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<TSharedPtr<FJsonValue>> Page;
    FString Error;
    auto Snapshot = CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error);
    if (!TestTrue(TEXT("A complete bounded guard can be captured"), Snapshot.IsValid())) return false;
    TestTrue(TEXT("Unchanged complete revisions are readable"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    ++Settings->Seed;
    TestFalse(TEXT("A real settings edit is refused"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    TestEqual(TEXT("A changed complete graph token is diagnosed as an authored change"), Error,
        FString(TEXT("The projection's authored graph revision changed.")));
    --Settings->Seed;
    TestTrue(TEXT("Restored authored state restores the original guard"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    const TArray<FPCGPoint> OriginalPoints = Settings->PointsToCreate;
    Settings->PointsToCreate.SetNum(FPCGReadBudget::MaxValues + 1);
    TestFalse(TEXT("An incomplete bounded fingerprint still fails closed"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    TestEqual(TEXT("An unavailable fingerprint is not asserted to be a changed fingerprint"), Error,
        FString(TEXT("The projection's complete graph revision is unavailable within the existing read bounds.")));
    TestEqual(TEXT("A refused read returns no partial rows"), Page.Num(), 0);
    Settings->PointsToCreate = OriginalPoints;
    TestTrue(TEXT("Narrowing the source restores reads without changing the budget"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    return true;
}
#endif
