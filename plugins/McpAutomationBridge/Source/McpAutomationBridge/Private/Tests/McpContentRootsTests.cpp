#include "McpConnectionManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Transport/Connection/McpConnectionManagerPrivate.h"

namespace McpContentRootsTestsLocal
{
	const TCHAR* ProbeRoot = TEXT("/McpContentRootsProbe/");
	const TCHAR* ProbeName = TEXT("/McpContentRootsProbe");

	bool Reports(const FString& Name)
	{
		for (const TSharedPtr<FJsonValue>& Value : McpBuildContentRootValues())
		{
			if (Value.IsValid() && Value->AsString() == Name)
			{
				return true;
			}
		}
		return false;
	}

	FString ProbeDir()
	{
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("McpContentRootsProbe/");
		IFileManager::Get().MakeDirectory(*Dir, true);
		return Dir;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpContentRootsSnapshotTest,
	"McpAutomationBridge.Transport.ContentRoots.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpContentRootsSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpContentRootsTestsLocal;
	TestTrue(TEXT("/Game is reported"), Reports(TEXT("/Game")));
	TestTrue(TEXT("/Engine is reported"), Reports(TEXT("/Engine")));
	for (const TSharedPtr<FJsonValue>& Value : McpBuildContentRootValues())
	{
		const FString Root = Value->AsString();
		TestTrue(FString::Printf(TEXT("'%s' has no trailing slash"), *Root), Root.Len() <= 1 || !Root.EndsWith(TEXT("/")));
	}

	TestFalse(TEXT("probe absent before mount"), Reports(ProbeName));
	const FString Dir = ProbeDir();
	FPackageName::RegisterMountPoint(ProbeRoot, Dir);
	TestTrue(TEXT("probe reported after mount"), Reports(ProbeName));
	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	TestFalse(TEXT("probe gone after dismount"), Reports(ProbeName));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpContentRootsSubscriptionTest,
	"McpAutomationBridge.Transport.ContentRoots.Subscription",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpContentRootsSubscriptionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpContentRootsTestsLocal;
	const FString Dir = ProbeDir();
	const TSharedRef<FMcpConnectionManager> Manager = MakeShared<FMcpConnectionManager>();

	Manager->SubscribeContentRootChanges();
	TestFalse(TEXT("nothing pending after subscribing"), Manager->HasPendingContentRootsUpdate());

	FPackageName::RegisterMountPoint(ProbeRoot, Dir);
	TestTrue(TEXT("a mount marks the snapshot stale"), Manager->HasPendingContentRootsUpdate());
	Manager->FlushContentRootsUpdate();
	TestFalse(TEXT("a flush clears it, with no socket to send to"), Manager->HasPendingContentRootsUpdate());

	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	TestTrue(TEXT("a dismount marks the snapshot stale"), Manager->HasPendingContentRootsUpdate());
	Manager->FlushContentRootsUpdate();

	Manager->UnsubscribeContentRootChanges();
	FPackageName::RegisterMountPoint(ProbeRoot, Dir);
	TestFalse(TEXT("no mark after unsubscribing"), Manager->HasPendingContentRootsUpdate());
	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	return true;
}

#endif
