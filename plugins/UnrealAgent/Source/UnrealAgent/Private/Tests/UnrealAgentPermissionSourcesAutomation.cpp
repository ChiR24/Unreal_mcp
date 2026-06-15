#if WITH_DEV_AUTOMATION_TESTS

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentPermissionSourcesTest,
    "UnrealAgent.Acp.PermissionSources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool SaveText(const FString& Path, const FString& Text)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveStringToFile(
        Text,
        *Path,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool IsPermissionSafe(const FString& ProjectDirectory)
{
    TArray<FString> Errors;
    return UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
        ProjectDirectory,
        Errors);
}
}

bool FUnrealAgentPermissionSourcesTest::RunTest(const FString& Parameters)
{
    const TCHAR* EnvironmentNames[] = {
        TEXT("OPENCODE_CONFIG_DIR"),
        TEXT("OPENCODE_CONFIG"),
        TEXT("OPENCODE_CONFIG_CONTENT"),
        TEXT("OPENCODE_PERMISSION"),
        TEXT("OPENCODE_PURE"),
        TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"),
        TEXT("XDG_CONFIG_HOME"),
        TEXT("HOME"),
        TEXT("APPDATA"),
        TEXT("LOCALAPPDATA")
    };
    TMap<FString, FString> PreviousEnvironment;
    for (const TCHAR* Name : EnvironmentNames)
    {
        PreviousEnvironment.Add(
            Name,
            FPlatformMisc::GetEnvironmentVariable(Name));
        FPlatformMisc::SetEnvironmentVar(Name, TEXT(""));
    }

    const FString RootDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("UnrealAgentPermissionSourcesHarness")));
    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RootDirectory, true);
    const FString GlobalDirectory =
        FPaths::Combine(RootDirectory, TEXT("Global"));
    const FString HomeDirectory =
        FPaths::Combine(RootDirectory, TEXT("Home"));
    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_CONFIG_DIR"),
        *GlobalDirectory);
    FPlatformMisc::SetEnvironmentVar(TEXT("HOME"), *HomeDirectory);

    const FString ProjectDirectory =
        FPaths::Combine(RootDirectory, TEXT("Project"));
    FUnrealAgentStudioKit::EnsureForProject(ProjectDirectory);
    IFileManager::Get().MakeDirectory(
        *FPaths::Combine(ProjectDirectory, TEXT(".git")),
        true);

    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_PERMISSION"),
        TEXT("{\"*\":\"allow\"}"));
    bool bPassed = TestFalse(
        TEXT("Unsafe OPENCODE_PERMISSION is rejected"),
        IsPermissionSafe(ProjectDirectory));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PERMISSION"), TEXT(""));

    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PURE"), TEXT("1"));
    bPassed &= TestFalse(
        TEXT("OPENCODE_PURE is rejected"),
        IsPermissionSafe(ProjectDirectory));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PURE"), TEXT(""));

    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"),
        TEXT("1"));
    bPassed &= TestFalse(
        TEXT("Disabling project config is rejected"),
        IsPermissionSafe(ProjectDirectory));
    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"),
        TEXT(""));

    const FString AncestorDirectory =
        FPaths::Combine(RootDirectory, TEXT("Ancestor"));
    const FString NestedProject =
        FPaths::Combine(AncestorDirectory, TEXT("Nested/Project"));
    FUnrealAgentStudioKit::EnsureForProject(NestedProject);
    IFileManager::Get().MakeDirectory(
        *FPaths::Combine(AncestorDirectory, TEXT(".git")),
        true);
    bPassed &= TestTrue(
        TEXT("Unsafe ancestor config is seeded"),
        SaveText(
            FPaths::Combine(AncestorDirectory, TEXT("opencode.jsonc")),
            TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
    bPassed &= TestFalse(
        TEXT("Unsafe ancestor project config is rejected"),
        IsPermissionSafe(NestedProject));
    IFileManager::Get().Delete(
        *FPaths::Combine(AncestorDirectory, TEXT("opencode.jsonc")),
        false,
        true);
    bPassed &= TestTrue(
        TEXT("Unsafe config above Git root is seeded"),
        SaveText(
            FPaths::Combine(RootDirectory, TEXT("opencode.jsonc")),
            TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
    bPassed &= TestTrue(
        TEXT("Config traversal stops at nearest Git root"),
        IsPermissionSafe(NestedProject));

    const FString UnsafeProjectSingularAgent =
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/agent/unsafe.md"));
    bPassed &= TestTrue(
        TEXT("Unsafe singular project agent is seeded"),
        SaveText(
            UnsafeProjectSingularAgent,
            TEXT("---\npermission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestFalse(
        TEXT("Unsafe singular project agent is rejected"),
        IsPermissionSafe(ProjectDirectory));
    IFileManager::Get().Delete(*UnsafeProjectSingularAgent, false, true);

    const FString UnsafeAncestorSingularAgent =
        FPaths::Combine(AncestorDirectory, TEXT(".opencode/agent/unsafe.md"));
    bPassed &= TestTrue(
        TEXT("Unsafe singular ancestor agent is seeded"),
        SaveText(
            UnsafeAncestorSingularAgent,
            TEXT("---\npermission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestFalse(
        TEXT("Unsafe singular ancestor agent is rejected"),
        IsPermissionSafe(NestedProject));
    IFileManager::Get().Delete(*UnsafeAncestorSingularAgent, false, true);

    IFileManager::Get().MakeDirectory(
        *FPaths::Combine(GlobalDirectory, TEXT("agents")),
        true);
    bPassed &= TestTrue(
        TEXT("Empty optional global agent directory is allowed"),
        IsPermissionSafe(ProjectDirectory));
    bPassed &= TestTrue(
        TEXT("Global agent without override is seeded"),
        SaveText(
            FPaths::Combine(GlobalDirectory, TEXT("agents/inherited.md")),
            TEXT("---\ndescription: inherits global permissions\n---\n")));
    bPassed &= TestTrue(
        TEXT("Global agent without permission override inherits safe policy"),
        IsPermissionSafe(ProjectDirectory));
    bPassed &= TestTrue(
        TEXT("Tagged unsafe global agent is seeded"),
        SaveText(
            FPaths::Combine(GlobalDirectory, TEXT("agents/inherited.md")),
            TEXT("---\n!!str permission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestFalse(
        TEXT("Tagged top-level permission in optional global agent is rejected"),
        IsPermissionSafe(ProjectDirectory));
    bPassed &= TestTrue(
        TEXT("Anchored unsafe global agent is seeded"),
        SaveText(
            FPaths::Combine(GlobalDirectory, TEXT("agents/inherited.md")),
            TEXT("---\n&policy permission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestFalse(
        TEXT("Anchored top-level permission in optional global agent is rejected"),
        IsPermissionSafe(ProjectDirectory));

    IFileManager::Get().DeleteDirectory(
        *FPaths::Combine(GlobalDirectory, TEXT("agents")),
        false,
        true);
    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_CONFIG_DIR"),
        TEXT(""));
    const FString AppDataDirectory =
        FPaths::Combine(RootDirectory, TEXT("AppData"));
    FPlatformMisc::SetEnvironmentVar(TEXT("APPDATA"), *AppDataDirectory);
    bPassed &= TestTrue(
        TEXT("Unsafe APPDATA config is seeded"),
        SaveText(
            FPaths::Combine(AppDataDirectory, TEXT("opencode/opencode.json")),
            TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
    bPassed &= TestFalse(
        TEXT("Unsafe APPDATA OpenCode config is rejected"),
        IsPermissionSafe(ProjectDirectory));
    IFileManager::Get().DeleteDirectory(*AppDataDirectory, false, true);
    FPlatformMisc::SetEnvironmentVar(TEXT("APPDATA"), TEXT(""));

    const FString RelativeConfigDirectory =
        FPaths::Combine(ProjectDirectory, TEXT("RelativeConfig"));
    bPassed &= TestTrue(
        TEXT("Unsafe relative config directory is seeded"),
        SaveText(
            FPaths::Combine(RelativeConfigDirectory, TEXT("opencode.json")),
            TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_CONFIG_DIR"),
        TEXT("RelativeConfig"));
    bPassed &= TestFalse(
        TEXT("Relative OPENCODE_CONFIG_DIR resolves from the Unreal project"),
        IsPermissionSafe(ProjectDirectory));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_DIR"), TEXT(""));

    const FString RelativeExplicitPath =
        FPaths::Combine(ProjectDirectory, TEXT("relative-explicit.json"));
    bPassed &= TestTrue(
        TEXT("Unsafe relative explicit config is seeded"),
        SaveText(
            RelativeExplicitPath,
            TEXT("{\"permission\":{\"execute_command\":\"allow\"}}\n")));
    FPlatformMisc::SetEnvironmentVar(
        TEXT("OPENCODE_CONFIG"),
        TEXT("relative-explicit.json"));
    bPassed &= TestFalse(
        TEXT("Relative OPENCODE_CONFIG resolves from the Unreal project"),
        IsPermissionSafe(ProjectDirectory));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG"), TEXT(""));

    for (const TPair<FString, FString>& Entry : PreviousEnvironment)
    {
        FPlatformMisc::SetEnvironmentVar(*Entry.Key, *Entry.Value);
    }
    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    return bPassed;
}

#endif
