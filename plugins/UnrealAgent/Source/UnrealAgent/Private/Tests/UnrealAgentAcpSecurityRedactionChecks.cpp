#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"
#include "Tests/UnrealAgentAcpSecurityChecks.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpSecurityRedactionChecks(FAcpSecurityTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        TArray<FString>& TranscriptEntries = Context.TranscriptEntries;
        bool bPassed = true;

        Client.SendPrompt(TEXT("capability_token: prompt-transmission-secret"));
        bPassed &= Test.TestTrue(
            TEXT("Capability-token prompt completes"),
            PumpClientUntil(
                Client,
                [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(
            TEXT("Capability-token prompt is redacted before ACP transmission"),
            ContainsTranscript(
                TranscriptEntries,
                TEXT("prompt-redacted-before-acp")));
        bPassed &= Test.TestFalse(
            TEXT("ACP never observes the raw capability-token prompt"),
            ContainsTranscript(TranscriptEntries, TEXT("prompt-leaked-to-acp")));
        bPassed &= Test.TestFalse(
            TEXT("Prompt transcript removes the capability-token value"),
            ContainsTranscript(
                TranscriptEntries,
                TEXT("prompt-transmission-secret")));

        Client.SendPrompt(TEXT("assistant secret path"));
        bPassed &= Test.TestTrue(TEXT("Assistant secret prompt requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Assistant message transcript redacts bearer token"), ContainsTranscript(TranscriptEntries, TEXT("assistant-secret")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Assistant secret prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        TranscriptEntries.Reset();
        Client.SendPrompt(TEXT("assistant split secret path"));
        bPassed &= Test.TestTrue(TEXT("Split assistant secret prompt requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Split assistant transcript redacts continuation token"), ContainsTranscript(TranscriptEntries, TEXT("opaque-assistant-token")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Split assistant secret prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(
            TEXT("Split assistant secret continuation clears after the completed line"),
            ContainsTranscript(TranscriptEntries, TEXT("ACP test response: ordinary assistant output")));

        TranscriptEntries.Reset();
        Client.SendPrompt(TEXT("assistant split key secret path"));
        bPassed &= Test.TestTrue(TEXT("Split-key assistant secret prompt requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Split-key assistant transcript redacts the token"), ContainsTranscript(TranscriptEntries, TEXT("split-key-secret")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Split-key assistant secret prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        TranscriptEntries.Reset();
        Client.SendPrompt(TEXT("assistant camel token path"));
        bPassed &= Test.TestTrue(TEXT("Camel-token assistant prompt requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Camel-token assistant transcript redacts the token"), ContainsTranscript(TranscriptEntries, TEXT("camel-assistant-secret")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Camel-token assistant prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        TranscriptEntries.Reset();
        Client.SendPrompt(TEXT("assistant split camel token path"));
        bPassed &= Test.TestTrue(TEXT("Split camel-token assistant prompt requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Split camel-token assistant transcript redacts the token"), ContainsTranscript(TranscriptEntries, TEXT("split-camel-secret")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Split camel-token assistant prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("foreign session path"));
        bPassed &= Test.TestTrue(TEXT("Foreign-session frames do not stall the active prompt"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Foreign permission never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestFalse(TEXT("Foreign session update is ignored"), ContainsTranscript(TranscriptEntries, TEXT("foreign-session-marker")));

        Client.SendPrompt(TEXT("stderr camel token exit path"));
        bPassed &= Test.TestTrue(TEXT("Camel-token stderr ACP test server exit is observed"), PumpClientUntil(Client, [&Client]() { return !Client.IsRunning(); }));
        bPassed &= Test.TestFalse(TEXT("Exit status redacts camel-case access token"), Context.LastStatus.Contains(TEXT("stderr-camel-secret")));
        bPassed &= Test.TestFalse(TEXT("Exit status redacts split camel-case refresh token"), Context.LastStatus.Contains(TEXT("stderr-split-camel-secret")));
        bPassed &= Test.TestFalse(TEXT("Exit transcript redacts camel-case access token"), ContainsTranscript(TranscriptEntries, TEXT("stderr-camel-secret")));
        bPassed &= Test.TestFalse(TEXT("Exit transcript redacts split camel-case refresh token"), ContainsTranscript(TranscriptEntries, TEXT("stderr-split-camel-secret")));

        bPassed &= Test.TestTrue(TEXT("ACP security client restarts after camel-token stderr exit"), Client.Start(Context.TestDirectory));
        bPassed &= Test.TestTrue(TEXT("Restarted ACP security client becomes ready after camel-token stderr exit"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
        Client.SetAttachEditorContext(false);

        Client.SendPrompt(TEXT("stderr secret exit path"));
        bPassed &= Test.TestTrue(TEXT("ACP test server exit is observed"), PumpClientUntil(Client, [&Client]() { return !Client.IsRunning(); }));
        bPassed &= Test.TestFalse(TEXT("Exit status redacts colon-bearing continuation"), Context.LastStatus.Contains(TEXT("opaque:value")));
        bPassed &= Test.TestFalse(TEXT("Exit status redacts padded base64 continuation"), Context.LastStatus.Contains(TEXT("QUJDREVGR0g")));
        bPassed &= Test.TestFalse(TEXT("Exit transcript redacts colon-bearing continuation"), ContainsTranscript(TranscriptEntries, TEXT("opaque:value")));
        bPassed &= Test.TestFalse(TEXT("Exit transcript redacts padded base64 continuation"), ContainsTranscript(TranscriptEntries, TEXT("QUJDREVGR0g")));
        bPassed &= Test.TestTrue(TEXT("Exit diagnostic retains a redaction marker"), Context.LastStatus.Contains(TEXT("[REDACTED]")));

        bPassed &= Test.TestTrue(TEXT("ACP security client restarts after stderr exit"), Client.Start(Context.TestDirectory));
        bPassed &= Test.TestTrue(TEXT("Restarted ACP security client becomes ready"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
        Client.SetAttachEditorContext(false);
        Client.SendPrompt(TEXT("stderr long exit path"));
        bPassed &= Test.TestTrue(TEXT("Long stderr ACP test server exit is observed"), PumpClientUntil(Client, [&Client]() { return !Client.IsRunning(); }));
        bPassed &= Test.TestTrue(TEXT("Long newline-free stderr remains bounded"), Context.LastStatus.Len() <= 4600);
        return bPassed;
    }
}

#endif
