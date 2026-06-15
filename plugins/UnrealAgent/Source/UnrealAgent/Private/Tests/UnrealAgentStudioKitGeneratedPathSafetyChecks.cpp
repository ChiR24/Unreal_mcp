#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitGeneratedPathSafetyChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
#if PLATFORM_UNIX || PLATFORM_MAC
        const FString SwapProjectDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitSwapTest")));
        const FString SwapOutsideDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitSwapOutside")));
        IFileManager::Get().DeleteDirectory(*SwapProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SwapOutsideDirectory, false, true);
        IFileManager::Get().MakeDirectory(*SwapProjectDirectory, true);
        IFileManager::Get().MakeDirectory(*SwapOutsideDirectory, true);
        bool bSwappedOpenCodeDirectory = false;
        UnrealAgentStudioKit::GBeforeStudioKitTemplateWriteForTest =
            [&bSwappedOpenCodeDirectory, SwapProjectDirectory, SwapOutsideDirectory](const FString&)
            {
                if (bSwappedOpenCodeDirectory)
                {
                    return;
                }
                bSwappedOpenCodeDirectory = true;
                const FString OpenCodePath = FPaths::Combine(SwapProjectDirectory, TEXT(".opencode"));
                IFileManager::Get().DeleteDirectory(*OpenCodePath, false, true);
                int32 LinkReturnCode = INDEX_NONE;
                FString LinkOutput;
                FPlatformProcess::ExecProcess(
                    TEXT("/bin/ln"),
                    *FString::Printf(TEXT("-s \"%s\" \"%s\""), *SwapOutsideDirectory, *OpenCodePath),
                    &LinkReturnCode,
                    &LinkOutput,
                    &LinkOutput);
            };
        const FUnrealAgentStudioKitResult SwapResult =
            FUnrealAgentStudioKit::EnsureForProject(SwapProjectDirectory);
        UnrealAgentStudioKit::GBeforeStudioKitTemplateWriteForTest = nullptr;
        bPassed &= Test.TestTrue(TEXT("Studio Kit symlink-swap fixture is triggered"), bSwappedOpenCodeDirectory);
        bPassed &= Test.TestFalse(TEXT("Studio Kit rejects a concurrent OpenCode symlink swap"), SwapResult.WasSuccessful());
        bPassed &= Test.TestFalse(
            TEXT("Studio Kit concurrent symlink swap cannot redirect generated files"),
            FPaths::FileExists(FPaths::Combine(SwapOutsideDirectory, TEXT("agents/unreal-agent.md"))));
        const FString SwappedOpenCodePath = FPaths::Combine(SwapProjectDirectory, TEXT(".opencode"));
        int32 SwapUnlinkReturnCode = INDEX_NONE;
        FString SwapUnlinkOutput;
        FPlatformProcess::ExecProcess(
            TEXT("/usr/bin/unlink"),
            *FString::Printf(TEXT("\"%s\""), *SwappedOpenCodePath),
            &SwapUnlinkReturnCode,
            &SwapUnlinkOutput,
            &SwapUnlinkOutput);
        IFileManager::Get().DeleteDirectory(*SwapProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SwapOutsideDirectory, false, true);

        const FString SymlinkProjectDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitSymlinkTest")));
        const FString SymlinkOutsideDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitSymlinkOutside")));
        IFileManager::Get().DeleteDirectory(*SymlinkProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SymlinkOutsideDirectory, false, true);
        IFileManager::Get().MakeDirectory(*SymlinkProjectDirectory, true);
        IFileManager::Get().MakeDirectory(*SymlinkOutsideDirectory, true);
        const FString OpenCodeSymlinkPath = FPaths::Combine(SymlinkProjectDirectory, TEXT(".opencode"));
        int32 LinkReturnCode = INDEX_NONE;
        FString LinkOutput;
        FPlatformProcess::ExecProcess(
            TEXT("/bin/ln"),
            *FString::Printf(TEXT("-s \"%s\" \"%s\""), *SymlinkOutsideDirectory, *OpenCodeSymlinkPath),
            &LinkReturnCode,
            &LinkOutput,
            &LinkOutput);
        bPassed &= Test.TestEqual(TEXT("OpenCode symlink test fixture is created"), LinkReturnCode, 0);
        const FUnrealAgentStudioKitResult SymlinkResult =
            FUnrealAgentStudioKit::EnsureForProject(SymlinkProjectDirectory);
        bPassed &= Test.TestFalse(TEXT("Studio Kit rejects symlinked generated-file ancestry"), SymlinkResult.WasSuccessful());
        bPassed &= Test.TestFalse(TEXT("Studio Kit does not write guardrails through OpenCode symlink"), FPaths::FileExists(FPaths::Combine(SymlinkOutsideDirectory, TEXT("plugins/unreal-agent-guardrails.ts"))));
        int32 UnlinkReturnCode = INDEX_NONE;
        FString UnlinkOutput;
        FPlatformProcess::ExecProcess(
            TEXT("/usr/bin/unlink"),
            *FString::Printf(TEXT("\"%s\""), *OpenCodeSymlinkPath),
            &UnlinkReturnCode,
            &UnlinkOutput,
            &UnlinkOutput);
        bPassed &= Test.TestEqual(TEXT("OpenCode symlink test fixture is removed"), UnlinkReturnCode, 0);
        IFileManager::Get().DeleteDirectory(*SymlinkProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SymlinkOutsideDirectory, false, true);
#elif PLATFORM_WINDOWS
        const FString JunctionProjectDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitJunctionTest")));
        const FString JunctionOutsideDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitJunctionOutside")));
        IFileManager::Get().DeleteDirectory(*JunctionProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*JunctionOutsideDirectory, false, true);
        IFileManager::Get().MakeDirectory(*JunctionProjectDirectory, true);
        IFileManager::Get().MakeDirectory(*JunctionOutsideDirectory, true);
        const FString OpenCodeJunctionPath = FPaths::Combine(JunctionProjectDirectory, TEXT(".opencode"));
        int32 JunctionReturnCode = INDEX_NONE;
        FString JunctionOutput;
        FPlatformProcess::ExecProcess(
            TEXT("cmd.exe"),
            *FString::Printf(TEXT("/c mklink /J \"%s\" \"%s\""), *OpenCodeJunctionPath, *JunctionOutsideDirectory),
            &JunctionReturnCode,
            &JunctionOutput,
            &JunctionOutput);
        bPassed &= Test.TestEqual(TEXT("OpenCode junction test fixture is created"), JunctionReturnCode, 0);
        const FUnrealAgentStudioKitResult JunctionResult =
            FUnrealAgentStudioKit::EnsureForProject(JunctionProjectDirectory);
        bPassed &= Test.TestFalse(TEXT("Studio Kit rejects junction-backed generated-file ancestry"), JunctionResult.WasSuccessful());
        bPassed &= Test.TestFalse(
            TEXT("Studio Kit does not write guardrails through an OpenCode junction"),
            FPaths::FileExists(FPaths::Combine(JunctionOutsideDirectory, TEXT("plugins/unreal-agent-guardrails.ts"))));
        int32 RemoveJunctionReturnCode = INDEX_NONE;
        FString RemoveJunctionOutput;
        FPlatformProcess::ExecProcess(
            TEXT("cmd.exe"),
            *FString::Printf(TEXT("/c rmdir \"%s\""), *OpenCodeJunctionPath),
            &RemoveJunctionReturnCode,
            &RemoveJunctionOutput,
            &RemoveJunctionOutput);
        bPassed &= Test.TestEqual(TEXT("OpenCode junction test fixture is removed"), RemoveJunctionReturnCode, 0);
        IFileManager::Get().DeleteDirectory(*JunctionProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*JunctionOutsideDirectory, false, true);

        const FString SwapProjectDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitJunctionSwapTest")));
        const FString SwapOutsideDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitJunctionSwapOutside")));
        IFileManager::Get().DeleteDirectory(*SwapProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SwapOutsideDirectory, false, true);
        IFileManager::Get().MakeDirectory(*SwapProjectDirectory, true);
        IFileManager::Get().MakeDirectory(*SwapOutsideDirectory, true);
        bool bSwappedOpenCodeDirectory = false;
        UnrealAgentStudioKit::GBeforeStudioKitTemplateWriteForTest =
            [&bSwappedOpenCodeDirectory, SwapProjectDirectory, SwapOutsideDirectory](const FString&)
            {
                if (bSwappedOpenCodeDirectory)
                {
                    return;
                }
                bSwappedOpenCodeDirectory = true;
                const FString OpenCodePath = FPaths::Combine(SwapProjectDirectory, TEXT(".opencode"));
                IFileManager::Get().DeleteDirectory(*OpenCodePath, false, true);
                int32 LinkReturnCode = INDEX_NONE;
                FString LinkOutput;
                FPlatformProcess::ExecProcess(
                    TEXT("cmd.exe"),
                    *FString::Printf(TEXT("/c mklink /J \"%s\" \"%s\""), *OpenCodePath, *SwapOutsideDirectory),
                    &LinkReturnCode,
                    &LinkOutput,
                    &LinkOutput);
            };
        const FUnrealAgentStudioKitResult SwapResult =
            FUnrealAgentStudioKit::EnsureForProject(SwapProjectDirectory);
        UnrealAgentStudioKit::GBeforeStudioKitTemplateWriteForTest = nullptr;
        bPassed &= Test.TestTrue(TEXT("Studio Kit Windows junction-swap fixture is triggered"), bSwappedOpenCodeDirectory);
        bPassed &= Test.TestFalse(TEXT("Studio Kit rejects a concurrent OpenCode junction swap"), SwapResult.WasSuccessful());
        bPassed &= Test.TestFalse(
            TEXT("Studio Kit concurrent junction swap cannot redirect generated files"),
            FPaths::FileExists(FPaths::Combine(SwapOutsideDirectory, TEXT("agents/unreal-agent.md"))));
        const FString SwappedOpenCodePath = FPaths::Combine(SwapProjectDirectory, TEXT(".opencode"));
        int32 RemoveSwapJunctionReturnCode = INDEX_NONE;
        FString RemoveSwapJunctionOutput;
        FPlatformProcess::ExecProcess(
            TEXT("cmd.exe"),
            *FString::Printf(TEXT("/c rmdir \"%s\""), *SwappedOpenCodePath),
            &RemoveSwapJunctionReturnCode,
            &RemoveSwapJunctionOutput,
            &RemoveSwapJunctionOutput);
        bPassed &= Test.TestEqual(TEXT("OpenCode swap junction fixture is removed"), RemoveSwapJunctionReturnCode, 0);
        IFileManager::Get().DeleteDirectory(*SwapProjectDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*SwapOutsideDirectory, false, true);
#endif
        return bPassed;
    }
}

#endif
