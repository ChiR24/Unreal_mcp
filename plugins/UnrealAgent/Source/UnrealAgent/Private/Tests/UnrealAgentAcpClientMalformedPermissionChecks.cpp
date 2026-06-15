#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpClientProtocolChecks.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientMalformedPermissionChecks(FAcpClientProtocolTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        TArray<FString>& TranscriptEntries = Context.TranscriptEntries;
        bool bPassed = true;

        auto CountPermissionOptionSelections = [&TranscriptEntries]()
        {
            int32 Count = 0;
            for (const FString& Entry : TranscriptEntries)
            {
                if (Entry.Contains(TEXT("permission option once"))
                    || Entry.Contains(TEXT("permission option always"))
                    || Entry.Contains(TEXT("permission option reject")))
                {
                    ++Count;
                }
            }
            return Count;
        };
        const int32 SelectionCountBeforeMismatchedAlways = CountPermissionOptionSelections();
        Client.SendPrompt(TEXT("mismatched always path"));
        bPassed &= Test.TestTrue(
            TEXT("Allow-id reject-kind conflict completes through an ACP error"),
            PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Allow-id reject-kind conflict never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestEqual(TEXT("Allow-id reject-kind conflict selects no option"), CountPermissionOptionSelections(), SelectionCountBeforeMismatchedAlways);
        bPassed &= Test.TestTrue(
            TEXT("Allow-id reject-kind conflict is rejected fail closed"),
            ContainsTranscript(TranscriptEntries, TEXT("permission error Permission option id conflicts with kind.")));

        const int32 SelectionCountBeforeConflictingReject = CountPermissionOptionSelections();
        Client.SendPrompt(TEXT("conflicting reject option path"));
        bPassed &= Test.TestTrue(
            TEXT("Conflicting reject option prompt completes through an ACP error"),
            PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Conflicting reject option never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestEqual(TEXT("Conflicting reject id is not selected when its kind allows"), CountPermissionOptionSelections(), SelectionCountBeforeConflictingReject);
        bPassed &= Test.TestTrue(
            TEXT("Conflicting reject option is rejected fail closed"),
            ContainsTranscript(TranscriptEntries, TEXT("permission error Permission option id conflicts with kind.")));

        const int32 SelectionCountBeforePlainAllowConflict = CountPermissionOptionSelections();
        Client.SendPrompt(TEXT("plain allow conflict path"));
        bPassed &= Test.TestTrue(
            TEXT("Plain allow-id reject-kind conflict completes through an ACP error"),
            PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Plain allow-id reject-kind conflict never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestEqual(TEXT("Plain allow-id reject-kind conflict selects no option"), CountPermissionOptionSelections(), SelectionCountBeforePlainAllowConflict);
        bPassed &= Test.TestTrue(
            TEXT("Plain allow-id reject-kind conflict is rejected fail closed"),
            ContainsTranscript(TranscriptEntries, TEXT("permission error Permission option id conflicts with kind.")));

        const int32 SelectionCountBeforeDuplicateOptionId = CountPermissionOptionSelections();
        Client.SendPrompt(TEXT("duplicate option id path"));
        bPassed &= Test.TestTrue(
            TEXT("Duplicate permission option id completes through an ACP error"),
            PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestFalse(TEXT("Duplicate permission option id never becomes pending"), Client.HasPendingPermission());
        bPassed &= Test.TestEqual(TEXT("Duplicate permission option id selects no option"), CountPermissionOptionSelections(), SelectionCountBeforeDuplicateOptionId);
        bPassed &= Test.TestTrue(
            TEXT("Duplicate permission option id is rejected fail closed"),
            ContainsTranscript(TranscriptEntries, TEXT("permission error Permission option ids must be unique.")));

        Client.SendPrompt(TEXT("error after permission path"));
        bPassed &= Test.TestTrue(TEXT("Prompt error completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(TEXT("Prompt error is reported"), ContainsTranscript(TranscriptEntries, TEXT("ACP test prompt error")));

        Client.SendPrompt(TEXT("no options path"));
        bPassed &= Test.TestTrue(TEXT("Malformed permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(TEXT("Malformed permission is reported"), ContainsTranscript(TranscriptEntries, TEXT("no selectable options")));
        return bPassed;
    }
}

#endif
