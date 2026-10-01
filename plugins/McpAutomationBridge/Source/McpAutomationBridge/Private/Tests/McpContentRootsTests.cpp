#include "McpConnectionManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Transport/Connection/McpConnectionManagerPrivate.h"

namespace McpContentRootsTestsLocal
{
	const TCHAR* ProbeRoot = TEXT("/McpContentRootsProbe/");
	const TCHAR* ProbeName = TEXT("/McpContentRootsProbe");
	// Shorter than the four characters a package name needs: a root this short is still a root.
	const TCHAR* ShortRoot = TEXT("/Zq/");

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpContentRootsPathAcceptanceTest,
	"McpAutomationBridge.Transport.ContentRoots.PathAcceptance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpContentRootsPathAcceptanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpContentRootsTestsLocal;
	const FString Dir = ProbeDir();
	FPackageName::RegisterMountPoint(ProbeRoot, Dir);
	FPackageName::RegisterMountPoint(ShortRoot, Dir);

	// A mounted root is accepted wherever a folder is, however it is written: what holds for /Game holds for a plugin.
	const TCHAR* Accepted[] = {
		TEXT("/McpContentRootsProbe"), TEXT("/McpContentRootsProbe/"), TEXT("/McpContentRootsProbe/Props/Rocks"),
		TEXT("/McpContentRootsProbe/Props/Rock.Rock"), TEXT("McpContentRootsProbe/Props"), TEXT("/Game"), TEXT("/Zq")};
	for (const TCHAR* Path : Accepted)
	{
		TestFalse(FString::Printf(TEXT("'%s' is accepted"), Path), SanitizeProjectRelativePath(Path).IsEmpty());
	}

	// Each refusal says what it was: only a '..' segment is traversal.
	struct FRefusal
	{
		const TCHAR* Path;
		EMcpPathRejection Reason;
	};
	const FRefusal Refused[] = {
		{TEXT("/Game/../Engine"), EMcpPathRejection::Traversal},
		{TEXT("C:/Windows/system.ini"), EMcpPathRejection::WindowsAbsolutePath},
		{TEXT(""), EMcpPathRejection::Empty},
		{TEXT("/NotMountedAnywhere/Props"), EMcpPathRejection::NotAMountedRoot},
		{TEXT("/etc/passwd"), EMcpPathRejection::NotAMountedRoot},
		{TEXT("/McpContentRootsProbe/Bad Name"), EMcpPathRejection::InvalidName}};
	for (const FRefusal& Case : Refused)
	{
		EMcpPathRejection Reason = EMcpPathRejection::None;
		FText Detail;
		TestTrue(FString::Printf(TEXT("'%s' is refused"), Case.Path),
			McpClassifyProjectPath(Case.Path, &Reason, &Detail, nullptr, /*bLogRefusal=*/false).IsEmpty());
		TestTrue(FString::Printf(TEXT("'%s' is refused for the right reason"), Case.Path), Reason == Case.Reason);
		const FString Message = McpPathRefusalMessage(TEXT("path"), Case.Path);
		TestTrue(FString::Printf(TEXT("'%s' is blamed on traversal only when it is"), Case.Path),
			Message.Contains(TEXT("traversal")) == (Case.Reason == EMcpPathRejection::Traversal));
		TestTrue(FString::Printf(TEXT("'%s' is called an unmounted root only when it is"), Case.Path),
			Message.Contains(TEXT("not a mounted content root")) == (Case.Reason == EMcpPathRejection::NotAMountedRoot));
	}

	// An unmounted root names itself (without its slash, which a reply redacts) and the roots that are mounted.
	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	FPackageName::UnRegisterMountPoint(ShortRoot, Dir);
	const FString Message = McpPathRefusalMessage(TEXT("packagePaths"), TEXT("/McpContentRootsProbe"));
	TestTrue(TEXT("the unmounted root is named"), Message.Contains(TEXT("'McpContentRootsProbe' is not a mounted content root")));
	TestTrue(TEXT("the mounted roots are listed"), Message.Contains(TEXT("Mounted roots: /Game")));
	TestTrue(TEXT("the field is named"), Message.Contains(TEXT("Invalid packagePaths '/McpContentRootsProbe'")));
	TestFalse(TEXT("the list ends the message, with no full stop for a redactor to read as part of a path"), Message.EndsWith(TEXT(".")));

	// A mounted root that starts like the mistyped one is listed next to /Game, where a caller looks first.
	FPackageName::RegisterMountPoint(ProbeRoot, Dir);
	const FString Near = McpPathRefusalMessage(TEXT("path"), TEXT("/McpContentRootsProbeX/Props"));
	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	TestTrue(TEXT("the closest mounted root follows /Game"), Near.Contains(TEXT("Mounted roots: /Game, /McpContentRootsProbe")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpContentRootsRedactionTest,
	"McpAutomationBridge.Transport.ContentRoots.ReplyRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpContentRootsRedactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpContentRootsTestsLocal;
	using McpAutomationBridgeSubsystemResponse::SanitizeEngineErrorForResponse;
	const FString Dir = ProbeDir();
	FPackageName::RegisterMountPoint(ProbeRoot, Dir);

	// A mount that is registered stays readable in a reply, bare or with a path below it, as /Game does.
	const TCHAR* Kept[] = {
		TEXT("Invalid package path '/McpContentRootsProbe/Props/Rocks': no such folder"),
		TEXT("Invalid package path '/McpContentRootsProbe': nothing there"),
		TEXT("roots: /McpContentRootsProbe, /Game")};
	for (const TCHAR* Text : Kept)
	{
		TestEqual(FString::Printf(TEXT("'%s' is kept"), Text), SanitizeEngineErrorForResponse(Text), FString(Text));
	}

	// A host path is hidden, and so is a root nothing has mounted. A root that is not mounted any more is hidden too.
	const TCHAR* Hidden[] = {
		TEXT("failed at /etc/passwd now"),		   TEXT("failed at /home/alice/project now"),
		TEXT("failed at /Users/alice/project now"), TEXT("failed at C:/Users/alice/project now"),
		TEXT("failed at \\\\server\\share\\alice now"), TEXT("failed at /NotMountedAnywhere/alice now")};
	for (const TCHAR* Text : Hidden)
	{
		const FString Reply = SanitizeEngineErrorForResponse(Text);
		TestTrue(FString::Printf(TEXT("'%s' is hidden"), Text), Reply.Contains(TEXT("[path redacted]")) && !Reply.Contains(TEXT("alice")) && !Reply.Contains(TEXT("passwd")) && !Reply.Contains(TEXT("NotMounted")));
	}

	// A host root never counts, whatever is mounted under that name.
	const FString UsersRoot = TEXT("/Users/");
	FPackageName::RegisterMountPoint(UsersRoot, Dir);
	TestTrue(TEXT("a mount named like a host root is still hidden"),
		SanitizeEngineErrorForResponse(TEXT("failed at /Users/alice/project now")).Contains(TEXT("[path redacted]")));
	FPackageName::UnRegisterMountPoint(UsersRoot, Dir);

	// The log device does not ask the engine which mounts exist, so there a mount reads as it always did.
	TestTrue(TEXT("without mounts the probe is hidden"),
		SanitizeEngineErrorForResponse(TEXT("failed at /McpContentRootsProbe/Props now"), /*bNameMounts=*/false).Contains(TEXT("[path redacted]")));

	// The refusal for a root that is not mounted reaches the caller whole: the root named, the mounted roots readable.
	FString Reply = SanitizeEngineErrorForResponse(McpPathRefusalMessage(TEXT("packagePaths"), TEXT("/McpContentRootsProbeX/Props")));
	TestTrue(TEXT("the unmounted root is still named"), Reply.Contains(TEXT("'McpContentRootsProbeX' is not a mounted content root")));
	TestTrue(TEXT("the mounted roots are readable"), Reply.Contains(TEXT("Mounted roots: /Game, /McpContentRootsProbe")));
	TestEqual(TEXT("only what the caller sent is hidden"), Reply.ReplaceInline(TEXT("[path redacted]"), TEXT("")), 1);

	FPackageName::UnRegisterMountPoint(ProbeRoot, Dir);
	return true;
}

#endif
