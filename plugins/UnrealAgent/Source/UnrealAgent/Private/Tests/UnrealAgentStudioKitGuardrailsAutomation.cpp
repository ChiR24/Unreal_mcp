#if WITH_DEV_AUTOMATION_TESTS

#include "Acp/StudioKit/UnrealAgentStudioKit.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentStudioKitGuardrailsTest,
    "UnrealAgent.Acp.StudioKitGuardrails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentStudioKitGuardrailsTest::RunTest(const FString& Parameters)
{
    const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitGuardrailsTest")));
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
    {
        AddError(FString::Printf(TEXT("Failed to create Studio Kit guardrails test directory: %s"), *TestDirectory));
        return false;
    }

    bool bPassed = true;
    const FUnrealAgentStudioKitResult KitResult = FUnrealAgentStudioKit::EnsureForProject(TestDirectory);
    bPassed &= TestTrue(TEXT("Studio Kit generation succeeds"), KitResult.WasSuccessful());

    FString GuardrailsPluginText;
    const FString GuardrailsPluginPath = FPaths::Combine(TestDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
    bPassed &= TestTrue(TEXT("Guardrails plugin is readable"), FFileHelper::LoadFileToString(GuardrailsPluginText, *GuardrailsPluginPath));
    bPassed &= TestTrue(TEXT("Guardrails track route-card state"), GuardrailsPluginText.Contains(TEXT("sawRouteCard")) && GuardrailsPluginText.Contains(TEXT("MAX_MCP_MUTATIONS_AFTER_ROUTE_CARD")));
    bPassed &= TestTrue(TEXT("Guardrails record only assistant route-card event preflight"), GuardrailsPluginText.Contains(TEXT("\"event\"")) && GuardrailsPluginText.Contains(TEXT("ROUTE_CARD_FIELD_PATTERNS")) && GuardrailsPluginText.Contains(TEXT("recordRouteCardFromEvent(event)")) && GuardrailsPluginText.Contains(TEXT("role === \"assistant\"")) && !GuardrailsPluginText.Contains(TEXT("command.execute.before")));
    bPassed &= TestTrue(TEXT("Guardrails block direct project-state file writes"), GuardrailsPluginText.Contains(TEXT("UNREAL_PROJECT_STATE_PATH_PATTERN")) && GuardrailsPluginText.Contains(TEXT("rejectDirectUnrealProjectStateWrite")) && GuardrailsPluginText.Contains(TEXT("blocked direct Unreal project-state file write")));
    bPassed &= TestTrue(TEXT("Guardrails block local Content and package mutations"), GuardrailsPluginText.Contains(TEXT("UNREAL_CONTENT_TARGET_PATTERN")) && GuardrailsPluginText.Contains(TEXT("rejectDirectUnrealContentMutation")) && GuardrailsPluginText.Contains(TEXT("blocked direct Unreal content/package mutation")));
    bPassed &= TestTrue(TEXT("Guardrails recognize local command aliases"), GuardrailsPluginText.Contains(TEXT("LOCAL_COMMAND_TOOLS")) && GuardrailsPluginText.Contains(TEXT("execute_command")) && GuardrailsPluginText.Contains(TEXT("apply_patch")));
    bPassed &= TestTrue(TEXT("Guardrails fail closed outside explicit read-only local commands"), GuardrailsPluginText.Contains(TEXT("READ_ONLY_LOCAL_COMMAND_PATTERN")) && GuardrailsPluginText.Contains(TEXT("READ_ONLY_COMMAND_ESCAPE_PATTERN")) && GuardrailsPluginText.Contains(TEXT("isExplicitReadOnlyCommand")));
    bPassed &= TestTrue(TEXT("Guardrails resolve symlink aliases before protected writes"), GuardrailsPluginText.Contains(TEXT("realpathSync")) && GuardrailsPluginText.Contains(TEXT("resolveThroughExistingAncestor")) && GuardrailsPluginText.Contains(TEXT("containsResolvedProtectedPath")));
    bPassed &= TestTrue(TEXT("Guardrails protect Studio Kit self-modification"), GuardrailsPluginText.Contains(TEXT(".opencode")) && GuardrailsPluginText.Contains(TEXT("segments[0] === \".opencode\"")));
    bPassed &= TestTrue(TEXT("Guardrails block direct Unreal Python editor mutation"), GuardrailsPluginText.Contains(TEXT("UNREAL_PYTHON_MUTATION_PATTERN")) && GuardrailsPluginText.Contains(TEXT("rejectDirectUnrealEditorStateMutation")));
    bPassed &= TestTrue(TEXT("Guardrails protect action-scoped system_control mutations"), GuardrailsPluginText.Contains(TEXT("MCP_ACTION_SCOPED_MUTATION_TOOLS")) && GuardrailsPluginText.Contains(TEXT("set_project_setting")) && GuardrailsPluginText.Contains(TEXT("isProtectedMcpMutation(input, output)")));
    bPassed &= TestTrue(TEXT("Guardrails require exact MCP tool names and canonical output"), GuardrailsPluginText.Contains(TEXT("getExactMcpParentTool")) && GuardrailsPluginText.Contains(TEXT("unreal-engine_")) && GuardrailsPluginText.Contains(TEXT("toolOutputSucceeded")) && GuardrailsPluginText.Contains(TEXT("does not match the exact parent tool/action")));
    bPassed &= TestTrue(TEXT("Guardrails bind assistant route cards to sessions"), GuardrailsPluginText.Contains(TEXT("MCP_ASSISTANT_MESSAGE_SESSIONS")) && GuardrailsPluginText.Contains(TEXT("!== sessionID")));
    bPassed &= TestTrue(TEXT("Guardrails require route card before mutation"), GuardrailsPluginText.Contains(TEXT("blocked MCP editor mutation before route-card preflight")) && GuardrailsPluginText.Contains(TEXT("/unreal-route-card")));
    bPassed &= TestTrue(TEXT("Fresh inspect invalidates stale route cards"), GuardrailsPluginText.Contains(TEXT("state.sawRouteCard = false")) && GuardrailsPluginText.Contains(TEXT("after the current inspect")));
    bPassed &= TestTrue(TEXT("Route card batches are bounded"), GuardrailsPluginText.Contains(TEXT("blocked MCP editor mutation after stale route card")) && GuardrailsPluginText.Contains(TEXT("state.mutationsSinceRouteCard += 1")));

    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    return bPassed;
}

#endif
