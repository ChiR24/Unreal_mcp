#if WITH_DEV_AUTOMATION_TESTS

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentPermissionSyntaxTest,
    "UnrealAgent.Acp.PermissionSyntax",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool RejectsAgentText(
    const FString& RootDirectory,
    const FString& Name,
    const FString& AgentText)
{
    const FString ProjectDirectory = FPaths::Combine(RootDirectory, Name);
    FUnrealAgentStudioKit::EnsureForProject(ProjectDirectory);
    const FString AgentPath = FPaths::Combine(
        ProjectDirectory,
        TEXT(".opencode/agents/unreal-agent.md"));
    if (!FFileHelper::SaveStringToFile(
            AgentText,
            *AgentPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return false;
    }
    TArray<FString> Errors;
    return !UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
        ProjectDirectory,
        Errors);
}
}

bool FUnrealAgentPermissionSyntaxTest::RunTest(const FString& Parameters)
{
    const FString RootDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("UnrealAgentPermissionSyntaxHarness")));
    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RootDirectory, true);

    bool bPassed = TestTrue(
        TEXT("Tagged permission key is rejected"),
        RejectsAgentText(
            RootDirectory,
            TEXT("TaggedKey"),
            TEXT("---\npermission:\n  !!str unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestTrue(
        TEXT("Anchored permission key is rejected"),
        RejectsAgentText(
            RootDirectory,
            TEXT("AnchoredKey"),
            TEXT("---\npermission:\n  &k unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestTrue(
        TEXT("Tagged top-level permission key is rejected"),
        RejectsAgentText(
            RootDirectory,
            TEXT("TaggedRootKey"),
            TEXT("---\n!!str permission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestTrue(
        TEXT("Anchored top-level permission key is rejected"),
        RejectsAgentText(
            RootDirectory,
            TEXT("AnchoredRootKey"),
            TEXT("---\n&policy permission:\n  unreal-engine_manage_asset: allow\n---\n")));
    bPassed &= TestTrue(
        TEXT("YAML merge-key permission bypass is rejected"),
        RejectsAgentText(
            RootDirectory,
            TEXT("MergeAlias"),
            TEXT("---\nunsafe: &unsafe\n  permission:\n    unreal-engine_manage_asset: allow\n<<: *unsafe\n---\n")));
    bPassed &= TestTrue(
        TEXT("Uniformly indented root permission is still validated"),
        RejectsAgentText(
            RootDirectory,
            TEXT("IndentedRoot"),
            TEXT("---\n  description: unsafe\n  permission:\n    unreal-engine_manage_asset: allow\n---\n")));

    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    return bPassed;
}

#endif
