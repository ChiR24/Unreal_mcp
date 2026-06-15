#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpClientProtocolChecks.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Misc/AutomationTest.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientPermissionBehaviorChecks(FAcpClientProtocolTestContext& Context)
    {
        FAutomationTestBase& Test = Context.Test;
        FOpenCodeAcpClient& Client = Context.Client;
        bool bPassed = true;

        Client.SendPrompt(TEXT("safe config read path"));
        bPassed &= Test.TestTrue(TEXT("Support-only config read requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestTrue(TEXT("Support-only config read can use Always allow"), Client.CanApprovePermissionAlways());
        bPassed &= Test.TestFalse(TEXT("Support-only config read has no MCP-first warning"), Context.LastPermission.Contains(TEXT("persistent approval disabled")));
        Client.ApprovePermissionAlways();
        bPassed &= Test.TestTrue(TEXT("Support-only config read completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("safe docs read path"));
        bPassed &= Test.TestTrue(TEXT("Support-only docs read requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestTrue(TEXT("Support-only docs read can use Always allow"), Client.CanApprovePermissionAlways());
        bPassed &= Test.TestFalse(TEXT("Support-only docs read has no MCP-first warning"), Context.LastPermission.Contains(TEXT("persistent approval disabled")));
        Client.ApprovePermissionAlways();
        bPassed &= Test.TestTrue(TEXT("Support-only docs read completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("safe extension content read path"));
        bPassed &= Test.TestTrue(TEXT("Unknown read-kind extension remains reviewable"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Unknown read-kind extension prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        const TCHAR* SafeInterpreterPrompts[] = {
            TEXT("safe python docs read path"),
            TEXT("safe python unreal text path"),
            TEXT("safe node analysis path"),
            TEXT("safe node function path"),
            TEXT("safe shell info path")
        };
        for (const TCHAR* Prompt : SafeInterpreterPrompts)
        {
            Client.SendPrompt(Prompt);
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s requests permission"), Prompt),
                PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s remains eligible for persistent approval"), Prompt),
                Client.CanApprovePermissionAlways());
            bPassed &= Test.TestFalse(
                FString::Printf(TEXT("%s has no MCP-first warning"), Prompt),
                Context.LastPermission.Contains(TEXT("persistent approval disabled")));
            Client.ApprovePermissionOnce();
            bPassed &= Test.TestTrue(
                FString::Printf(TEXT("%s completes"), Prompt),
                PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        }

        Client.SendPrompt(TEXT("safe binary mention path"));
        bPassed &= Test.TestTrue(TEXT("Harmless binary-extension search remains reviewable"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Harmless binary-extension search completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("secret permission path"));
        bPassed &= Test.TestTrue(TEXT("Secret-bearing permission request remains reviewable"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Permission description redacts bearer token"), Context.LastPermission.Contains(TEXT("permission-secret")));
        bPassed &= Test.TestFalse(TEXT("Permission description redacts password"), Context.LastPermission.Contains(TEXT("permission-password")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Secret-bearing permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("secret tool activity path"));
        bPassed &= Test.TestTrue(TEXT("Secret-bearing tool activity remains reviewable"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Tool activity transcript redacts bearer token"), ContainsTranscript(Context.TranscriptEntries, TEXT("transcript-secret")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Secret-bearing tool activity prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("source edit mentioning uasset extension"));
        bPassed &= Test.TestTrue(TEXT("Source edit mentioning binary asset extension still requests permission"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Source edit mention prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("unreal-engine mutation path"));
        bPassed &= Test.TestTrue(TEXT("Unreal permission request arrives"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Unreal permission cannot be approved persistently"), Client.CanApprovePermissionAlways());
        bPassed &= Test.TestTrue(TEXT("Unreal permission explains persistent approval is disabled"), Context.LastPermission.Contains(TEXT("persistent approval disabled")));
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Unreal one-shot permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("inspect permission path"));
        bPassed &= Test.TestTrue(TEXT("Inspect permission request arrives"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestFalse(TEXT("Inspect permission cannot be approved persistently"), Client.CanApprovePermissionAlways());
        Client.ApprovePermissionOnce();
        bPassed &= Test.TestTrue(TEXT("Inspect one-shot permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));

        Client.SendPrompt(TEXT("cancel path"));
        bPassed &= Test.TestTrue(TEXT("Permission request arrives before cancel"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= Test.TestTrue(TEXT("Non-Unreal permission can still expose always option"), Client.CanApprovePermissionAlways());
        Client.CancelPrompt();
        bPassed &= Test.TestTrue(TEXT("Cancelled prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= Test.TestTrue(TEXT("Cancel status reports cancelled stop reason"), Context.LastStatus.Contains(TEXT("cancelled")));
        return bPassed;
    }
}

#endif
