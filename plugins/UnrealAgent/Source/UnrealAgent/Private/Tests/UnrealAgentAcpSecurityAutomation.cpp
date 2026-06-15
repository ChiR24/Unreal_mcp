#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"
#include "Tests/UnrealAgentAcpSecurityChecks.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryPatterns.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionGitCommands.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionReadOnlyCommands.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentAcpSecurityTest,
    "UnrealAgent.Acp.Security",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentAcpSecurityTest::RunTest(const FString& Parameters)
{
#if !(PLATFORM_LINUX || PLATFORM_MAC)
    AddInfo(TEXT("Skipping ACP security process harness on this platform."));
    return true;
#else
    using namespace UnrealAgent::AutomationTests;
    using UnrealAgent::OpenCodeAcp::PermissionGitCommands::ContainsDestructiveGitCommand;
    if (!FPaths::FileExists(TEXT("/usr/bin/python3")))
    {
        AddInfo(TEXT("Skipping ACP security process harness because /usr/bin/python3 is unavailable."));
        return true;
    }

    const FString TestDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpSecurityHarness")));
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    IFileManager::Get().MakeDirectory(*TestDirectory, true);
    FScopedOpenCodeConfigEnvironment ConfigEnvironment(TestDirectory);
    IFileManager::Get().MakeDirectory(*FPaths::Combine(TestDirectory, TEXT("Content")), true);
    IFileManager::Get().MakeDirectory(*FPaths::Combine(TestDirectory, TEXT("Docs")), true);
    if (!FFileHelper::SaveStringToFile(TEXT("asset"), *FPaths::Combine(TestDirectory, TEXT("Content/Danger.uasset")))
        || !FFileHelper::SaveStringToFile(TEXT("documentation"), *FPaths::Combine(TestDirectory, TEXT("Docs/Notes.md"))))
    {
        AddError(TEXT("Failed to seed ACP security path fixtures."));
        return false;
    }

    int32 LinkReturnCode = 1;
    FString LinkOutput;
    const FString BinaryTargetPath = FPaths::Combine(TestDirectory, TEXT("Content/Danger.uasset"));
    const FString BinaryAliasPath = FPaths::Combine(TestDirectory, TEXT("Docs/cache.bin"));
    const FString SafeAliasPath = FPaths::Combine(TestDirectory, TEXT("Docs/safe-link.md"));
    const FString SafeTargetPath = FPaths::Combine(TestDirectory, TEXT("Docs/Notes.md"));
    FPlatformProcess::ExecProcess(
        TEXT("/bin/ln"),
        *FString::Printf(TEXT("-s \"%s\" \"%s\""), *BinaryTargetPath, *BinaryAliasPath),
        &LinkReturnCode,
        &LinkOutput,
        &LinkOutput);
    if (LinkReturnCode != 0)
    {
        AddError(FString::Printf(TEXT("Failed to seed ACP security symlink fixture: %s"), *LinkOutput));
        return false;
    }
    FPlatformProcess::ExecProcess(
        TEXT("/bin/ln"),
        *FString::Printf(TEXT("-s \"%s\" \"%s\""), *SafeTargetPath, *SafeAliasPath),
        &LinkReturnCode,
        &LinkOutput,
        &LinkOutput);
    if (LinkReturnCode != 0)
    {
        AddError(FString::Printf(TEXT("Failed to seed safe ACP symlink fixture: %s"), *LinkOutput));
        return false;
    }

    const FString ScriptPath = FPaths::Combine(TestDirectory, TEXT("acptest"));
    bool bPassed = TestEqual(
        TEXT("ACP security harness executable uses the acptest filename"),
        FPaths::GetCleanFilename(ScriptPath),
        FString(TEXT("acptest")));
    bPassed &= TestTrue(
        TEXT("Native classifier treats git apply as destructive"),
        ContainsDestructiveGitCommand(TEXT("git apply /tmp/policy.patch")));
    bPassed &= TestTrue(
        TEXT("Native classifier treats ANSI-C quoted shell-backed Git aliases as destructive"),
        ContainsDestructiveGitCommand(
            TEXT("git -c $'alias.wipe=!rm -rf .' wipe")));
    bPassed &= TestTrue(
        TEXT("Native classifier decodes ANSI-C escaped Git executables"),
        ContainsDestructiveGitCommand(
            TEXT("g$'\\x69t' -C . reset --hard")));
    bPassed &= TestTrue(
        TEXT("Native classifier decodes ANSI-C escaped Git alias payloads"),
        ContainsDestructiveGitCommand(
            TEXT("git -c $'alias.wipe=\\x21rm -rf .' wipe")));
    bPassed &= TestTrue(
        TEXT("Native classifier rejects environment-backed Git aliases"),
        ContainsDestructiveGitCommand(
            TEXT("UA_ALIAS='!rm -rf .' git --config-env=alias.wipe=UA_ALIAS wipe")));
    bPassed &= TestTrue(
        TEXT("Native classifier rejects Git restore"),
        ContainsDestructiveGitCommand(TEXT("git restore .")));
    bPassed &= TestTrue(
        TEXT("Native classifier rejects Git checkout"),
        ContainsDestructiveGitCommand(TEXT("git checkout .")));
    bPassed &= TestFalse(
        TEXT("ANSI-C quoted Git aliases are not Unreal binary asset paths"),
        UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns::
            HasUnrealBinaryAssetExtensionOrGlob(
                TEXT("git -c $'alias.wipe=!rm -rf .' wipe")));
    bPassed &= TestTrue(
        TEXT("ANSI-C escaped Unreal binary asset extensions remain blocked"),
        UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns::
            HasUnrealBinaryAssetExtensionOrGlob(
                TEXT("cat Docs/Danger.uasse$'\\x74'")));
    bPassed &= TestFalse(
        TEXT("Search patterns do not exempt explicit Unreal binary asset operands"),
        UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns::
            IsHarmlessBinaryExtensionMention(
                TEXT("rg -n \"\\.uasset\" Content/Danger.uasset")));
    bPassed &= TestTrue(
        TEXT("Locale-quoted documentation reads remain read-only"),
        UnrealAgent::OpenCodeAcp::PermissionReadOnlyCommands::
            IsExplicitReadOnlyCommandText(
                TEXT("cat Docs/Notes.$\"md\"")));
    bPassed &= TestFalse(
        TEXT("Locale-quoted documentation paths are not Unreal binary assets"),
        UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns::
            HasUnrealBinaryAssetExtensionOrGlob(
                TEXT("cat Docs/Notes.$\"md\"")));
    if (!FFileHelper::SaveStringToFile(MakeAcpTestServerScript(), *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        AddError(TEXT("Failed to write ACP security test server."));
        return false;
    }
    int32 ChmodReturnCode = 1;
    FString ChmodOutput;
    FPlatformProcess::ExecProcess(
        TEXT("/bin/chmod"),
        *FString::Printf(TEXT("+x \"%s\""), *ScriptPath),
        &ChmodReturnCode,
        &ChmodOutput,
        &ChmodOutput);
    if (ChmodReturnCode != 0)
    {
        AddError(FString::Printf(TEXT("Failed to make ACP security test server executable: %s"), *ChmodOutput));
        return false;
    }

    const FString PreviousPath = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
    const FString SymlinkBinDirectory = FPaths::Combine(FPaths::GetPath(TestDirectory), TEXT("UnrealAgentExternalBin"));
    const FString SymlinkExecutable = FPaths::Combine(SymlinkBinDirectory, TEXT("opencode"));
    IFileManager::Get().DeleteDirectory(*SymlinkBinDirectory, false, true);
    IFileManager::Get().MakeDirectory(*SymlinkBinDirectory, true);
    FPlatformProcess::ExecProcess(
        TEXT("/bin/ln"),
        *FString::Printf(TEXT("-s \"%s\" \"%s\""), *ScriptPath, *SymlinkExecutable),
        &LinkReturnCode,
        &LinkOutput,
        &LinkOutput);
    if (LinkReturnCode != 0)
    {
        AddError(FString::Printf(TEXT("Failed to seed executable symlink fixture: %s"), *LinkOutput));
        return false;
    }
    FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *SymlinkBinDirectory);
    FOpenCodeAcpClient SymlinkExecutableClient;
    bPassed &= TestFalse(TEXT("PATH symlink resolving inside the project is rejected"), SymlinkExecutableClient.Start(TestDirectory));
    SymlinkExecutableClient.Stop();
    FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *PreviousPath);

    const FString PreviousCommand = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_ACP_COMMAND"));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *ScriptPath);
    FScopedAutomationBridgeSettingsOverride BridgeSettings;
    {
        FOpenCodeAcpClient StubbornClient;
        bPassed &= TestTrue(
            TEXT("Stubborn ACP client starts"),
            StubbornClient.Start(TestDirectory));
        bPassed &= TestTrue(
            TEXT("Stubborn ACP client becomes ready"),
            PumpClientUntil(
                StubbornClient,
                [&StubbornClient]() { return StubbornClient.IsReady(); }));
        StubbornClient.SetAttachEditorContext(false);
        StubbornClient.SendPrompt(TEXT("ignore termination path"));
        bPassed &= TestTrue(
            TEXT("Stubborn ACP prompt completes"),
            PumpClientUntil(
                StubbornClient,
                [&StubbornClient]() {
                    return !StubbornClient.IsPromptInFlight();
                }));
        const FString StubbornPidPath = ScriptPath + TEXT(".pid");
        FString StubbornPidText;
        bPassed &= TestTrue(
            TEXT("Stubborn ACP server records its process id"),
            FFileHelper::LoadFileToString(
                StubbornPidText,
                *StubbornPidPath));
        StubbornClient.Stop();
        int32 ProbeReturnCode = 0;
        FString ProbeOutput;
        FPlatformProcess::ExecProcess(
            TEXT("/bin/kill"),
            *FString::Printf(
                TEXT("-0 %s"),
                *StubbornPidText.TrimStartAndEnd()),
            &ProbeReturnCode,
            &ProbeOutput,
            &ProbeOutput);
        bPassed &= TestTrue(
            TEXT("Stop force-terminates a SIGTERM-ignoring ACP process"),
            ProbeReturnCode != 0);
        IFileManager::Get().Delete(*StubbornPidPath);
    }
    FOpenCodeAcpClient Client;
    FString LastStatus;
    TArray<FString> TranscriptEntries;
    Client.OnStatus.BindLambda([&LastStatus](const FString& Status) { LastStatus = Status; });
    Client.OnTranscript.BindLambda([&TranscriptEntries](const FString& Role, const FString& Text)
    {
        TranscriptEntries.Add(Role + TEXT(":") + Text);
    });

    bPassed &= TestTrue(TEXT("ACP security client starts"), Client.Start(TestDirectory));
    bPassed &= TestTrue(TEXT("ACP security client becomes ready"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
    Client.SetAttachEditorContext(false);
    FAcpSecurityTestContext Context{*this, Client, TestDirectory, LastStatus, TranscriptEntries};
    bPassed &= RunAcpSecurityPathPolicyChecks(Context);
    bPassed &= RunAcpSecurityRedactionChecks(Context);

    Client.Stop();
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *PreviousCommand);
    IFileManager::Get().DeleteDirectory(*SymlinkBinDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    return bPassed;
#endif
}

#endif
