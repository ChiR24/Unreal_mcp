#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::Validation
{
namespace
{
    void AddFileCheck(FUnrealAgentValidationResult& Result, const FString& Label, const FString& Path, const FString& RequiredText = FString())
    {
        FString FileText;
        const bool bExists = FPaths::FileExists(Path);
        const bool bLoaded = RequiredText.IsEmpty() || (bExists && FFileHelper::LoadFileToString(FileText, *Path));
        const bool bContainsRequiredText = RequiredText.IsEmpty() || (bLoaded && FileText.Contains(RequiredText));
        if (bExists && bContainsRequiredText)
        {
            Result.Checks.Add(FString::Printf(TEXT("OK %s: %s"), *Label, *Path));
            return;
        }

        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(TEXT("Missing or unmanaged %s: %s"), *Label, *Path));
    }

    void AddFileContentCheck(FUnrealAgentValidationResult& Result, const FString& Label, const FString& Path, const FString& RequiredText)
    {
        FString FileText;
        if (FPaths::FileExists(Path) && FFileHelper::LoadFileToString(FileText, *Path) && FileText.Contains(RequiredText))
        {
            Result.Checks.Add(FString::Printf(TEXT("OK %s: %s"), *Label, *Path));
            return;
        }

        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(TEXT("Missing %s in %s"), *Label, *Path));
    }

}

void AddStudioKitValidationChecks(FUnrealAgentValidationResult& Result, const FString& NormalizedProjectDirectory)
{
    const FString Marker = FUnrealAgentStudioKit::GetStudioKitVersionMarker();
    AddFileCheck(Result, TEXT("primary agent"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/agents/unreal-agent.md")), FUnrealAgentStudioKit::GetPromptVersionMarker());
    const FString ToolPlaybookPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md"));
    AddFileCheck(Result, TEXT("tool playbook skill"), ToolPlaybookPath, Marker);
    AddFileContentCheck(Result, TEXT("Content Browser inspect guidance"), ToolPlaybookPath, TEXT("get_content_browser_state"));
    AddFileContentCheck(Result, TEXT("MCP tool inventory discipline"), ToolPlaybookPath, TEXT("compact tool inventory"));
    AddFileContentCheck(Result, TEXT("MCP mutation discipline"), ToolPlaybookPath, TEXT("direct filesystem writes"));
    AddFileContentCheck(Result, TEXT("MCP route card discipline"), ToolPlaybookPath, TEXT("MCP route card"));
    AddFileContentCheck(Result, TEXT("MCP stale inspection discipline"), ToolPlaybookPath, TEXT("short batch of protected MCP mutations"));
    AddFileContentCheck(Result, TEXT("data save accessibility plan discipline"), ToolPlaybookPath, TEXT("/unreal-data-save-accessibility-plan"));
    AddFileContentCheck(Result, TEXT("source control plan discipline"), ToolPlaybookPath, TEXT("/unreal-source-control-plan"));
    AddFileContentCheck(Result, TEXT("performance insights plan discipline"), ToolPlaybookPath, TEXT("/unreal-performance-insights-plan"));
    AddFileContentCheck(Result, TEXT("project setup plan discipline"), ToolPlaybookPath, TEXT("/unreal-project-setup-plan"));
    AddFileContentCheck(Result, TEXT("diagnostics crash recovery plan discipline"), ToolPlaybookPath, TEXT("/unreal-diagnostics-plan"));
    const FString RouteCardPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-mcp-route-card/SKILL.md"));
    AddFileCheck(Result, TEXT("MCP route card skill"), RouteCardPath, Marker);
    AddFileContentCheck(Result, TEXT("MCP route card required fields"), RouteCardPath, TEXT("Required fields"));
    AddFileContentCheck(Result, TEXT("MCP route card tool route"), RouteCardPath, TEXT("Tool route"));
    AddFileContentCheck(Result, TEXT("MCP route card rollback discipline"), RouteCardPath, TEXT("Rollback"));
    const FString PieSieSafetyPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-pie-sie-safety/SKILL.md"));
    AddFileCheck(Result, TEXT("PIE/SIE safety skill"), PieSieSafetyPath, Marker);
    AddFileContentCheck(Result, TEXT("PIE/SIE transient state discipline"), PieSieSafetyPath, TEXT("transient"));
    AddFileContentCheck(Result, TEXT("PIE/SIE persistence discipline"), PieSieSafetyPath, TEXT("Keep Simulation Changes"));
    AddFileContentCheck(Result, TEXT("PIE/SIE post-stop inspection"), PieSieSafetyPath, TEXT("post-stop inspect"));
    const FString EditorControlDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-editor-control-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("editor control discipline skill"), EditorControlDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("editor control route discipline"), EditorControlDisciplinePath, TEXT("narrow control_editor MCP route"));
    AddFileContentCheck(Result, TEXT("editor control viewport and selection discipline"), EditorControlDisciplinePath, TEXT("viewport/selection/tab/PIE/SIE/dirty-package state"));
    AddFileContentCheck(Result, TEXT("editor control modal and transaction discipline"), EditorControlDisciplinePath, TEXT("modal dialogs, and transaction/undo/redo operations"));
    AddFileContentCheck(Result, TEXT("editor control persistence discipline"), EditorControlDisciplinePath, TEXT("Validate persistence separately"));
    const FString ContentDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-content-browser-asset-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("Content Browser asset discipline skill"), ContentDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Content Browser package path discipline"), ContentDisciplinePath, TEXT("/Game package path"));
    AddFileContentCheck(Result, TEXT("Content Browser dependency discipline"), ContentDisciplinePath, TEXT("dependency and redirector risk"));
    AddFileContentCheck(Result, TEXT("Content Browser MCP mutation discipline"), ContentDisciplinePath, TEXT("do not edit .uasset or .umap files directly"));
    const FString BlueprintDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-blueprint-compile-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("Blueprint compile discipline skill"), BlueprintDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Blueprint compile validation discipline"), BlueprintDisciplinePath, TEXT("compile the Blueprint"));
    AddFileContentCheck(Result, TEXT("Blueprint save validation discipline"), BlueprintDisciplinePath, TEXT("save the touched asset"));
    AddFileContentCheck(Result, TEXT("Blueprint runtime validation discipline"), BlueprintDisciplinePath, TEXT("runtime success from persisted asset save evidence"));
    const FString LevelActorDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-level-actor-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("level and Actor discipline skill"), LevelActorDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Actor root transform discipline"), LevelActorDisciplinePath, TEXT("root SceneComponent"));
    AddFileContentCheck(Result, TEXT("World Partition ownership discipline"), LevelActorDisciplinePath, TEXT("Is Spatially Loaded"));
    AddFileContentCheck(Result, TEXT("level save and runtime discipline"), LevelActorDisciplinePath, TEXT("saved level state, and runtime behavior evidence"));
    const FString WorldBuildingDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-world-building-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("world building discipline skill"), WorldBuildingDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("world building MCP route discipline"), WorldBuildingDisciplinePath, TEXT("build_environment/manage_pcg/manage_geometry/manage_level_structure"));
    AddFileContentCheck(Result, TEXT("world building persistence discipline"), WorldBuildingDisciplinePath, TEXT("persisted map/external-actor state"));
    const FString GameplayInputDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-gameplay-input-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("gameplay and input discipline skill"), GameplayInputDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Enhanced Input mapping discipline"), GameplayInputDisciplinePath, TEXT("Input Actions, Mapping Contexts"));
    AddFileContentCheck(Result, TEXT("gameplay possession discipline"), GameplayInputDisciplinePath, TEXT("correct pawn is possessed"));
    AddFileContentCheck(Result, TEXT("gameplay PIE validation discipline"), GameplayInputDisciplinePath, TEXT("camera and HUD respond"));
    const FString AnimationPhysicsDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-animation-physics-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("animation and physics discipline skill"), AnimationPhysicsDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Animation Blueprint and Skeleton discipline"), AnimationPhysicsDisciplinePath, TEXT("Skeletal Mesh, Skeleton, Animation Blueprint"));
    AddFileContentCheck(Result, TEXT("Physics Asset collision discipline"), AnimationPhysicsDisciplinePath, TEXT("Physics Asset bodies/constraints"));
    AddFileContentCheck(Result, TEXT("animation PIE validation discipline"), AnimationPhysicsDisciplinePath, TEXT("notifies fire at the expected time"));
    const FString VfxMaterialDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-vfx-material-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("VFX and material discipline skill"), VfxMaterialDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Niagara ownership discipline"), VfxMaterialDisciplinePath, TEXT("Niagara System, emitter stack"));
    AddFileContentCheck(Result, TEXT("Material instance discipline"), VfxMaterialDisciplinePath, TEXT("Material or Material Instance"));
    AddFileContentCheck(Result, TEXT("VFX PIE screenshot discipline"), VfxMaterialDisciplinePath, TEXT("capture a viewport screenshot"));
    const FString AudioDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-audio-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("audio discipline skill"), AudioDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("MetaSound and Audio Component discipline"), AudioDisciplinePath, TEXT("MetaSound Source or Preset, Audio Component"));
    AddFileContentCheck(Result, TEXT("audio PIE validation discipline"), AudioDisciplinePath, TEXT("attenuation and spatialization respond"));
    const FString CinematicSequenceDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-cinematic-sequence-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("cinematic sequence discipline skill"), CinematicSequenceDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Level Sequence ownership discipline"), CinematicSequenceDisciplinePath, TEXT("Level Sequence Asset, Level Sequence Actor"));
    AddFileContentCheck(Result, TEXT("Sequencer playback validation discipline"), CinematicSequenceDisciplinePath, TEXT("camera cuts take control correctly"));
    const FString NetworkingGasDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-networking-gas-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("networking and GAS discipline skill"), NetworkingGasDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("network authority discipline"), NetworkingGasDisciplinePath, TEXT("server authority"));
    AddFileContentCheck(Result, TEXT("GAS owner and prediction discipline"), NetworkingGasDisciplinePath, TEXT("Ability System Component owner/avatar"));
    const FString CharacterSystemsDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-character-systems-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("character systems discipline skill"), CharacterSystemsDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Character Movement and collision discipline"), CharacterSystemsDisciplinePath, TEXT("Character Movement Component"));
    AddFileContentCheck(Result, TEXT("combat inventory interaction discipline"), CharacterSystemsDisciplinePath, TEXT("inventory/equipment state"));
    const FString UiHudDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-ui-hud-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("UI and HUD discipline skill"), UiHudDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Widget Blueprint and UserWidget discipline"), UiHudDisciplinePath, TEXT("Widget Blueprint/UserWidget"));
    AddFileContentCheck(Result, TEXT("CommonUI input focus discipline"), UiHudDisciplinePath, TEXT("CommonUI activation stack"));
    AddFileContentCheck(Result, TEXT("UI PIE viewport validation discipline"), UiHudDisciplinePath, TEXT("expected widget appears in the viewport"));
    const FString AiNavigationDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-ai-navigation-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("AI and navigation discipline skill"), AiNavigationDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("Behavior Tree and Blackboard discipline"), AiNavigationDisciplinePath, TEXT("Behavior Tree, Blackboard"));
    AddFileContentCheck(Result, TEXT("NavMesh pathing discipline"), AiNavigationDisciplinePath, TEXT("Nav Mesh Bounds Volume"));
    AddFileContentCheck(Result, TEXT("AI PIE validation discipline"), AiNavigationDisciplinePath, TEXT("AIController possesses the expected pawn"));
    const FString DataSaveAccessibilityDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-data-save-accessibility-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("data save accessibility discipline skill"), DataSaveAccessibilityDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("SaveGame and data ownership discipline"), DataSaveAccessibilityDisciplinePath, TEXT("SaveGame Blueprint or C++ class"));
    AddFileContentCheck(Result, TEXT("data save MCP route discipline"), DataSaveAccessibilityDisciplinePath, TEXT("manage_asset/manage_inventory/manage_blueprint/system_control"));
    AddFileContentCheck(Result, TEXT("localization evidence discipline"), DataSaveAccessibilityDisciplinePath, TEXT("localized text resolves"));
    AddFileContentCheck(Result, TEXT("accessibility persistence discipline"), DataSaveAccessibilityDisciplinePath, TEXT("accessibility options can be toggled and persisted"));
    const FString SourceControlCollaborationDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-source-control-collaboration-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("source control collaboration discipline skill"), SourceControlCollaborationDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("source control state discipline"), SourceControlCollaborationDisciplinePath, TEXT("get_source_control_state"));
    AddFileContentCheck(Result, TEXT("source control checkout submit discipline"), SourceControlCollaborationDisciplinePath, TEXT("source_control_checkout/source_control_submit"));
    AddFileContentCheck(Result, TEXT("source control team risk discipline"), SourceControlCollaborationDisciplinePath, TEXT("checked out by another user"));
    AddFileContentCheck(Result, TEXT("source control local git separation discipline"), SourceControlCollaborationDisciplinePath, TEXT("Do not use git add/commit/reset/checkout/clean"));
    const FString PerformanceInsightsDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-performance-insights-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("performance insights discipline skill"), PerformanceInsightsDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("performance baseline discipline"), PerformanceInsightsDisciplinePath, TEXT("baseline metric"));
    AddFileContentCheck(Result, TEXT("performance route discipline"), PerformanceInsightsDisciplinePath, TEXT("manage_performance actions"));
    AddFileContentCheck(Result, TEXT("Insights trace route discipline"), PerformanceInsightsDisciplinePath, TEXT("capture_insights_trace/get_trace_status/analyze_trace"));
    AddFileContentCheck(Result, TEXT("performance config-only separation discipline"), PerformanceInsightsDisciplinePath, TEXT("setting change is not proof"));
    const FString SystemProjectDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-system-project-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("system and project discipline skill"), SystemProjectDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("system control route discipline"), SystemProjectDisciplinePath, TEXT("narrow system_control MCP route"));
    AddFileContentCheck(Result, TEXT("console and Python automation discipline"), SystemProjectDisciplinePath, TEXT("console variables, console commands, Python/editor utility automation"));
    AddFileContentCheck(Result, TEXT("packaging and profiling evidence discipline"), SystemProjectDisciplinePath, TEXT("Do not claim packaging or performance from config-only evidence"));
    AddFileContentCheck(Result, TEXT("system operation output evidence discipline"), SystemProjectDisciplinePath, TEXT("capture exact command output or log paths"));
    const FString ProjectSetupTemplateDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-project-setup-template-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("project setup template discipline skill"), ProjectSetupTemplateDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("project setup read-back discipline"), ProjectSetupTemplateDisciplinePath, TEXT("system_control get_project_settings read-back"));
    AddFileContentCheck(Result, TEXT("project setup route discipline"), ProjectSetupTemplateDisciplinePath, TEXT("system_control get_project_settings before set_project_setting"));
    AddFileContentCheck(Result, TEXT("project setup raw content copy guardrail"), ProjectSetupTemplateDisciplinePath, TEXT("Do not copy Content directories"));
    AddFileContentCheck(Result, TEXT("project setup startup validation discipline"), ProjectSetupTemplateDisciplinePath, TEXT("run PIE for startup map/GameMode/Pawn/input behavior"));
    const FString DiagnosticsCrashRecoveryDisciplinePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-diagnostics-crash-recovery-discipline/SKILL.md"));
    AddFileCheck(Result, TEXT("diagnostics crash recovery discipline skill"), DiagnosticsCrashRecoveryDisciplinePath, Marker);
    AddFileContentCheck(Result, TEXT("diagnostics exact baseline discipline"), DiagnosticsCrashRecoveryDisciplinePath, TEXT("exact error text"));
    AddFileContentCheck(Result, TEXT("diagnostics call stack discipline"), DiagnosticsCrashRecoveryDisciplinePath, TEXT("call stack"));
    AddFileContentCheck(Result, TEXT("diagnostics route discipline"), DiagnosticsCrashRecoveryDisciplinePath, TEXT("run_tests/validate_assets/run_ubt"));
    AddFileContentCheck(Result, TEXT("diagnostics cache deletion guardrail"), DiagnosticsCrashRecoveryDisciplinePath, TEXT("Do not clear caches"));
    AddFileContentCheck(Result, TEXT("diagnostics original failure retest"), DiagnosticsCrashRecoveryDisciplinePath, TEXT("original exact error must be absent"));
    const FString OfficialGettingStartedPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-official-getting-started/SKILL.md"));
    AddFileCheck(Result, TEXT("official getting started skill"), OfficialGettingStartedPath, Marker);
    AddFileContentCheck(Result, TEXT("official getting started docs anchors"), OfficialGettingStartedPath, TEXT("Create your First Project"));
    AddFileContentCheck(Result, TEXT("official getting started MCP inspection"), OfficialGettingStartedPath, TEXT("get_content_browser_state"));
    const FString EditorOrientationPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-editor-orientation/SKILL.md"));
    AddFileCheck(Result, TEXT("editor orientation skill"), EditorOrientationPath, Marker);
    AddFileContentCheck(Result, TEXT("editor orientation MCP inspection"), EditorOrientationPath, TEXT("get_content_browser_state"));
    AddFileContentCheck(Result, TEXT("editor orientation first-session loop"), EditorOrientationPath, TEXT("World Outliner"));
    const FString FirstPlayablePath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-first-playable-loop/SKILL.md"));
    AddFileCheck(Result, TEXT("first playable loop skill"), FirstPlayablePath, Marker);
    AddFileContentCheck(Result, TEXT("first playable MCP sequence"), FirstPlayablePath, TEXT("Required MCP-first sequence"));
    AddFileContentCheck(Result, TEXT("first playable PIE validation"), FirstPlayablePath, TEXT("stopping PIE cleanly"));
    AddFileCheck(Result, TEXT("validation skill"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md")), Marker);
    AddFileCheck(Result, TEXT("C++ UObject lifecycle skill"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/skills/unreal-cpp-uobject-lifecycle-integrity/SKILL.md")), Marker);
    const FString GuardrailsPluginPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
    AddFileCheck(Result, TEXT("guardrails plugin"), GuardrailsPluginPath, Marker);
    AddFileContentCheck(Result, TEXT("binary asset local-tool guardrail"), GuardrailsPluginPath, TEXT("blocked direct .uasset/.umap filesystem access"));
    AddFileContentCheck(Result, TEXT("project state local-tool guardrail"), GuardrailsPluginPath, TEXT("blocked direct Unreal project-state file write"));
    AddFileContentCheck(Result, TEXT("content package local-tool guardrail"), GuardrailsPluginPath, TEXT("blocked direct Unreal content/package mutation"));
    AddFileContentCheck(Result, TEXT("destructive shell local-tool guardrail"), GuardrailsPluginPath, TEXT("blocked destructive local shell command"));
    AddFileContentCheck(Result, TEXT("destructive git local-tool guardrail"), GuardrailsPluginPath, TEXT("containsDestructiveGitCommand"));
    AddFileContentCheck(Result, TEXT("system control scoped mutation guardrail"), GuardrailsPluginPath, TEXT("MCP_ACTION_SCOPED_MUTATION_TOOLS"));
    AddFileContentCheck(Result, TEXT("MCP mutation preflight guardrail"), GuardrailsPluginPath, TEXT("blocked MCP editor mutation before completed preflight"));
    AddFileContentCheck(Result, TEXT("MCP route-card assistant-event guardrail"), GuardrailsPluginPath, TEXT("recordRouteCardFromEvent"));
    AddFileContentCheck(Result, TEXT("MCP route-card preflight guardrail"), GuardrailsPluginPath, TEXT("blocked MCP editor mutation before route-card preflight"));
    AddFileContentCheck(Result, TEXT("session-scoped MCP preflight guardrail"), GuardrailsPluginPath, TEXT("MCP_PREFLIGHT_STATE"));
    AddFileContentCheck(Result, TEXT("successful MCP preflight guardrail"), GuardrailsPluginPath, TEXT("toolOutputSucceeded(output)"));
    AddFileContentCheck(Result, TEXT("stale inspection mutation guardrail"), GuardrailsPluginPath, TEXT("blocked MCP editor mutation after stale inspection"));
    const FString OpenCodeConfigPath = FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/opencode.json"));
    AddFileCheck(Result, TEXT("OpenCode config"), OpenCodeConfigPath);
    AddOpenCodePermissionCheck(Result, OpenCodeConfigPath);
    AddOpenCodeAgentPermissionChecks(Result, FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/agents")));
    AddFileCheck(Result, TEXT("validate command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-validate.md")), Marker);
    AddFileCheck(Result, TEXT("tool inventory command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-tool-inventory.md")), Marker);
    AddFileCheck(Result, TEXT("route card command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-route-card.md")), Marker);
    AddFileCheck(Result, TEXT("PIE/SIE check command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-pie-sie-check.md")), Marker);
    AddFileCheck(Result, TEXT("editor control plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-editor-control-plan.md")), Marker);
    AddFileCheck(Result, TEXT("content plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-content-plan.md")), Marker);
    AddFileCheck(Result, TEXT("Blueprint plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-blueprint-plan.md")), Marker);
    AddFileCheck(Result, TEXT("level and Actor plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-level-actor-plan.md")), Marker);
    AddFileCheck(Result, TEXT("world building plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-world-building-plan.md")), Marker);
    AddFileCheck(Result, TEXT("gameplay and input plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-gameplay-input-plan.md")), Marker);
    AddFileCheck(Result, TEXT("animation and physics plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-animation-physics-plan.md")), Marker);
    AddFileCheck(Result, TEXT("VFX and material plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-vfx-material-plan.md")), Marker);
    AddFileCheck(Result, TEXT("audio plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-audio-plan.md")), Marker);
    AddFileCheck(Result, TEXT("cinematic sequence plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-cinematic-sequence-plan.md")), Marker);
    AddFileCheck(Result, TEXT("networking and GAS plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-networking-gas-plan.md")), Marker);
    AddFileCheck(Result, TEXT("character systems plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-character-systems-plan.md")), Marker);
    AddFileCheck(Result, TEXT("UI and HUD plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-ui-hud-plan.md")), Marker);
    AddFileCheck(Result, TEXT("AI and navigation plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-ai-navigation-plan.md")), Marker);
    AddFileCheck(Result, TEXT("data save accessibility plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-data-save-accessibility-plan.md")), Marker);
    AddFileCheck(Result, TEXT("source control plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-source-control-plan.md")), Marker);
    AddFileCheck(Result, TEXT("performance insights plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-performance-insights-plan.md")), Marker);
    AddFileCheck(Result, TEXT("system and project plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-system-project-plan.md")), Marker);
    AddFileCheck(Result, TEXT("project setup plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-project-setup-plan.md")), Marker);
    AddFileCheck(Result, TEXT("diagnostics crash recovery plan command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-diagnostics-plan.md")), Marker);
    AddFileCheck(Result, TEXT("getting started command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-getting-started.md")), Marker);
    AddFileCheck(Result, TEXT("editor tour command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-editor-tour.md")), Marker);
    AddFileCheck(Result, TEXT("first playable command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-first-playable.md")), Marker);
    AddFileCheck(Result, TEXT("C++ lifecycle command"), FPaths::Combine(NormalizedProjectDirectory, TEXT(".opencode/commands/unreal-cpp-context.md")), Marker);

}
}
