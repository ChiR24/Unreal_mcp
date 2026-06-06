#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationTestDelegates.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentAcpPanelSmokeTest,
    "UnrealAgent.Acp.PanelOpens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentAcpPanelSmokeTest::RunTest(const FString& Parameters)
{
    return UnrealAgent::AutomationTests::RunPanelSmokeTest(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentStudioKitAndContextTest,
    "UnrealAgent.Acp.StudioKitAndContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentStudioKitAndContextTest::RunTest(const FString& Parameters)
{
    return UnrealAgent::AutomationTests::RunStudioKitAndContextTest(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentAcpClientProtocolTest,
    "UnrealAgent.Acp.ClientProtocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentAcpClientProtocolTest::RunTest(const FString& Parameters)
{
    return UnrealAgent::AutomationTests::RunAcpClientProtocolTest(*this);
}

#endif
