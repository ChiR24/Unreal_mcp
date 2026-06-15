#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"
#include "Tests/UnrealAgentAcpSecurityChecks.h"
#include "Tests/UnrealAgentAcpSecurityPrompts.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpSecurityPathPolicyChecks(FAcpSecurityTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        TArray<FString>& TranscriptEntries = Context.TranscriptEntries;
        bool bPassed = true;

        const TCHAR* BlockedPrompts[] = {
            TEXT("unreal python actor read path"),
            TEXT("obfuscated unreal python path"),
            TEXT("env wrapped unreal python path"),
            TEXT("shell wrapped unreal python path"),
            TEXT("multiline command path"),
            TEXT("single ampersand mutation path"),
            TEXT("escaped separator mutation path"),
            TEXT("encoded unreal python path"),
            TEXT("git alias apply path"),
            TEXT("git quoted alias apply path"),
            TEXT("split content mkdir path"),
            TEXT("variable content mkdir path"),
            TEXT("split variable content mkdir path"),
            TEXT("python split content path"),
            TEXT("command substitution opencode write path"),
            TEXT("globbed binary content read path"),
            TEXT("binary search operand path"),
            TEXT("bracket binary content read path"),
            TEXT("brace binary content read path"),
            TEXT("brace prefix binary content read path"),
            TEXT("brace empty alternative binary content read path"),
            TEXT("brace sequence binary content read path"),
            TEXT("escaped binary content read path"),
            TEXT("ansi c quoted binary content read path"),
            TEXT("ansi c escaped binary content read path"),
            TEXT("adjacent quoted binary content read path"),
            TEXT("locale quoted binary content read path"),
            TEXT("adjacent double quoted binary content read path"),
            TEXT("long globbed binary content read path"),
            TEXT("brace expansion content path"),
            TEXT("python pathlib touch path"),
            TEXT("python keyword mode write path"),
            TEXT("node create write stream path"),
            TEXT("quote split content mkdir path"),
            TEXT("direct unreal editor commandlet path"),
            TEXT("quoted windows unreal editor path"),
            TEXT("unknown local location path"),
            TEXT("unknown local hidden content path"),
            TEXT("unknown local hidden config path"),
            TEXT("unknown save tool path"),
            TEXT("spoofed read run content path"),
            TEXT("spoofed read invoke content path"),
            TEXT("unknown local hidden unreal python path"),
            TEXT("spoofed read content payload path"),
            TEXT("spoofed read config payload path"),
            TEXT("spoofed hidden config payload path"),
            TEXT("custom reader hidden content payload path"),
            TEXT("binary alias read path"),
            TEXT("binary alias normal read path"),
            TEXT("binary destination path"),
            TEXT("binary patch path"),
            TEXT("custom reader binary path"),
            TEXT("custom reader hidden binary path"),
            TEXT("symlink docs write path"),
            TEXT("tree short output path")
        };
        for (const TCHAR* Prompt : BlockedPrompts)
        {
            TranscriptEntries.Reset();
            Client.SendPrompt(Prompt);
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s completes after automatic rejection"), Prompt),
                PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestFalse(
                FString::Printf(TEXT("%s never becomes pending"), Prompt),
                Client.HasPendingPermission());
            bPassed &= Test.TestTrue(
                FString::Printf(
                    TEXT("%s reports a direct-access block; transcript=%s"),
                    Prompt,
                    *FString::Join(TranscriptEntries, TEXT(" | "))),
                ContainsTranscript(TranscriptEntries, TEXT("Blocked direct")));
        }

        for (const FString& Prompt : GetDestructiveSecurityPrompts())
        {
            TranscriptEntries.Reset();
            Client.SendPrompt(Prompt);
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s completes after automatic rejection"), *Prompt),
                PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            bPassed &= Test.TestFalse(
                FString::Printf(TEXT("%s never becomes pending"), *Prompt),
                Client.HasPendingPermission());
            bPassed &= Test.TestTrue(
                FString::Printf(
                    TEXT("%s reports a destructive-command block; transcript=%s"),
                    *Prompt,
                    *FString::Join(TranscriptEntries, TEXT(" | "))),
                ContainsTranscript(TranscriptEntries, TEXT("Blocked destructive")));
            if (Client.HasPendingPermission())
            {
                Client.RejectPermission();
                PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); });
            }
        }

        Client.SendPrompt(TEXT("safe tar list path"));
        bPassed &= Test.TestTrue(TEXT("Tar list command requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Tar list command completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("safe symlink docs read path"));
        bPassed &= Test.TestTrue(TEXT("Safe symlink documentation read requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Safe symlink documentation read completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        TranscriptEntries.Reset();
        Client.SendPrompt(TEXT("safe locale quoted docs path"));
        const bool bLocaleReadPending = PumpClientUntil(
            Client,
            [&Client]()
            {
                return Client.HasPendingPermission()
                    || !Client.IsPromptInFlight();
            });
        bPassed &= Test.TestTrue(
            FString::Printf(
                TEXT("Safe locale-quoted documentation read requests permission; transcript=%s"),
                *FString::Join(TranscriptEntries, TEXT(" | "))),
            bLocaleReadPending && Client.HasPendingPermission());
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Safe locale-quoted documentation read completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        return bPassed;
    }
}

#endif
