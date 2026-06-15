#if WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;
class FOpenCodeAcpClient;

namespace UnrealAgent::AutomationTests
{
    struct FAcpClientProtocolTestContext
    {
        FAutomationTestBase& Test;
        FOpenCodeAcpClient& Client;
        const FString& TestDirectory;
        FString& LastStatus;
        FString& LastPermission;
        TArray<FString>& TranscriptEntries;
        int32& ModelChangeCount;
    };

    bool RunAcpClientSessionChecks(FAcpClientProtocolTestContext& Context);
    bool RunAcpClientLocalAccessChecks(FAcpClientProtocolTestContext& Context);
    bool RunAcpClientPermissionBehaviorChecks(FAcpClientProtocolTestContext& Context);
    bool RunAcpClientMalformedPermissionChecks(FAcpClientProtocolTestContext& Context);
    bool RunAcpClientStartupSafetyChecks(FAutomationTestBase& Test);
}

#endif
