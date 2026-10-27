#pragma once

#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

// The diagnostics store pointed at a fresh temp root with a fake clock at Now; the destructor resets the store
// and deletes the root. The root is a GUID, not a timestamp: the suite shares one singleton and each test
// deletes its root, so a per-second name handed every test the same directory and they deleted each other's.
struct FMcpDiagnosticsTestStore
{
	FMcpDiagnosticsSnapshot& Store = FMcpDiagnosticsSnapshot::Get();
	FString Root;
	double Now;

	explicit FMcpDiagnosticsTestStore(double InNow)
		: Root(FPaths::Combine(FPlatformProcess::UserTempDir(),
			  FString::Printf(TEXT("McpDiagnosticsTests_%d_%s"), FPlatformProcess::GetCurrentProcessId(),
				  *FGuid::NewGuid().ToString(EGuidFormats::Digits))))
		, Now(InNow)
	{
		IFileManager::Get().MakeDirectory(*Root, true);
		Store.Reset();
		Store.SetRootOverride(Root);
		Store.SetClock([this]() { return Now; });
	}

	~FMcpDiagnosticsTestStore()
	{
		Store.Reset();
		IFileManager::Get().DeleteDirectory(*Root, false, true);
	}
};

inline FString McpTestReadFile(const FString& Path)
{
	FString Content;
	FFileHelper::LoadFileToString(Content, *Path);
	return Content;
}

inline void McpTestWriteFile(const FString& Path, const FString& Content)
{
	FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
#endif
