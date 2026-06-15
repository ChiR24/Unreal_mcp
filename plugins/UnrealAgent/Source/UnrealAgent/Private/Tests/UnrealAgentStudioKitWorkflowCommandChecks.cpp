#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitWorkflowCommandChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        auto SkillPath = [&TestDirectory](const TCHAR* Name)
        {
            return FPaths::Combine(TestDirectory, TEXT(".opencode/skills"), Name, TEXT("SKILL.md"));
        };
        auto CommandPath = [&TestDirectory](const TCHAR* Name)
        {
            return FPaths::Combine(TestDirectory, TEXT(".opencode/commands"), Name);
        };

        FString OfficialGettingStartedSkillText;
        const FString OfficialGettingStartedSkillPath = SkillPath(TEXT("unreal-official-getting-started"));
        bPassed &= Test.TestTrue(TEXT("Official getting started skill is readable"), FFileHelper::LoadFileToString(OfficialGettingStartedSkillText, *OfficialGettingStartedSkillPath));
        bPassed &= Test.TestTrue(TEXT("Official getting started skill anchors Epic docs and MCP inspection"), OfficialGettingStartedSkillText.Contains(TEXT("Create your First Project")) && OfficialGettingStartedSkillText.Contains(TEXT("Unreal Editor Interface")) && OfficialGettingStartedSkillText.Contains(TEXT("Content Browser")) && OfficialGettingStartedSkillText.Contains(TEXT("In-Editor Testing")) && OfficialGettingStartedSkillText.Contains(TEXT("get_content_browser_state")));
        bPassed &= Test.TestTrue(TEXT("Official getting started skill separates docs from live state"), OfficialGettingStartedSkillText.Contains(TEXT("documentation as conceptual anchors")) && OfficialGettingStartedSkillText.Contains(TEXT("Do not claim a panel location")) && OfficialGettingStartedSkillText.Contains(TEXT("live editor state is unverified")));
        FString EditorOrientationSkillText;
        const FString EditorOrientationSkillPath = SkillPath(TEXT("unreal-editor-orientation"));
        bPassed &= Test.TestTrue(TEXT("Editor orientation skill is readable"), FFileHelper::LoadFileToString(EditorOrientationSkillText, *EditorOrientationSkillPath));
        bPassed &= Test.TestTrue(TEXT("Editor orientation skill covers inspected editor surfaces"), EditorOrientationSkillText.Contains(TEXT("get_content_browser_state")) && EditorOrientationSkillText.Contains(TEXT("World Outliner")) && EditorOrientationSkillText.Contains(TEXT("Details panel")) && EditorOrientationSkillText.Contains(TEXT("PIE")));
        bPassed &= Test.TestTrue(TEXT("Editor orientation skill requires evidence-backed teaching"), EditorOrientationSkillText.Contains(TEXT("live editor state is unverified")) && EditorOrientationSkillText.Contains(TEXT("MCP route card")));
        FString FirstPlayableSkillText;
        const FString FirstPlayableSkillPath = SkillPath(TEXT("unreal-first-playable-loop"));
        bPassed &= Test.TestTrue(TEXT("First playable skill is readable"), FFileHelper::LoadFileToString(FirstPlayableSkillText, *FirstPlayableSkillPath));
        bPassed &= Test.TestTrue(TEXT("First playable skill covers getting-started editor surfaces"), FirstPlayableSkillText.Contains(TEXT("Project Browser/template")) && FirstPlayableSkillText.Contains(TEXT("viewport")) && FirstPlayableSkillText.Contains(TEXT("World Outliner")) && FirstPlayableSkillText.Contains(TEXT("Details panel")) && FirstPlayableSkillText.Contains(TEXT("Content Browser")));
        bPassed &= Test.TestTrue(TEXT("First playable skill requires MCP-first route and validation"), FirstPlayableSkillText.Contains(TEXT("Required MCP-first sequence")) && FirstPlayableSkillText.Contains(TEXT("MCP route card")) && FirstPlayableSkillText.Contains(TEXT("get_content_browser_state")) && FirstPlayableSkillText.Contains(TEXT("stopping PIE cleanly")));
        bPassed &= Test.TestTrue(TEXT("First playable skill treats PIE/SIE changes as transient"), FirstPlayableSkillText.Contains(TEXT("preview-world changes as transient")) && FirstPlayableSkillText.Contains(TEXT("fresh post-stop inspect/log evidence")));
        bPassed &= Test.TestTrue(TEXT("First playable skill blocks direct binary asset edits"), FirstPlayableSkillText.Contains(TEXT("Never hand-edit .uasset or .umap files")));

        FString ToolInventoryCommandText;
        const FString ToolInventoryCommandPath = CommandPath(TEXT("unreal-tool-inventory.md"));
        bPassed &= Test.TestTrue(TEXT("Tool inventory command is readable"), FFileHelper::LoadFileToString(ToolInventoryCommandText, *ToolInventoryCommandPath));
        bPassed &= Test.TestTrue(TEXT("Tool inventory command separates inventory from inspection"), ToolInventoryCommandText.Contains(TEXT("manage_tools")) && ToolInventoryCommandText.Contains(TEXT("use `inspect`")) && ToolInventoryCommandText.Contains(TEXT("capability gaps")));
        FString RouteCardCommandText;
        const FString RouteCardCommandPath = CommandPath(TEXT("unreal-route-card.md"));
        bPassed &= Test.TestTrue(TEXT("Route card command is readable"), FFileHelper::LoadFileToString(RouteCardCommandText, *RouteCardCommandPath));
        bPassed &= Test.TestTrue(TEXT("Route card command routes through route-card skill"), RouteCardCommandText.Contains(TEXT("unreal-mcp-route-card")) && RouteCardCommandText.Contains(TEXT("parent tool/action")) && RouteCardCommandText.Contains(TEXT("mutation bounds")) && RouteCardCommandText.Contains(TEXT("rollback")));
        FString PieSieCheckCommandText;
        const FString PieSieCheckCommandPath = CommandPath(TEXT("unreal-pie-sie-check.md"));
        bPassed &= Test.TestTrue(TEXT("PIE/SIE check command is readable"), FFileHelper::LoadFileToString(PieSieCheckCommandText, *PieSieCheckCommandPath));
        bPassed &= Test.TestTrue(TEXT("PIE/SIE check command routes through safety skill"), PieSieCheckCommandText.Contains(TEXT("unreal-pie-sie-safety")) && PieSieCheckCommandText.Contains(TEXT("PIE world")) && PieSieCheckCommandText.Contains(TEXT("post-stop inspect")));
        FString EditorControlPlanCommandText;
        const FString EditorControlPlanCommandPath = CommandPath(TEXT("unreal-editor-control-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Editor control plan command is readable"), FFileHelper::LoadFileToString(EditorControlPlanCommandText, *EditorControlPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Editor control plan command routes through editor discipline skill"), EditorControlPlanCommandText.Contains(TEXT("unreal-editor-control-discipline")) && EditorControlPlanCommandText.Contains(TEXT("narrow control_editor route")) && EditorControlPlanCommandText.Contains(TEXT("modal risk")) && EditorControlPlanCommandText.Contains(TEXT("editor-world versus preview-world distinction")));
        FString ContentPlanCommandText;
        const FString ContentPlanCommandPath = CommandPath(TEXT("unreal-content-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Content plan command is readable"), FFileHelper::LoadFileToString(ContentPlanCommandText, *ContentPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Content plan command routes through Content Browser discipline skill"), ContentPlanCommandText.Contains(TEXT("unreal-content-browser-asset-discipline")) && ContentPlanCommandText.Contains(TEXT("get_content_browser_state")) && ContentPlanCommandText.Contains(TEXT("/Game package paths")) && ContentPlanCommandText.Contains(TEXT("redirector risk")));
        FString BlueprintPlanCommandText;
        const FString BlueprintPlanCommandPath = CommandPath(TEXT("unreal-blueprint-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Blueprint plan command is readable"), FFileHelper::LoadFileToString(BlueprintPlanCommandText, *BlueprintPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Blueprint plan command routes through Blueprint discipline skill"), BlueprintPlanCommandText.Contains(TEXT("unreal-blueprint-compile-discipline")) && BlueprintPlanCommandText.Contains(TEXT("class/CDO facts")) && BlueprintPlanCommandText.Contains(TEXT("manage_blueprint route")) && BlueprintPlanCommandText.Contains(TEXT("compile status")));
        FString LevelActorPlanCommandText;
        const FString LevelActorPlanCommandPath = CommandPath(TEXT("unreal-level-actor-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Level and Actor plan command is readable"), FFileHelper::LoadFileToString(LevelActorPlanCommandText, *LevelActorPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Level and Actor plan command routes through world discipline skill"), LevelActorPlanCommandText.Contains(TEXT("unreal-level-actor-discipline")) && LevelActorPlanCommandText.Contains(TEXT("Outliner hierarchy")) && LevelActorPlanCommandText.Contains(TEXT("World Partition ownership")) && LevelActorPlanCommandText.Contains(TEXT("control_actor/manage_level/manage_level_structure/build_environment route")));
        FString WorldBuildingPlanCommandText;
        const FString WorldBuildingPlanCommandPath = CommandPath(TEXT("unreal-world-building-plan.md"));
        bPassed &= Test.TestTrue(TEXT("World building plan command is readable"), FFileHelper::LoadFileToString(WorldBuildingPlanCommandText, *WorldBuildingPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("World building plan command routes through world-building discipline skill"), WorldBuildingPlanCommandText.Contains(TEXT("unreal-world-building-discipline")) && WorldBuildingPlanCommandText.Contains(TEXT("PCG graph/component")) && WorldBuildingPlanCommandText.Contains(TEXT("build_environment/manage_pcg/manage_geometry/manage_level_structure MCP route")) && WorldBuildingPlanCommandText.Contains(TEXT("PIE streaming/collision/navigation/performance validation")));
        FString GameplayInputPlanCommandText;
        const FString GameplayInputPlanCommandPath = CommandPath(TEXT("unreal-gameplay-input-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Gameplay and input plan command is readable"), FFileHelper::LoadFileToString(GameplayInputPlanCommandText, *GameplayInputPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Gameplay and input plan command routes through gameplay discipline skill"), GameplayInputPlanCommandText.Contains(TEXT("unreal-gameplay-input-discipline")) && GameplayInputPlanCommandText.Contains(TEXT("GameMode or World Settings override")) && GameplayInputPlanCommandText.Contains(TEXT("Input Actions")) && GameplayInputPlanCommandText.Contains(TEXT("PIE possession/input/camera/HUD validation")));
        FString AnimationPhysicsPlanCommandText;
        const FString AnimationPhysicsPlanCommandPath = CommandPath(TEXT("unreal-animation-physics-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Animation and physics plan command is readable"), FFileHelper::LoadFileToString(AnimationPhysicsPlanCommandText, *AnimationPhysicsPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Animation and physics plan command routes through motion discipline skill"), AnimationPhysicsPlanCommandText.Contains(TEXT("unreal-animation-physics-discipline")) && AnimationPhysicsPlanCommandText.Contains(TEXT("Skeletal Mesh, Skeleton, Animation Blueprint")) && AnimationPhysicsPlanCommandText.Contains(TEXT("Physics Asset")) && AnimationPhysicsPlanCommandText.Contains(TEXT("PIE animation/notify/collision/physics/root-motion validation")));
        FString VfxMaterialPlanCommandText;
        const FString VfxMaterialPlanCommandPath = CommandPath(TEXT("unreal-vfx-material-plan.md"));
        bPassed &= Test.TestTrue(TEXT("VFX and material plan command is readable"), FFileHelper::LoadFileToString(VfxMaterialPlanCommandText, *VfxMaterialPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("VFX and material plan command routes through visual discipline skill"), VfxMaterialPlanCommandText.Contains(TEXT("unreal-vfx-material-discipline")) && VfxMaterialPlanCommandText.Contains(TEXT("Niagara System")) && VfxMaterialPlanCommandText.Contains(TEXT("Material or Material Instance")) && VfxMaterialPlanCommandText.Contains(TEXT("PIE spawn/parameter/material/bounds/screenshot validation")));
        FString AudioPlanCommandText;
        const FString AudioPlanCommandPath = CommandPath(TEXT("unreal-audio-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Audio plan command is readable"), FFileHelper::LoadFileToString(AudioPlanCommandText, *AudioPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Audio plan command routes through audio discipline skill"), AudioPlanCommandText.Contains(TEXT("unreal-audio-discipline")) && AudioPlanCommandText.Contains(TEXT("Sound Waves")) && AudioPlanCommandText.Contains(TEXT("MetaSound Sources or Presets")) && AudioPlanCommandText.Contains(TEXT("PIE audible playback/spatialization/mix/concurrency validation")));
        FString CinematicSequencePlanCommandText;
        const FString CinematicSequencePlanCommandPath = CommandPath(TEXT("unreal-cinematic-sequence-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Cinematic sequence plan command is readable"), FFileHelper::LoadFileToString(CinematicSequencePlanCommandText, *CinematicSequencePlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Cinematic sequence plan command routes through cinematic discipline skill"), CinematicSequencePlanCommandText.Contains(TEXT("unreal-cinematic-sequence-discipline")) && CinematicSequencePlanCommandText.Contains(TEXT("Level Sequence Asset")) && CinematicSequencePlanCommandText.Contains(TEXT("Camera Cut track")) && CinematicSequencePlanCommandText.Contains(TEXT("render-queue validation")));
        FString NetworkingGasPlanCommandText;
        const FString NetworkingGasPlanCommandPath = CommandPath(TEXT("unreal-networking-gas-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Networking and GAS plan command is readable"), FFileHelper::LoadFileToString(NetworkingGasPlanCommandText, *NetworkingGasPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Networking and GAS plan command routes through multiplayer discipline skill"), NetworkingGasPlanCommandText.Contains(TEXT("unreal-networking-gas-discipline")) && NetworkingGasPlanCommandText.Contains(TEXT("Ability System Component owner/avatar")) && NetworkingGasPlanCommandText.Contains(TEXT("Gameplay Effects")) && NetworkingGasPlanCommandText.Contains(TEXT("multiplayer PIE authority/replication/RPC/GAS prediction validation")));
        FString CharacterSystemsPlanCommandText;
        const FString CharacterSystemsPlanCommandPath = CommandPath(TEXT("unreal-character-systems-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Character systems plan command is readable"), FFileHelper::LoadFileToString(CharacterSystemsPlanCommandText, *CharacterSystemsPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Character systems plan command routes through character discipline skill"), CharacterSystemsPlanCommandText.Contains(TEXT("unreal-character-systems-discipline")) && CharacterSystemsPlanCommandText.Contains(TEXT("Character Movement Component")) && CharacterSystemsPlanCommandText.Contains(TEXT("inventory/equipment data")) && CharacterSystemsPlanCommandText.Contains(TEXT("PIE movement/combat/inventory/interaction/UI validation")));
        FString UiHudPlanCommandText;
        const FString UiHudPlanCommandPath = CommandPath(TEXT("unreal-ui-hud-plan.md"));
        bPassed &= Test.TestTrue(TEXT("UI and HUD plan command is readable"), FFileHelper::LoadFileToString(UiHudPlanCommandText, *UiHudPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("UI and HUD plan command routes through UI discipline skill"), UiHudPlanCommandText.Contains(TEXT("unreal-ui-hud-discipline")) && UiHudPlanCommandText.Contains(TEXT("Widget Blueprint/UserWidget ownership")) && UiHudPlanCommandText.Contains(TEXT("CommonUI activatable widget stack")) && UiHudPlanCommandText.Contains(TEXT("PIE viewport/input/focus/layout validation")));
        FString AiNavigationPlanCommandText;
        const FString AiNavigationPlanCommandPath = CommandPath(TEXT("unreal-ai-navigation-plan.md"));
        bPassed &= Test.TestTrue(TEXT("AI and navigation plan command is readable"), FFileHelper::LoadFileToString(AiNavigationPlanCommandText, *AiNavigationPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("AI and navigation plan command routes through AI discipline skill"), AiNavigationPlanCommandText.Contains(TEXT("unreal-ai-navigation-discipline")) && AiNavigationPlanCommandText.Contains(TEXT("Behavior Tree")) && AiNavigationPlanCommandText.Contains(TEXT("Blackboard")) && AiNavigationPlanCommandText.Contains(TEXT("Nav Mesh Bounds Volume")) && AiNavigationPlanCommandText.Contains(TEXT("PIE AIController/Blackboard/NavMesh validation")));
        FString DataSaveAccessibilityPlanCommandText;
        const FString DataSaveAccessibilityPlanCommandPath = CommandPath(TEXT("unreal-data-save-accessibility-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Data save accessibility plan command is readable"), FFileHelper::LoadFileToString(DataSaveAccessibilityPlanCommandText, *DataSaveAccessibilityPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Data save accessibility plan command routes through data discipline skill"), DataSaveAccessibilityPlanCommandText.Contains(TEXT("unreal-data-save-accessibility-discipline")) && DataSaveAccessibilityPlanCommandText.Contains(TEXT("SaveGame class or slot schema")) && DataSaveAccessibilityPlanCommandText.Contains(TEXT("manage_asset/manage_inventory/manage_blueprint/system_control route")) && DataSaveAccessibilityPlanCommandText.Contains(TEXT("save/load or read-back evidence")));
        FString SourceControlPlanCommandText;
        const FString SourceControlPlanCommandPath = CommandPath(TEXT("unreal-source-control-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Source control plan command is readable"), FFileHelper::LoadFileToString(SourceControlPlanCommandText, *SourceControlPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Source control plan command routes through source control discipline skill"), SourceControlPlanCommandText.Contains(TEXT("unreal-source-control-collaboration-discipline")) && SourceControlPlanCommandText.Contains(TEXT("get_source_control_state")) && SourceControlPlanCommandText.Contains(TEXT("source_control_checkout/source_control_submit route")) && SourceControlPlanCommandText.Contains(TEXT("checked-out-by-other risk")));
        FString PerformanceInsightsPlanCommandText;
        const FString PerformanceInsightsPlanCommandPath = CommandPath(TEXT("unreal-performance-insights-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Performance Insights plan command is readable"), FFileHelper::LoadFileToString(PerformanceInsightsPlanCommandText, *PerformanceInsightsPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Performance Insights plan command routes through performance discipline skill"), PerformanceInsightsPlanCommandText.Contains(TEXT("unreal-performance-insights-discipline")) && PerformanceInsightsPlanCommandText.Contains(TEXT("manage_performance route")) && PerformanceInsightsPlanCommandText.Contains(TEXT("capture_insights_trace/get_trace_status/analyze_trace route")) && PerformanceInsightsPlanCommandText.Contains(TEXT("baseline and after metrics")));
        FString SystemProjectPlanCommandText;
        const FString SystemProjectPlanCommandPath = CommandPath(TEXT("unreal-system-project-plan.md"));
        bPassed &= Test.TestTrue(TEXT("System and project plan command is readable"), FFileHelper::LoadFileToString(SystemProjectPlanCommandText, *SystemProjectPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("System and project plan command routes through system discipline skill"), SystemProjectPlanCommandText.Contains(TEXT("unreal-system-project-discipline")) && SystemProjectPlanCommandText.Contains(TEXT("project settings")) && SystemProjectPlanCommandText.Contains(TEXT("console variables")) && SystemProjectPlanCommandText.Contains(TEXT("narrow system_control route")) && SystemProjectPlanCommandText.Contains(TEXT("exact output/log evidence")));
        FString ProjectSetupPlanCommandText;
        const FString ProjectSetupPlanCommandPath = CommandPath(TEXT("unreal-project-setup-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Project setup plan command is readable"), FFileHelper::LoadFileToString(ProjectSetupPlanCommandText, *ProjectSetupPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Project setup plan command routes through setup discipline skill"), ProjectSetupPlanCommandText.Contains(TEXT("unreal-project-setup-template-discipline")) && ProjectSetupPlanCommandText.Contains(TEXT("system_control get_project_settings read-back")) && ProjectSetupPlanCommandText.Contains(TEXT("manage_level/manage_level_structure map route")) && ProjectSetupPlanCommandText.Contains(TEXT("supported manage_asset package route")));
        FString DiagnosticsPlanCommandText;
        const FString DiagnosticsPlanCommandPath = CommandPath(TEXT("unreal-diagnostics-plan.md"));
        bPassed &= Test.TestTrue(TEXT("Diagnostics crash recovery plan command is readable"), FFileHelper::LoadFileToString(DiagnosticsPlanCommandText, *DiagnosticsPlanCommandPath));
        bPassed &= Test.TestTrue(TEXT("Diagnostics crash recovery plan command routes through diagnostics discipline skill"), DiagnosticsPlanCommandText.Contains(TEXT("unreal-diagnostics-crash-recovery-discipline")) && DiagnosticsPlanCommandText.Contains(TEXT("exact error text")) && DiagnosticsPlanCommandText.Contains(TEXT("run_tests/validate_assets/run_ubt")) && DiagnosticsPlanCommandText.Contains(TEXT("retest path")));
        FString GettingStartedCommandText;
        const FString GettingStartedCommandPath = CommandPath(TEXT("unreal-getting-started.md"));
        bPassed &= Test.TestTrue(TEXT("Getting started command is readable"), FFileHelper::LoadFileToString(GettingStartedCommandText, *GettingStartedCommandPath));
        bPassed &= Test.TestTrue(TEXT("Getting started command uses docs and MCP inspection"), GettingStartedCommandText.Contains(TEXT("unreal-official-getting-started")) && GettingStartedCommandText.Contains(TEXT("Epic")) && GettingStartedCommandText.Contains(TEXT("/unreal-tool-inventory")) && GettingStartedCommandText.Contains(TEXT("inspect current project/editor state")));
        FString EditorTourCommandText;
        const FString EditorTourCommandPath = CommandPath(TEXT("unreal-editor-tour.md"));
        bPassed &= Test.TestTrue(TEXT("Editor tour command is readable"), FFileHelper::LoadFileToString(EditorTourCommandText, *EditorTourCommandPath));
        bPassed &= Test.TestTrue(TEXT("Editor tour command starts from inventory and inspection"), EditorTourCommandText.Contains(TEXT("/unreal-tool-inventory")) && EditorTourCommandText.Contains(TEXT("Content Browser")) && EditorTourCommandText.Contains(TEXT("without guessing")));
        FString FirstPlayableCommandText;
        const FString FirstPlayableCommandPath = CommandPath(TEXT("unreal-first-playable.md"));
        bPassed &= Test.TestTrue(TEXT("First playable command is readable"), FFileHelper::LoadFileToString(FirstPlayableCommandText, *FirstPlayableCommandPath));
        bPassed &= Test.TestTrue(TEXT("First playable command routes through MCP-first workflow"), FirstPlayableCommandText.Contains(TEXT("unreal-first-playable-loop")) && FirstPlayableCommandText.Contains(TEXT("Project Browser/template")) && FirstPlayableCommandText.Contains(TEXT("/unreal-tool-inventory")) && FirstPlayableCommandText.Contains(TEXT("MCP-backed change")) && FirstPlayableCommandText.Contains(TEXT("PIE")));
        FString CppLifecycleSkillText;
        const FString CppLifecycleSkillPath = SkillPath(TEXT("unreal-cpp-uobject-lifecycle-integrity"));
        bPassed &= Test.TestTrue(TEXT("C++ lifecycle skill is readable"), FFileHelper::LoadFileToString(CppLifecycleSkillText, *CppLifecycleSkillPath));
        bPassed &= Test.TestTrue(TEXT("C++ lifecycle skill covers UObject safety"), CppLifecycleSkillText.Contains(TEXT("UPROPERTY")) && CppLifecycleSkillText.Contains(TEXT("CDO-safe")) && CppLifecycleSkillText.Contains(TEXT("game thread")));
        return bPassed;
    }
}

#endif
