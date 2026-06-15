#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpClientProtocolChecks.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientSessionChecks(FAcpClientProtocolTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        bool bPassed = true;

        FString GeneratedAgentPrompt;
        const FString GeneratedAgentPromptPath =
            FPaths::Combine(Context.TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(TEXT("Generated Unreal Agent prompt is written"), FPaths::FileExists(GeneratedAgentPromptPath) && FFileHelper::LoadFileToString(GeneratedAgentPrompt, *GeneratedAgentPromptPath));
        bPassed &= Test.TestTrue(TEXT("Generated prompt has current version marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_prompt_version: 2")));
        bPassed &= Test.TestTrue(TEXT("Generated prompt has Studio Kit marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_studio_kit_version: 1")));
        bPassed &= Test.TestTrue(TEXT("Generated OpenCode config is written"), FPaths::FileExists(FPaths::Combine(Context.TestDirectory, TEXT(".opencode/opencode.json"))));
        bPassed &= Test.TestTrue(TEXT("Studio Kit summary is exposed"), Client.GetLastStudioKitSummary().Contains(TEXT("Studio Kit:")));
        bPassed &= Test.TestTrue(TEXT("Client validation can run"), Client.RunProjectValidation());

        Client.SetAttachEditorContext(false);
        bPassed &= Test.TestTrue(TEXT("ACP client becomes ready against test server"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
        const FString ContextEnvelope = Client.RefreshEditorContext();
        bPassed &= Test.TestTrue(TEXT("Client builds an editor context envelope"), ContextEnvelope.Contains(TEXT("<unreal_editor_context")));
        bPassed &= Test.TestTrue(TEXT("Client context reports configured Unreal MCP session"), ContextEnvelope.Contains(TEXT("unrealMcpConfiguredForSession: true")));
        bPassed &= Test.TestTrue(TEXT("Client context requires MCP preflight before broad work"), ContextEnvelope.Contains(TEXT("/unreal-tool-inventory")) && ContextEnvelope.Contains(TEXT("get_content_browser_state")));
        bPassed &= Test.TestEqual(TEXT("ACP test session id parsed"), Client.GetSessionId(), FString(TEXT("acp-test-session")));
        bPassed &= Test.TestEqual(TEXT("Initial model parsed"), Client.GetCurrentModel(), FString(TEXT("model-a")));
        bPassed &= Test.TestEqual(TEXT("Initial thinking parsed"), Client.GetCurrentThinking(), FString(TEXT("medium")));
        bPassed &= Test.TestEqual(TEXT("Model options parsed"), Client.GetModelOptions().Num(), 3);
        bPassed &= Test.TestEqual(TEXT("Thinking options parsed"), Client.GetThinkingOptions().Num(), 3);
        bPassed &= Test.TestEqual(TEXT("Agent options parsed"), Client.GetAgentOptions().Num(), 2);
        bPassed &= Test.TestTrue(TEXT("Default Unreal agent selection completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentAgent() == TEXT("unreal-agent"); }));
        bPassed &= Test.TestTrue(TEXT("Model changes delegate fired"), Context.ModelChangeCount > 0);

        Client.SetThinking(TEXT("high"));
        bPassed &= Test.TestTrue(TEXT("Thinking switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentThinking() == TEXT("high") && Client.CanSelectThinking(); }));
        Client.SetModel(TEXT("model-b"));
        bPassed &= Test.TestTrue(TEXT("Model switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentModel() == TEXT("model-b") && Client.CanSelectModel(); }));

        Client.SendPrompt(TEXT("approve once path"));
        bPassed &= Test.TestTrue(TEXT("Permission request arrives for allow once"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestTrue(TEXT("Context usage update is parsed"), Client.HasContextWindowUsage());
        bPassed &= Test.TestEqual(TEXT("Context used tokens parsed"), Client.GetContextWindowUsedTokens(), 32000);
        bPassed &= Test.TestEqual(TEXT("Context size tokens parsed"), Client.GetContextWindowSizeTokens(), 64000);
        bPassed &= Test.TestTrue(TEXT("Permission description includes test tool title"), Context.LastPermission.Contains(TEXT("test permission")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Allow once prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(TEXT("Allow once selected option reached ACP test server"), ContainsTranscript(Context.TranscriptEntries, TEXT("permission option once")));
        bPassed &= Test.TestTrue(TEXT("Structured tool call update reuses saved location"), ContainsTranscript(Context.TranscriptEntries, TEXT("Tool:read: /Game/SourceA.cpp completed")));

        Client.SendPrompt(TEXT("large message burst path"));
        bPassed &= Test.TestTrue(
            TEXT("Complete ACP frames survive a burst larger than the partial-frame limit"),
            PumpClientUntil(
                Client,
                [&Client]() { return !Client.IsPromptInFlight(); },
                10.0));

        Client.SendPrompt(TEXT("oversized complete frame path"));
        bPassed &= Test.TestTrue(
            TEXT("A valid frame after an oversized complete frame still completes"),
            PumpClientUntil(
                Client,
                [&Client]() { return !Client.IsPromptInFlight(); },
                10.0));
        bPassed &= Test.TestFalse(
            TEXT("Oversized complete ACP frames are discarded before parsing"),
            ContainsTranscript(
                Context.TranscriptEntries,
                TEXT("oversized-frame-marker")));
        return bPassed;
    }
}

#endif
