#include "Foundation/McpLiveStateRevisionTracker.h"
#include "Foundation/McpLiveStateRevisions.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLiveStateRevisionTrackerTest,
	"McpAutomationBridge.Foundation.LiveStateRevisions.EditorDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLiveStateRevisionTrackerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpLiveStateRevisions& Revisions = FMcpLiveStateRevisions::Get();
	McpStopLiveStateTracking();
	Revisions.Reset();
	McpStartLiveStateTracking();

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		AddError(TEXT("Editor world is unavailable"));
		McpStopLiveStateTracking();
		McpStartLiveStateTracking();
		return false;
	}

	const int64 SelectionBefore = Revisions.Current(EMcpStateKind::Selection);
	AActor* Actor = World->SpawnActor<AActor>();
	if (!Actor)
	{
		AddError(TEXT("Unable to create selection fixture actor"));
		McpStopLiveStateTracking();
		McpStartLiveStateTracking();
		return false;
	}
	GEditor->SelectActor(Actor, true, true, true, true);
	TestTrue(TEXT("selection delegate advances"),
		Revisions.Current(EMcpStateKind::Selection) > SelectionBefore);

	const int64 LevelBefore = Revisions.Current(EMcpStateKind::Level);
	FEditorDelegates::MapChange.Broadcast(0);
	TestTrue(TEXT("map delegate advances"),
		Revisions.Current(EMcpStateKind::Level) > LevelBefore);

	UPackage* Package = CreatePackage(TEXT("/Game/MCP_Task42_RevisionTracker"));
	UCurveFloat* Asset = NewObject<UCurveFloat>(
		Package, TEXT("MCP_Task42_TrackedAsset"), RF_Public | RF_Standalone);
	Package->SetDirtyFlag(false);
	const int64 PackageBefore = Revisions.Current(EMcpStateKind::Package);
	TestTrue(TEXT("package fixture marks dirty"), Asset->MarkPackageDirty());
	TestTrue(TEXT("package delegate advances"),
		Revisions.Current(EMcpStateKind::Package) > PackageBefore);

	const int64 RegistryBefore = Revisions.Current(EMcpStateKind::AssetRegistry);
	FAssetRegistryModule::AssetCreated(Asset);
	TestTrue(TEXT("asset registry delegate advances"),
		Revisions.Current(EMcpStateKind::AssetRegistry) > RegistryBefore);

	McpStopLiveStateTracking();
	const int64 LevelAfterStop = Revisions.Current(EMcpStateKind::Level);
	FEditorDelegates::MapChange.Broadcast(0);
	TestEqual(TEXT("stopped tracker ignores events"),
		Revisions.Current(EMcpStateKind::Level), LevelAfterStop);

	GEditor->SelectActor(Actor, false, true, true, true);
	Actor->Destroy();
	FAssetRegistryModule::AssetDeleted(Asset);
	Package->SetDirtyFlag(false);
	McpStartLiveStateTracking();
	return true;
}

#endif
