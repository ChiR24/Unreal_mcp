#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitGuardrailContentChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        const FString ConfigPath = FPaths::Combine(TestDirectory, TEXT(".opencode/opencode.json"));
        const FString PluginPath = FPaths::Combine(TestDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
        FString OpenCodeConfigText;
        bPassed &= Test.TestTrue(TEXT("OpenCode config is readable"), FFileHelper::LoadFileToString(OpenCodeConfigText, *ConfigPath));
        bPassed &= Test.TestTrue(TEXT("OpenCode config has Studio Kit comment marker"), OpenCodeConfigText.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
        FString GuardrailsPluginText;
        bPassed &= Test.TestTrue(TEXT("Guardrails plugin is readable"), FFileHelper::LoadFileToString(GuardrailsPluginText, *PluginPath));
        bPassed &= Test.TestTrue(TEXT("Guardrails reminder includes MCP route cards"), GuardrailsPluginText.Contains(TEXT("write MCP route cards before mutations")));
        bPassed &= Test.TestTrue(TEXT("Guardrails block direct binary asset local tools"), GuardrailsPluginText.Contains(TEXT("containsPathLikeValue")) && GuardrailsPluginText.Contains(TEXT("blocked direct .uasset/.umap filesystem access")) && GuardrailsPluginText.Contains(TEXT("throw new Error")));
        bPassed &= Test.TestTrue(TEXT("Guardrails block direct project-state local writes"), GuardrailsPluginText.Contains(TEXT("UNREAL_PROJECT_STATE_PATH_PATTERN")) && GuardrailsPluginText.Contains(TEXT("rejectDirectUnrealProjectStateWrite")) && GuardrailsPluginText.Contains(TEXT("blocked direct Unreal project-state file write")));
        bPassed &= Test.TestTrue(TEXT("Guardrails block destructive local shell tools"), GuardrailsPluginText.Contains(TEXT("rejectDestructiveLocalShellAccess")) && GuardrailsPluginText.Contains(TEXT("containsDestructiveGitCommand")) && GuardrailsPluginText.Contains(TEXT("DESTRUCTIVE_INTERPRETER_PATTERN")) && GuardrailsPluginText.Contains(TEXT("blocked destructive local shell command")));
        bPassed &= Test.TestTrue(TEXT("Guardrails scope recursive delete blocking to Unreal project state"), GuardrailsPluginText.Contains(TEXT("containsDestructiveRecursiveRemove")) && GuardrailsPluginText.Contains(TEXT("isProjectRootRemoveTarget")) && GuardrailsPluginText.Contains(TEXT("UNREAL_PROJECT_DIRECTORY_PATTERN")) && GuardrailsPluginText.Contains(TEXT("GUARDRAIL_PROJECT_DIRECTORY")));
        bPassed &= Test.TestTrue(TEXT("Guardrails distinguish path fields from text content"), GuardrailsPluginText.Contains(TEXT("PATH_KEY_PATTERN")) && GuardrailsPluginText.Contains(TEXT("pathContext")) && GuardrailsPluginText.Contains(TEXT("typeof args ===")));
        bPassed &= Test.TestTrue(TEXT("Guardrails enforce MCP mutation preflight"), GuardrailsPluginText.Contains(TEXT("enforceMcpMutationPreflight")) && GuardrailsPluginText.Contains(TEXT("recordMcpPreflightSuccess")) && GuardrailsPluginText.Contains(TEXT("MCP_PROTECTED_PARENT_TOOLS")) && GuardrailsPluginText.Contains(TEXT("blocked MCP editor mutation before completed preflight")) && GuardrailsPluginText.Contains(TEXT("/unreal-tool-inventory")));
        bPassed &= Test.TestTrue(TEXT("Guardrails record successful preflight only after tool execution"), GuardrailsPluginText.Contains(TEXT("tool.execute.after")) && GuardrailsPluginText.Contains(TEXT("recordMcpPreflightSuccess(input, output)")) && GuardrailsPluginText.Contains(TEXT("toolOutputSucceeded(output)")));
        bPassed &= Test.TestTrue(TEXT("Guardrails require exact MCP parents and canonical success evidence"), GuardrailsPluginText.Contains(TEXT("getExactMcpParentTool")) && GuardrailsPluginText.Contains(TEXT("toolOutputSucceeded")) && GuardrailsPluginText.Contains(TEXT("does not match the exact parent tool/action")) && !GuardrailsPluginText.Contains(TEXT("matchesCanonicalTool")));
        bPassed &= Test.TestTrue(TEXT("Guardrails scope MCP mutation preflight by OpenCode session"), GuardrailsPluginText.Contains(TEXT("MCP_PREFLIGHT_STATE")) && GuardrailsPluginText.Contains(TEXT("getSessionKey")) && GuardrailsPluginText.Contains(TEXT("sessionID")));
        bPassed &= Test.TestTrue(TEXT("Guardrails reject noncanonical MCP preflight output"), GuardrailsPluginText.Contains(TEXT("toolOutputSucceeded")) && GuardrailsPluginText.Contains(TEXT("parseStructuredToolOutput")) && GuardrailsPluginText.Contains(TEXT("hasError")));
        bPassed &= Test.TestTrue(TEXT("Guardrails require fresh inspection after mutation batches"), GuardrailsPluginText.Contains(TEXT("MAX_MCP_MUTATIONS_AFTER_INSPECTION")) && GuardrailsPluginText.Contains(TEXT("recordMcpMutationResult")) && GuardrailsPluginText.Contains(TEXT("blocked MCP editor mutation after stale inspection")));
        bPassed &= Test.TestTrue(TEXT("Guardrails protect editor mutation parent tools"), GuardrailsPluginText.Contains(TEXT("manage_asset")) && GuardrailsPluginText.Contains(TEXT("manage_blueprint")) && GuardrailsPluginText.Contains(TEXT("control_actor")) && GuardrailsPluginText.Contains(TEXT("manage_level")) && GuardrailsPluginText.Contains(TEXT("control_editor")));
        bPassed &= Test.TestTrue(TEXT("Guardrails protect system_control mutation actions"), GuardrailsPluginText.Contains(TEXT("MCP_ACTION_SCOPED_MUTATION_TOOLS")) && GuardrailsPluginText.Contains(TEXT("set_project_setting")) && GuardrailsPluginText.Contains(TEXT("execute_python")));
        bPassed &= Test.TestTrue(TEXT("Guardrails use only assistant events for route cards"), GuardrailsPluginText.Contains(TEXT("recordRouteCardFromEvent")) && GuardrailsPluginText.Contains(TEXT("message.part.updated")) && GuardrailsPluginText.Contains(TEXT("role === \"assistant\"")) && !GuardrailsPluginText.Contains(TEXT("chat.message")) && !GuardrailsPluginText.Contains(TEXT("command.execute.before")));
        bPassed &= Test.TestTrue(TEXT("Guardrails bind assistant route cards to matching sessions"), GuardrailsPluginText.Contains(TEXT("MCP_ASSISTANT_MESSAGE_SESSIONS")) && GuardrailsPluginText.Contains(TEXT("!== sessionID")));
        bPassed &= Test.TestTrue(TEXT("OpenCode config asks globally and for local tools"), OpenCodeConfigText.Contains(TEXT("\"*\": \"ask\"")) && OpenCodeConfigText.Contains(TEXT("\"read\": \"ask\"")) && OpenCodeConfigText.Contains(TEXT("\"write\": \"ask\"")) && OpenCodeConfigText.Contains(TEXT("\"apply_patch\": \"ask\"")));
        bPassed &= Test.TestTrue(TEXT("OpenCode config asks for exact Unreal MCP parents"), OpenCodeConfigText.Contains(TEXT("\"unreal-engine_manage_tools\": \"ask\"")) && OpenCodeConfigText.Contains(TEXT("\"unreal-engine_inspect\": \"ask\"")) && OpenCodeConfigText.Contains(TEXT("\"unreal-engine_control_actor\": \"ask\"")) && !OpenCodeConfigText.Contains(TEXT("\"unreal-engine_*\"")));
        bPassed &= Test.TestFalse(TEXT("OpenCode config does not contain unknown Studio Kit keys"), OpenCodeConfigText.Contains(TEXT("\"unreal_agent_studio_kit_version\"")));
        return bPassed;
    }
}

#endif
