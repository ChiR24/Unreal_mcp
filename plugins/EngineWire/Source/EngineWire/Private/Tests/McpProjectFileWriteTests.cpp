#include "Foundation/BridgeHelpers/Security/McpProjectFileWrite.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if !PLATFORM_WINDOWS
#include <unistd.h>
#endif

// The checked writer refuses a file whose folder is a link out of the project, which is what a link made after the
// resolver's check looks like by the time the write runs (the resolver itself would refuse a link already there).

namespace
{
// A folder link at Link to Target: a junction on Windows (no privilege needed), a symbolic link elsewhere.
bool MakeFolderLink(const FString& Link, const FString& Target)
{
#if PLATFORM_WINDOWS
	int32 Code = -1;
	const FString Args = FString::Printf(TEXT("/c mklink /J \"%s\" \"%s\""),
		*Link.Replace(TEXT("/"), TEXT("\\")), *Target.Replace(TEXT("/"), TEXT("\\")));
	return FPlatformProcess::ExecProcess(TEXT("cmd.exe"), *Args, &Code, nullptr, nullptr) && Code == 0;
#else
	return symlink(TCHAR_TO_UTF8(*Target), TCHAR_TO_UTF8(*Link)) == 0;
#endif
}

// Removes the link itself, never what it points at.
void RemoveFolderLink(const FString& Link)
{
#if PLATFORM_WINDOWS
	IFileManager::Get().DeleteDirectory(*Link, false, false);
#else
	unlink(TCHAR_TO_UTF8(*Link));
#endif
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpProjectFileWriteTest,
	"EngineWire.Foundation.ProjectFileWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpProjectFileWriteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	IFileManager& Files = IFileManager::Get();
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("McpFileWriteTest"));
	const FString Outside = FPaths::ConvertRelativePathToFull(FPlatformProcess::UserTempDir()) / TEXT("McpFileWriteOutside");
	const FString Link = Folder / TEXT("Link");
	RemoveFolderLink(Link); // what an earlier run left
	Files.DeleteDirectory(*Outside, false, true);
	Files.MakeDirectory(*Outside, true);

	FString Error;
	FString Read;
	TestTrue(TEXT("control: a file in a new folder inside the project is written"),
		McpWriteProjectText(Folder / TEXT("New/plain.txt"), TEXT("plain"), Error));
	TestTrue(TEXT("control: it holds the text"),
		FFileHelper::LoadFileToString(Read, *(Folder / TEXT("New/plain.txt"))) && Read == TEXT("plain"));
	TestTrue(TEXT("control: it is overwritten, not appended to"),
		McpWriteProjectText(Folder / TEXT("New/plain.txt"), TEXT("p"), Error)
			&& FFileHelper::LoadFileToString(Read, *(Folder / TEXT("New/plain.txt"))) && Read == TEXT("p"));
	TestFalse(TEXT("a file outside the project is refused"),
		McpWriteProjectText(Outside / TEXT("direct.txt"), TEXT("x"), Error));
	TestFalse(TEXT("nothing was written there"), FPaths::FileExists(Outside / TEXT("direct.txt")));

	if (TestTrue(TEXT("a folder link out of the project is made"), MakeFolderLink(Link, Outside)))
	{
		TestFalse(TEXT("a file under the link is refused"), McpWriteProjectText(Link / TEXT("escaped.txt"), TEXT("x"), Error));
		TestTrue(TEXT("the refusal names the link"), Error.Contains(TEXT("link")));
		TestFalse(TEXT("nothing reached the folder the link points at"), FPaths::FileExists(Outside / TEXT("escaped.txt")));
		RemoveFolderLink(Link);
	}
	if (!Files.DirectoryExists(*Link))
	{
		Files.DeleteDirectory(*Folder, false, true);
	}
	Files.DeleteDirectory(*Outside, false, true);
	return true;
}
#endif
