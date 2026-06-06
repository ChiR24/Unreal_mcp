#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

namespace UnrealAgent::AutomationTests
{
    bool RunPanelSmokeTest(FAutomationTestBase& Test);
    bool RunStudioKitAndContextTest(FAutomationTestBase& Test);
    bool RunAcpClientProtocolTest(FAutomationTestBase& Test);
}
