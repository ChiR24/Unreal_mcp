#if WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;
class FOpenCodeAcpClient;

namespace UnrealAgent::AutomationTests
{
    struct FAcpSecurityTestContext
    {
        FAutomationTestBase& Test;
        FOpenCodeAcpClient& Client;
        const FString& TestDirectory;
        FString& LastStatus;
        TArray<FString>& TranscriptEntries;
    };

    bool RunAcpSecurityPathPolicyChecks(FAcpSecurityTestContext& Context);
    bool RunAcpSecurityRedactionChecks(FAcpSecurityTestContext& Context);
}

#endif
