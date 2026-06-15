#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpClientProtocolChecks.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientLocalAccessChecks(FAcpClientProtocolTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        TArray<FString>& TranscriptEntries = Context.TranscriptEntries;
        bool bPassed = true;

        Client.SendPrompt(TEXT("direct uasset write path"));
        bPassed &= Test.TestTrue(TEXT("Direct binary asset write is auto-rejected"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Direct binary asset write never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestTrue(TEXT("Direct binary asset write reports MCP-first block"), ContainsTranscript(TranscriptEntries, TEXT("Blocked direct Unreal binary asset filesystem access")));
        bPassed &= Test.TestTrue(TEXT("Direct binary asset write selects ACP reject option"), ContainsTranscript(TranscriptEntries, TEXT("permission option reject")));

        auto CountRejectPermissionOptions = [&TranscriptEntries]()
        {
            int32 Count = 0;
            for (const FString& Entry : TranscriptEntries)
            {
                if (Entry.Contains(TEXT("permission option reject")))
                {
                    ++Count;
                }
            }
            return Count;
        };
        const int32 RejectTranscriptCountBeforeBinaryCat = CountRejectPermissionOptions();
        Client.SendPrompt(TEXT("direct binary cat path"));
        bPassed &= Test.TestTrue(TEXT("Direct binary asset shell read is auto-rejected"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Direct binary asset shell read never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestTrue(TEXT("Direct binary asset shell read reports MCP-first block"), ContainsTranscript(TranscriptEntries, TEXT("Blocked direct Unreal binary asset filesystem access")));
        bPassed &= Test.TestTrue(TEXT("Direct binary asset shell read selects ACP reject option"), CountRejectPermissionOptions() > RejectTranscriptCountBeforeBinaryCat);

        for (const TPair<FString, FString>& SafeDocumentationCase : {
            TPair<FString, FString>(TEXT("safe binary suffix docs path"), TEXT("Binary-looking documentation filename remains reviewable")),
            TPair<FString, FString>(TEXT("safe nested content docs path"), TEXT("Nested documentation Content folder remains reviewable")),
            TPair<FString, FString>(TEXT("safe nested config docs path"), TEXT("Nested documentation Config folder remains reviewable"))
        })
        {
            Client.SendPrompt(SafeDocumentationCase.Key);
            const bool bReachedPermissionDecision = PumpClientUntil(
                Client,
                [&Client]()
                {
                    return Client.HasPendingPermission() || !Client.IsPromptInFlight();
                });
            bPassed &= Test.TestTrue(*SafeDocumentationCase.Value, bReachedPermissionDecision && Client.HasPendingPermission());
            if (Client.HasPendingPermission())
            {
                Client.RejectPermission();
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s prompt completes after explicit rejection"), *SafeDocumentationCase.Key),
                    PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            }
        }

        const int32 RejectTranscriptCountBeforeProjectConfig = CountRejectPermissionOptions();
        Client.SendPrompt(TEXT("direct project config write path"));
        bPassed &= Test.TestTrue(TEXT("Direct project config write is auto-rejected"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Direct project config write never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestTrue(TEXT("Direct project config write reports MCP-first block"), ContainsTranscript(TranscriptEntries, TEXT("Blocked direct Unreal project-state file write")));
        bPassed &= Test.TestTrue(TEXT("Direct project config write selects ACP reject option"), CountRejectPermissionOptions() > RejectTranscriptCountBeforeProjectConfig);

        struct FUnsafeLocalMutationCase
        {
            const TCHAR* Prompt;
            const TCHAR* ExpectedBlock;
        };
        const FUnsafeLocalMutationCase UnsafeLocalMutationCases[] = {
            { TEXT("apply patch config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("apply patch content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("apply patch text content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("shell redirect config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("shell tee config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("command sed config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("execute python config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("execute node config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("execute powershell config path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("execute pathlib content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute ruby content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute dd content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("spoofed read command content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("spoofed read run content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("spoofed read content payload path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("spoofed read config payload path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("execute rsync content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute sponge content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute tar content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute ruby io content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute mkdir content path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("split variable content mkdir path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("execute python binary read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("execute node binary read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("binary destination path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("binary patch path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("execute opencode self edit path"), TEXT("Blocked direct Unreal project-state file write") },
            { TEXT("semantic actor delete path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("unreal python actor delete path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("ripgrep preprocessor execution path"), TEXT("Blocked destructive local shell command") },
            { TEXT("wrapped ripgrep preprocessor execution path"), TEXT("Blocked destructive local shell command") },
            { TEXT("wrapped fd execution path"), TEXT("Blocked destructive local shell command") },
            { TEXT("shell encoded interpreter path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("encoded interpreter content path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("command substitution opencode write path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("globbed binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("bracket binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("brace binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("brace prefix binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("brace empty alternative binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("brace sequence binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("escaped binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("ansi c quoted binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("ansi c escaped binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("adjacent quoted binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("locale quoted binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("adjacent double quoted binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("long globbed binary content read path"), TEXT("Blocked direct Unreal binary asset filesystem access") },
            { TEXT("brace expansion content path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("python pathlib touch path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("python keyword mode write path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("node create write stream path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("bsdtar extract path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("quoted windows unreal editor path"), TEXT("Blocked direct local Unreal editor-state access") },
            { TEXT("execute command content remove path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute command content root remove path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("execute command game move path"), TEXT("Blocked direct Unreal content/package mutation") },
            { TEXT("apply patch engine path"), TEXT("Blocked direct Unreal content/package mutation") }
        };
        for (const FUnsafeLocalMutationCase& MutationCase : UnsafeLocalMutationCases)
        {
            const int32 RejectCountBeforeMutation = CountRejectPermissionOptions();
            Client.SendPrompt(MutationCase.Prompt);
            const bool bReachedPermissionDecision = PumpClientUntil(
                Client,
                [&Client]()
                {
                    return Client.HasPendingPermission() || !Client.IsPromptInFlight();
                });
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s is auto-rejected"), MutationCase.Prompt),
                bReachedPermissionDecision && !Client.HasPendingPermission());
            bPassed &= Test.TestFalse(
                FString::Printf(TEXT("%s never becomes pending"), MutationCase.Prompt),
                Client.HasPendingPermission());
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s reports its MCP-first block"), MutationCase.Prompt),
                ContainsTranscript(TranscriptEntries, MutationCase.ExpectedBlock));
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s selects the ACP reject option"), MutationCase.Prompt),
                CountRejectPermissionOptions() > RejectCountBeforeMutation);
            if (Client.HasPendingPermission())
            {
                Client.RejectPermission();
                bPassed &= Test.TestTrue(
                    FString::Printf(TEXT("%s cleanup rejection completes"), MutationCase.Prompt),
                    PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
            }
        }
        return bPassed;
    }
}

#endif
