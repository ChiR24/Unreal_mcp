#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationTestDelegates.h"
#include "UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/McpOpenCodeAcpClient.h"
#include "Acp/UnrealAgentStudioKit.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientProtocolTest(FAutomationTestBase& Test)
    {
#if !(PLATFORM_LINUX || PLATFORM_MAC)
        Test.AddInfo(TEXT("Skipping fake ACP process harness on this platform."));
        return true;
#else
        if (!FPaths::FileExists(TEXT("/usr/bin/python3")))
        {
            Test.AddInfo(TEXT("Skipping fake ACP process harness because /usr/bin/python3 is unavailable."));
            return true;
        }

        const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpHarness")));
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
        {
            Test.AddError(FString::Printf(TEXT("Failed to create fake ACP harness directory: %s"), *TestDirectory));
            return false;
        }

        const FString ScriptPath = FPaths::Combine(TestDirectory, TEXT("acp"));
        if (!FFileHelper::SaveStringToFile(MakeFakeAcpScript(), *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            Test.AddError(FString::Printf(TEXT("Failed to write fake ACP script: %s"), *ScriptPath));
            return false;
        }

        int32 ChmodReturnCode = 1;
        FString ChmodOutput;
        FPlatformProcess::ExecProcess(TEXT("/bin/chmod"), *FString::Printf(TEXT("+x \"%s\""), *ScriptPath), &ChmodReturnCode, &ChmodOutput, &ChmodOutput);
        if (ChmodReturnCode != 0)
        {
            Test.AddError(FString::Printf(TEXT("Failed to make fake ACP script executable: %s"), *ChmodOutput));
            return false;
        }

        const FString PreviousOpenCodeCommand = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_ACP_COMMAND"));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *ScriptPath);
        FScopedAutomationBridgeSettingsOverride BridgeSettings;

        FOpenCodeAcpClient Client;
        FString LastStatus;
        FString LastPermission;
        TArray<FString> TranscriptEntries;
        int32 ModelChangeCount = 0;
        int32 StopCount = 0;

        Client.OnStatus.BindLambda([&LastStatus](const FString& Status) { LastStatus = Status; });
        Client.OnTranscript.BindLambda([&TranscriptEntries](const FString& Role, const FString& Text) { TranscriptEntries.Add(Role + TEXT(":") + Text); });
        Client.OnPermission.BindLambda([&LastPermission](const FString& Description) { LastPermission = Description; });
        Client.OnModelsChanged.BindLambda([&ModelChangeCount]() { ++ModelChangeCount; });
        Client.OnStopped.BindLambda([&StopCount]() { ++StopCount; });

        bool bPassed = true;
        bool bClientStarted = false;
        {
            FScopedIgnoredSigPwrForTest IgnoredSigPwr;
            bClientStarted = Client.Start(TestDirectory);
        }

        if (Test.TestTrue(TEXT("Fake ACP client starts with ignored parent SIGPWR"), bClientStarted))
        {
            FString GeneratedAgentPrompt;
            const FString GeneratedAgentPromptPath = FPaths::Combine(TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
            bPassed &= Test.TestTrue(TEXT("Generated Unreal Agent prompt is written"), FPaths::FileExists(GeneratedAgentPromptPath) && FFileHelper::LoadFileToString(GeneratedAgentPrompt, *GeneratedAgentPromptPath));
            bPassed &= Test.TestTrue(TEXT("Generated prompt has current version marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_prompt_version: 2")));
            bPassed &= Test.TestTrue(TEXT("Generated prompt has Studio Kit marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_studio_kit_version: 1")));
            bPassed &= Test.TestTrue(TEXT("Generated OpenCode config is written"), FPaths::FileExists(FPaths::Combine(TestDirectory, TEXT(".opencode/opencode.json"))));
            bPassed &= Test.TestTrue(TEXT("Studio Kit summary is exposed"), Client.GetLastStudioKitSummary().Contains(TEXT("Studio Kit:")));

            const FString ContextEnvelope = Client.RefreshEditorContext();
            bPassed &= Test.TestTrue(TEXT("Client builds an editor context envelope"), ContextEnvelope.Contains(TEXT("<unreal_editor_context")));
            bPassed &= Test.TestTrue(TEXT("Client validation can run"), Client.RunProjectValidation());

            Client.SetAttachEditorContext(false);
            bPassed &= Test.TestTrue(TEXT("Fake ACP client becomes ready"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
            bPassed &= Test.TestEqual(TEXT("Fake ACP session id parsed"), Client.GetSessionId(), FString(TEXT("fake-session")));
            bPassed &= Test.TestEqual(TEXT("Initial model parsed"), Client.GetCurrentModel(), FString(TEXT("model-a")));
            bPassed &= Test.TestEqual(TEXT("Initial thinking parsed"), Client.GetCurrentThinking(), FString(TEXT("medium")));
            bPassed &= Test.TestEqual(TEXT("Model options parsed"), Client.GetModelOptions().Num(), 3);
            bPassed &= Test.TestEqual(TEXT("Thinking options parsed"), Client.GetThinkingOptions().Num(), 3);
            bPassed &= Test.TestEqual(TEXT("Agent options parsed"), Client.GetAgentOptions().Num(), 2);
            bPassed &= Test.TestTrue(TEXT("Default Unreal agent selection completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentAgent() == TEXT("unreal-agent"); }));
            bPassed &= Test.TestTrue(TEXT("Model changes delegate fired"), ModelChangeCount > 0);

            Client.SetThinking(TEXT("high"));
            bPassed &= Test.TestTrue(TEXT("Thinking switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentThinking() == TEXT("high") && Client.CanSelectThinking(); }));
            Client.SetModel(TEXT("model-b"));
            bPassed &= Test.TestTrue(TEXT("Model switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentModel() == TEXT("model-b") && Client.CanSelectModel(); }));

            Client.SendPrompt(TEXT("approve once path"));
            bPassed &= Test.TestTrue(TEXT("Permission request arrives for allow once"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
            bPassed &= Test.TestTrue(TEXT("Context usage update is parsed"), Client.HasContextWindowUsage());
            bPassed &= Test.TestEqual(TEXT("Context used tokens parsed"), Client.GetContextWindowUsedTokens(), 32000);
            bPassed &= Test.TestEqual(TEXT("Context size tokens parsed"), Client.GetContextWindowSizeTokens(), 64000);
            bPassed &= Test.TestTrue(TEXT("Permission description includes fake tool title"), LastPermission.Contains(TEXT("fake permission")));
            Client.ApprovePermissionOnce();
            bPassed &= Test.TestTrue(TEXT("Allow once prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestTrue(TEXT("Allow once selected option reached fake ACP"), ContainsTranscript(TranscriptEntries, TEXT("permission option once")));
            bPassed &= Test.TestTrue(TEXT("Structured tool call update reuses saved location"), ContainsTranscript(TranscriptEntries, TEXT("Tool:read: /Game/SourceA.cpp completed")));

            Client.SendPrompt(TEXT("cancel path"));
            bPassed &= Test.TestTrue(TEXT("Permission request arrives before cancel"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
            Client.CancelPrompt();
            bPassed &= Test.TestTrue(TEXT("Cancelled prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestTrue(TEXT("Cancel status reports cancelled stop reason"), LastStatus.Contains(TEXT("cancelled")));

            Client.SendPrompt(TEXT("mismatched always path"));
            bPassed &= Test.TestTrue(TEXT("Permission request arrives for mismatched always option"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
            bPassed &= Test.TestFalse(TEXT("Mismatched always option is not available"), Client.CanApprovePermissionAlways());
            Client.RejectPermission();
            bPassed &= Test.TestTrue(TEXT("Mismatched option prompt completes after reject"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

            Client.SendPrompt(TEXT("error after permission path"));
            bPassed &= Test.TestTrue(TEXT("Prompt error completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestTrue(TEXT("Prompt error is reported"), ContainsTranscript(TranscriptEntries, TEXT("fake prompt error")));

            Client.SendPrompt(TEXT("no options path"));
            bPassed &= Test.TestTrue(TEXT("Malformed permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestTrue(TEXT("Malformed permission is reported"), ContainsTranscript(TranscriptEntries, TEXT("no selectable options")));
        }
        else
        {
            bPassed = false;
        }

        Client.Stop();
        bPassed &= Test.TestEqual(TEXT("Stopped callback fires only when an active client stops"), StopCount, bClientStarted ? 1 : 0);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *PreviousOpenCodeCommand);
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        return bPassed;
#endif
    }
}

#endif
