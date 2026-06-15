#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitCoreSkillContentChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        const FString ToolPlaybookSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md"));
        const FString RouteCardSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-mcp-route-card/SKILL.md"));
        const FString PieSieSafetySkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-pie-sie-safety/SKILL.md"));
        const FString EditorControlDisciplineSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-editor-control-discipline/SKILL.md"));
        const FString ContentDisciplineSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-content-browser-asset-discipline/SKILL.md"));
        const FString BlueprintDisciplineSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-blueprint-compile-discipline/SKILL.md"));
        const FString LevelActorDisciplineSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-level-actor-discipline/SKILL.md"));
        const FString WorldBuildingDisciplineSkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-world-building-discipline/SKILL.md"));

        FString ToolPlaybookSkillText;
        bPassed &= Test.TestTrue(TEXT("Tool playbook skill is readable"), FFileHelper::LoadFileToString(ToolPlaybookSkillText, *ToolPlaybookSkillPath));
        bPassed &= Test.TestTrue(TEXT("Tool playbook covers Content Browser selection"), ToolPlaybookSkillText.Contains(TEXT("Content Browser")) && ToolPlaybookSkillText.Contains(TEXT("get_content_browser_state")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires MCP tool inventory"), ToolPlaybookSkillText.Contains(TEXT("compact tool inventory")) && ToolPlaybookSkillText.Contains(TEXT("canonical parent tools")) && ToolPlaybookSkillText.Contains(TEXT("missing capabilities")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook rejects direct asset filesystem writes"), ToolPlaybookSkillText.Contains(TEXT("direct filesystem writes")) && ToolPlaybookSkillText.Contains(TEXT(".uasset")) && ToolPlaybookSkillText.Contains(TEXT(".umap")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires MCP route cards"), ToolPlaybookSkillText.Contains(TEXT("MCP route card")) && ToolPlaybookSkillText.Contains(TEXT("tool/action")) && ToolPlaybookSkillText.Contains(TEXT("validation evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook refreshes inspection during mutation batches"), ToolPlaybookSkillText.Contains(TEXT("short batch of protected MCP mutations")) && ToolPlaybookSkillText.Contains(TEXT("dirty packages")) && ToolPlaybookSkillText.Contains(TEXT("selected assets")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook separates PIE/SIE persistence evidence"), ToolPlaybookSkillText.Contains(TEXT("PIE/SIE")) && ToolPlaybookSkillText.Contains(TEXT("preview worlds as transient")) && ToolPlaybookSkillText.Contains(TEXT("post-stop inspect")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires editor control plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-editor-control-plan")) && ToolPlaybookSkillText.Contains(TEXT("control_editor route")) && ToolPlaybookSkillText.Contains(TEXT("dirty-package impact")) && ToolPlaybookSkillText.Contains(TEXT("editor-world versus preview-world evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires Content Browser asset plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-content-plan")) && ToolPlaybookSkillText.Contains(TEXT("dependencies")) && ToolPlaybookSkillText.Contains(TEXT("redirectors")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires Blueprint compile plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-blueprint-plan")) && ToolPlaybookSkillText.Contains(TEXT("compile/save/runtime evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires level and Actor plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-level-actor-plan")) && ToolPlaybookSkillText.Contains(TEXT("Data Layer")) && ToolPlaybookSkillText.Contains(TEXT("World Partition")) && ToolPlaybookSkillText.Contains(TEXT("editor-world ownership")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires gameplay and input plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-gameplay-input-plan")) && ToolPlaybookSkillText.Contains(TEXT("GameMode")) && ToolPlaybookSkillText.Contains(TEXT("Mapping Context")) && ToolPlaybookSkillText.Contains(TEXT("PIE evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires animation and physics plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-animation-physics-plan")) && ToolPlaybookSkillText.Contains(TEXT("Animation Blueprint")) && ToolPlaybookSkillText.Contains(TEXT("Physics Asset")) && ToolPlaybookSkillText.Contains(TEXT("PIE evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires VFX and material plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-vfx-material-plan")) && ToolPlaybookSkillText.Contains(TEXT("Niagara System")) && ToolPlaybookSkillText.Contains(TEXT("Material Instance")) && ToolPlaybookSkillText.Contains(TEXT("screenshot evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires UI and HUD plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-ui-hud-plan")) && ToolPlaybookSkillText.Contains(TEXT("Widget Blueprint")) && ToolPlaybookSkillText.Contains(TEXT("input focus")) && ToolPlaybookSkillText.Contains(TEXT("PIE viewport evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires data save accessibility plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-data-save-accessibility-plan")) && ToolPlaybookSkillText.Contains(TEXT("SaveGame")) && ToolPlaybookSkillText.Contains(TEXT("localized text")) && ToolPlaybookSkillText.Contains(TEXT("accessibility validation")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires source control plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-source-control-plan")) && ToolPlaybookSkillText.Contains(TEXT("get_source_control_state")) && ToolPlaybookSkillText.Contains(TEXT("checkout/submit approval")) && ToolPlaybookSkillText.Contains(TEXT("changelist description")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires performance Insights plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-performance-insights-plan")) && ToolPlaybookSkillText.Contains(TEXT("baseline/after metrics")) && ToolPlaybookSkillText.Contains(TEXT("manage_performance")) && ToolPlaybookSkillText.Contains(TEXT("manage_insights")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires project setup plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-project-setup-plan")) && ToolPlaybookSkillText.Contains(TEXT("get_project_settings read-back")) && ToolPlaybookSkillText.Contains(TEXT("default maps and modes")) && ToolPlaybookSkillText.Contains(TEXT("starter/sample content")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires diagnostics crash recovery plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-diagnostics-plan")) && ToolPlaybookSkillText.Contains(TEXT("exact error text")) && ToolPlaybookSkillText.Contains(TEXT("run_tests/validate_assets/run_ubt")) && ToolPlaybookSkillText.Contains(TEXT("Blueprint compile route")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires AI and navigation plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-ai-navigation-plan")) && ToolPlaybookSkillText.Contains(TEXT("Behavior Tree")) && ToolPlaybookSkillText.Contains(TEXT("NavMesh")) && ToolPlaybookSkillText.Contains(TEXT("PIE evidence")));
        bPassed &= Test.TestTrue(TEXT("Tool playbook requires system and project plan"), ToolPlaybookSkillText.Contains(TEXT("/unreal-system-project-plan")) && ToolPlaybookSkillText.Contains(TEXT("system_control route")) && ToolPlaybookSkillText.Contains(TEXT("exact output/log evidence")) && ToolPlaybookSkillText.Contains(TEXT("do not claim packaging or performance from config-only evidence")));
        FString RouteCardSkillText;
        bPassed &= Test.TestTrue(TEXT("Route card skill is readable"), FFileHelper::LoadFileToString(RouteCardSkillText, *RouteCardSkillPath));
        bPassed &= Test.TestTrue(TEXT("Route card skill has required fields"), RouteCardSkillText.Contains(TEXT("Required fields")) && RouteCardSkillText.Contains(TEXT("Tool route")) && RouteCardSkillText.Contains(TEXT("Mutation bounds")) && RouteCardSkillText.Contains(TEXT("Rollback")));
        bPassed &= Test.TestTrue(TEXT("Route card skill blocks direct binary asset edits"), RouteCardSkillText.Contains(TEXT("Do not use a route card to justify direct .uasset or .umap filesystem edits")));
        FString PieSieSafetySkillText;
        bPassed &= Test.TestTrue(TEXT("PIE/SIE safety skill is readable"), FFileHelper::LoadFileToString(PieSieSafetySkillText, *PieSieSafetySkillPath));
        bPassed &= Test.TestTrue(TEXT("PIE/SIE safety skill separates preview from persisted state"), PieSieSafetySkillText.Contains(TEXT("PIE/SIE facts are runtime/editor-preview evidence")) && PieSieSafetySkillText.Contains(TEXT("not automatic proof")) && PieSieSafetySkillText.Contains(TEXT("persisted")));
        bPassed &= Test.TestTrue(TEXT("PIE/SIE safety skill requires post-stop evidence"), PieSieSafetySkillText.Contains(TEXT("post-stop inspect")) && PieSieSafetySkillText.Contains(TEXT("Keep Simulation Changes")) && PieSieSafetySkillText.Contains(TEXT("runtime/editor preview persistence is unverified")));
        FString EditorControlDisciplineSkillText;
        bPassed &= Test.TestTrue(TEXT("Editor control discipline skill is readable"), FFileHelper::LoadFileToString(EditorControlDisciplineSkillText, *EditorControlDisciplineSkillPath));
        bPassed &= Test.TestTrue(TEXT("Editor control discipline skill inspects editor state"), EditorControlDisciplineSkillText.Contains(TEXT("map/world, viewport, selection")) && EditorControlDisciplineSkillText.Contains(TEXT("active asset/editor tab")) && EditorControlDisciplineSkillText.Contains(TEXT("dirty packages")));
        bPassed &= Test.TestTrue(TEXT("Editor control discipline skill covers modal and transaction risk"), EditorControlDisciplineSkillText.Contains(TEXT("modal dialogs")) && EditorControlDisciplineSkillText.Contains(TEXT("transaction/undo/redo operations")) && EditorControlDisciplineSkillText.Contains(TEXT("Save All")));
        bPassed &= Test.TestTrue(TEXT("Editor control discipline skill separates persistence evidence"), EditorControlDisciplineSkillText.Contains(TEXT("Validate persistence separately")) && EditorControlDisciplineSkillText.Contains(TEXT("screenshots prove captured pixels only")) && EditorControlDisciplineSkillText.Contains(TEXT("saved packages, compile/save output")));
        FString ContentDisciplineSkillText;
        bPassed &= Test.TestTrue(TEXT("Content Browser asset discipline skill is readable"), FFileHelper::LoadFileToString(ContentDisciplineSkillText, *ContentDisciplineSkillPath));
        bPassed &= Test.TestTrue(TEXT("Content Browser asset discipline skill requires inspected package paths"), ContentDisciplineSkillText.Contains(TEXT("get_content_browser_state")) && ContentDisciplineSkillText.Contains(TEXT("/Game package path")) && ContentDisciplineSkillText.Contains(TEXT("source and destination")));
        bPassed &= Test.TestTrue(TEXT("Content Browser asset discipline skill protects references"), ContentDisciplineSkillText.Contains(TEXT("dependency and redirector risk")) && ContentDisciplineSkillText.Contains(TEXT("Developers folder scope")) && ContentDisciplineSkillText.Contains(TEXT("do not edit .uasset or .umap files directly")));
        FString BlueprintDisciplineSkillText;
        bPassed &= Test.TestTrue(TEXT("Blueprint compile discipline skill is readable"), FFileHelper::LoadFileToString(BlueprintDisciplineSkillText, *BlueprintDisciplineSkillPath));
        bPassed &= Test.TestTrue(TEXT("Blueprint compile discipline skill inspects Blueprint context"), BlueprintDisciplineSkillText.Contains(TEXT("Blueprint asset path")) && BlueprintDisciplineSkillText.Contains(TEXT("generated class/CDO facts")) && BlueprintDisciplineSkillText.Contains(TEXT("compile status")));
        bPassed &= Test.TestTrue(TEXT("Blueprint compile discipline skill requires compile save runtime evidence"), BlueprintDisciplineSkillText.Contains(TEXT("compile the Blueprint")) && BlueprintDisciplineSkillText.Contains(TEXT("save the touched asset")) && BlueprintDisciplineSkillText.Contains(TEXT("runtime success from persisted asset save evidence")));
        FString LevelActorDisciplineSkillText;
        bPassed &= Test.TestTrue(TEXT("Level and Actor discipline skill is readable"), FFileHelper::LoadFileToString(LevelActorDisciplineSkillText, *LevelActorDisciplineSkillPath));
        bPassed &= Test.TestTrue(TEXT("Level and Actor discipline skill inspects Outliner and root transform context"), LevelActorDisciplineSkillText.Contains(TEXT("Outliner hierarchy")) && LevelActorDisciplineSkillText.Contains(TEXT("root SceneComponent")) && LevelActorDisciplineSkillText.Contains(TEXT("location/rotation/scale")));
        bPassed &= Test.TestTrue(TEXT("Level and Actor discipline skill covers World Partition ownership"), LevelActorDisciplineSkillText.Contains(TEXT("Data Layer")) && LevelActorDisciplineSkillText.Contains(TEXT("Runtime Grid")) && LevelActorDisciplineSkillText.Contains(TEXT("Is Spatially Loaded")) && LevelActorDisciplineSkillText.Contains(TEXT("One File Per Actor")));
        bPassed &= Test.TestTrue(TEXT("Level and Actor discipline skill requires save viewport and PIE evidence"), LevelActorDisciplineSkillText.Contains(TEXT("save the map or affected external actor package")) && LevelActorDisciplineSkillText.Contains(TEXT("viewport screenshot")) && LevelActorDisciplineSkillText.Contains(TEXT("saved level state, and runtime behavior evidence")));
        FString WorldBuildingDisciplineSkillText;
        bPassed &= Test.TestTrue(TEXT("World building discipline skill is readable"), FFileHelper::LoadFileToString(WorldBuildingDisciplineSkillText, *WorldBuildingDisciplineSkillPath));
        bPassed &= Test.TestTrue(TEXT("World building discipline skill inspects environment ownership"), WorldBuildingDisciplineSkillText.Contains(TEXT("World Partition state")) && WorldBuildingDisciplineSkillText.Contains(TEXT("Landscape actor")) && WorldBuildingDisciplineSkillText.Contains(TEXT("PCG graph/component/nodes/execution state")));
        bPassed &= Test.TestTrue(TEXT("World building discipline skill routes through environment MCP tools"), WorldBuildingDisciplineSkillText.Contains(TEXT("build_environment/manage_pcg/manage_geometry/manage_level_structure")) && WorldBuildingDisciplineSkillText.Contains(TEXT("viewport screenshot")) && WorldBuildingDisciplineSkillText.Contains(TEXT("persisted map/external-actor state")));
        return bPassed;
    }
}

#endif
