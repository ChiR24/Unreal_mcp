#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    struct FGeneratedArtifactExpectation
    {
        const TCHAR* Label;
        const TCHAR* RelativePath;
    };

    bool RunStudioKitGeneratedArtifactChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        const FUnrealAgentStudioKitResult KitResult =
            FUnrealAgentStudioKit::EnsureForProject(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Studio Kit generation succeeds"), KitResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Studio Kit writes multiple OpenCode files"), KitResult.FilesWritten >= 10);

        FString PrimaryAgent;
        const FString PrimaryAgentPath =
            FPaths::Combine(TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(TEXT("Primary Unreal Agent file exists"), FPaths::FileExists(PrimaryAgentPath) && FFileHelper::LoadFileToString(PrimaryAgent, *PrimaryAgentPath));
        bPassed &= Test.TestTrue(TEXT("Primary agent has prompt marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetPromptVersionMarker()));
        bPassed &= Test.TestTrue(TEXT("Primary agent has Studio Kit marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes specialist roles"), PrimaryAgent.Contains(TEXT("unreal-technical-director")) && PrimaryAgent.Contains(TEXT("unreal-qa-release")));
        bPassed &= Test.TestTrue(TEXT("Primary agent keeps local and exact Unreal MCP permissions one-shot"), PrimaryAgent.Contains(TEXT("\"*\": ask")) && PrimaryAgent.Contains(TEXT("read: ask")) && PrimaryAgent.Contains(TEXT("unreal-engine_manage_tools: ask")) && PrimaryAgent.Contains(TEXT("unreal-engine_inspect: ask")) && !PrimaryAgent.Contains(TEXT("unreal-engine_*: allow")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes MCP tool playbook"), PrimaryAgent.Contains(TEXT("MCP tool playbook")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes release workflow"), PrimaryAgent.Contains(TEXT("Shipping: confirm packaging readiness")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes C++ lifecycle discipline"), PrimaryAgent.Contains(TEXT("unreal-cpp-uobject-lifecycle-integrity")) && PrimaryAgent.Contains(TEXT("/unreal-cpp-context")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes Content Browser inspection"), PrimaryAgent.Contains(TEXT("get_content_browser_state")));
        bPassed &= Test.TestTrue(TEXT("Primary agent requires MCP tool inventory"), PrimaryAgent.Contains(TEXT("/unreal-tool-inventory")) && PrimaryAgent.Contains(TEXT("planned domain mapping")));
        bPassed &= Test.TestTrue(TEXT("Primary agent prefers MCP asset mutation"), PrimaryAgent.Contains(TEXT("Prefer unreal-engine MCP tools")) && PrimaryAgent.Contains(TEXT("Do not directly edit .uasset")));
        bPassed &= Test.TestTrue(TEXT("Primary agent documents blocked local asset mutations"), PrimaryAgent.Contains(TEXT("Direct .uasset/.umap filesystem permission requests")) && PrimaryAgent.Contains(TEXT("Content/, /Game, or /Engine mutation aliases are blocked")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes MCP route cards"), PrimaryAgent.Contains(TEXT("MCP route card")) && PrimaryAgent.Contains(TEXT("/unreal-route-card")) && PrimaryAgent.Contains(TEXT("rollback or stop condition")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes PIE/SIE persistence discipline"), PrimaryAgent.Contains(TEXT("/unreal-pie-sie-check")) && PrimaryAgent.Contains(TEXT("preview-world evidence")) && PrimaryAgent.Contains(TEXT("persisted editor assets/maps")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes editor control planning"), PrimaryAgent.Contains(TEXT("/unreal-editor-control-plan")) && PrimaryAgent.Contains(TEXT("viewport and selection")) && PrimaryAgent.Contains(TEXT("modal and overwrite risk")) && PrimaryAgent.Contains(TEXT("current-state versus persistence evidence")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes Content Browser asset planning"), PrimaryAgent.Contains(TEXT("/unreal-content-plan")) && PrimaryAgent.Contains(TEXT("/Game package paths")) && PrimaryAgent.Contains(TEXT("dependency and redirector risk")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes Blueprint compile planning"), PrimaryAgent.Contains(TEXT("/unreal-blueprint-plan")) && PrimaryAgent.Contains(TEXT("Blueprint path/type")) && PrimaryAgent.Contains(TEXT("compile/save validation")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes level and Actor planning"), PrimaryAgent.Contains(TEXT("/unreal-level-actor-plan")) && PrimaryAgent.Contains(TEXT("root component")) && PrimaryAgent.Contains(TEXT("World Partition ownership")) && PrimaryAgent.Contains(TEXT("save/viewport/PIE validation")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes world building planning"), PrimaryAgent.Contains(TEXT("/unreal-world-building-plan")) && PrimaryAgent.Contains(TEXT("landscape/material/layer state")) && PrimaryAgent.Contains(TEXT("build_environment/manage_pcg/manage_geometry/manage_level_structure")) && PrimaryAgent.Contains(TEXT("streaming/performance expectations")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes gameplay and input planning"), PrimaryAgent.Contains(TEXT("/unreal-gameplay-input-plan")) && PrimaryAgent.Contains(TEXT("GameMode/Pawn/Controller/HUD ownership")) && PrimaryAgent.Contains(TEXT("Enhanced Input actions and mapping contexts")) && PrimaryAgent.Contains(TEXT("possession/camera/UI expectations")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes animation and physics planning"), PrimaryAgent.Contains(TEXT("/unreal-animation-physics-plan")) && PrimaryAgent.Contains(TEXT("Skeleton/Skeletal Mesh/Animation Blueprint ownership")) && PrimaryAgent.Contains(TEXT("Physics Asset/collision/constraint evidence")) && PrimaryAgent.Contains(TEXT("Blend Space/Montage/Notify expectations")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes VFX and material planning"), PrimaryAgent.Contains(TEXT("/unreal-vfx-material-plan")) && PrimaryAgent.Contains(TEXT("Niagara System/emitter/user-parameter ownership")) && PrimaryAgent.Contains(TEXT("Material/Material Instance/texture assignment")) && PrimaryAgent.Contains(TEXT("shader/bounds/culling evidence")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes audio planning"), PrimaryAgent.Contains(TEXT("/unreal-audio-plan")) && PrimaryAgent.Contains(TEXT("Sound Wave/Sound Cue/MetaSound/Audio Component ownership")) && PrimaryAgent.Contains(TEXT("attenuation/spatialization evidence")) && PrimaryAgent.Contains(TEXT("Submix/Sound Class routing")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes cinematic sequence planning"), PrimaryAgent.Contains(TEXT("/unreal-cinematic-sequence-plan")) && PrimaryAgent.Contains(TEXT("Level Sequence Asset/Actor ownership")) && PrimaryAgent.Contains(TEXT("Camera Cut/CineCameraActor expectations")) && PrimaryAgent.Contains(TEXT("Movie Render Queue config")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes networking and GAS planning"), PrimaryAgent.Contains(TEXT("/unreal-networking-gas-plan")) && PrimaryAgent.Contains(TEXT("Ability System Component owner/avatar")) && PrimaryAgent.Contains(TEXT("Ability/Attribute/Effect/Tag/Cue contract")) && PrimaryAgent.Contains(TEXT("multiplayer PIE validation")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes character systems planning"), PrimaryAgent.Contains(TEXT("/unreal-character-systems-plan")) && PrimaryAgent.Contains(TEXT("Character/Pawn/Controller ownership")) && PrimaryAgent.Contains(TEXT("damage/combat contract")) && PrimaryAgent.Contains(TEXT("interaction trace/overlap path")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes UI and HUD planning"), PrimaryAgent.Contains(TEXT("/unreal-ui-hud-plan")) && PrimaryAgent.Contains(TEXT("Widget Blueprint/UserWidget ownership")) && PrimaryAgent.Contains(TEXT("CommonUI/input focus expectations")) && PrimaryAgent.Contains(TEXT("DPI layout evidence")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes data save accessibility planning"), PrimaryAgent.Contains(TEXT("/unreal-data-save-accessibility-plan")) && PrimaryAgent.Contains(TEXT("SaveGame class or slot schema")) && PrimaryAgent.Contains(TEXT("localization namespace/key/table")) && PrimaryAgent.Contains(TEXT("save/load or read-back validation")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes source control collaboration planning"), PrimaryAgent.Contains(TEXT("/unreal-source-control-plan")) && PrimaryAgent.Contains(TEXT("get_source_control_state read-back")) && PrimaryAgent.Contains(TEXT("source_control_checkout/source_control_submit")) && PrimaryAgent.Contains(TEXT("checked-out-by-other/conflict/out-of-date risk")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes performance and Insights planning"), PrimaryAgent.Contains(TEXT("/unreal-performance-insights-plan")) && PrimaryAgent.Contains(TEXT("baseline metric")) && PrimaryAgent.Contains(TEXT("manage_performance action")) && PrimaryAgent.Contains(TEXT("manage_insights trace action")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes AI and navigation planning"), PrimaryAgent.Contains(TEXT("/unreal-ai-navigation-plan")) && PrimaryAgent.Contains(TEXT("Behavior Tree/Blackboard ownership")) && PrimaryAgent.Contains(TEXT("NavMesh pathing evidence")) && PrimaryAgent.Contains(TEXT("Perception/EQS expectations")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes project setup and template planning"), PrimaryAgent.Contains(TEXT("/unreal-project-setup-plan")) && PrimaryAgent.Contains(TEXT("get_project_settings read-back")) && PrimaryAgent.Contains(TEXT("starter/sample or migrated content")) && PrimaryAgent.Contains(TEXT("system_control set_project_setting action")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes diagnostics crash recovery planning"), PrimaryAgent.Contains(TEXT("/unreal-diagnostics-plan")) && PrimaryAgent.Contains(TEXT("exact error text")) && PrimaryAgent.Contains(TEXT("run_tests/validate_assets/run_ubt")) && PrimaryAgent.Contains(TEXT("original-failure retest evidence")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes system and project planning"), PrimaryAgent.Contains(TEXT("/unreal-system-project-plan")) && PrimaryAgent.Contains(TEXT("console variable or command target")) && PrimaryAgent.Contains(TEXT("package/cook/build/deploy target")) && PrimaryAgent.Contains(TEXT("do not claim packaging or performance from config-only evidence")));
        bPassed &= Test.TestTrue(TEXT("Primary agent refreshes inspection during mutation batches"), PrimaryAgent.Contains(TEXT("short batch of protected MCP mutations")) && PrimaryAgent.Contains(TEXT("refresh inspect")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes official getting started workflow"), PrimaryAgent.Contains(TEXT("unreal-official-getting-started")) && PrimaryAgent.Contains(TEXT("/unreal-getting-started")) && PrimaryAgent.Contains(TEXT("official Epic onboarding anchors")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes editor orientation workflow"), PrimaryAgent.Contains(TEXT("unreal-editor-orientation")) && PrimaryAgent.Contains(TEXT("/unreal-editor-tour")));
        bPassed &= Test.TestTrue(TEXT("Primary agent includes first playable workflow"), PrimaryAgent.Contains(TEXT("unreal-first-playable-loop")) && PrimaryAgent.Contains(TEXT("/unreal-first-playable")));

        const FGeneratedArtifactExpectation Expectations[] = {
            { TEXT("Validation skill is generated"), TEXT(".opencode/skills/unreal-validation-loop/SKILL.md") },
            { TEXT("Tool playbook skill is generated"), TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md") },
            { TEXT("Route card skill is generated"), TEXT(".opencode/skills/unreal-mcp-route-card/SKILL.md") },
            { TEXT("PIE/SIE safety skill is generated"), TEXT(".opencode/skills/unreal-pie-sie-safety/SKILL.md") },
            { TEXT("Editor control discipline skill is generated"), TEXT(".opencode/skills/unreal-editor-control-discipline/SKILL.md") },
            { TEXT("Content Browser asset discipline skill is generated"), TEXT(".opencode/skills/unreal-content-browser-asset-discipline/SKILL.md") },
            { TEXT("Blueprint compile discipline skill is generated"), TEXT(".opencode/skills/unreal-blueprint-compile-discipline/SKILL.md") },
            { TEXT("Level and Actor discipline skill is generated"), TEXT(".opencode/skills/unreal-level-actor-discipline/SKILL.md") },
            { TEXT("World building discipline skill is generated"), TEXT(".opencode/skills/unreal-world-building-discipline/SKILL.md") },
            { TEXT("Gameplay and input discipline skill is generated"), TEXT(".opencode/skills/unreal-gameplay-input-discipline/SKILL.md") },
            { TEXT("Animation and physics discipline skill is generated"), TEXT(".opencode/skills/unreal-animation-physics-discipline/SKILL.md") },
            { TEXT("VFX and material discipline skill is generated"), TEXT(".opencode/skills/unreal-vfx-material-discipline/SKILL.md") },
            { TEXT("Audio discipline skill is generated"), TEXT(".opencode/skills/unreal-audio-discipline/SKILL.md") },
            { TEXT("Cinematic sequence discipline skill is generated"), TEXT(".opencode/skills/unreal-cinematic-sequence-discipline/SKILL.md") },
            { TEXT("Networking and GAS discipline skill is generated"), TEXT(".opencode/skills/unreal-networking-gas-discipline/SKILL.md") },
            { TEXT("Character systems discipline skill is generated"), TEXT(".opencode/skills/unreal-character-systems-discipline/SKILL.md") },
            { TEXT("UI and HUD discipline skill is generated"), TEXT(".opencode/skills/unreal-ui-hud-discipline/SKILL.md") },
            { TEXT("AI and navigation discipline skill is generated"), TEXT(".opencode/skills/unreal-ai-navigation-discipline/SKILL.md") },
            { TEXT("Data save accessibility discipline skill is generated"), TEXT(".opencode/skills/unreal-data-save-accessibility-discipline/SKILL.md") },
            { TEXT("Source control collaboration discipline skill is generated"), TEXT(".opencode/skills/unreal-source-control-collaboration-discipline/SKILL.md") },
            { TEXT("Performance Insights discipline skill is generated"), TEXT(".opencode/skills/unreal-performance-insights-discipline/SKILL.md") },
            { TEXT("System and project discipline skill is generated"), TEXT(".opencode/skills/unreal-system-project-discipline/SKILL.md") },
            { TEXT("Project setup template discipline skill is generated"), TEXT(".opencode/skills/unreal-project-setup-template-discipline/SKILL.md") },
            { TEXT("Diagnostics crash recovery discipline skill is generated"), TEXT(".opencode/skills/unreal-diagnostics-crash-recovery-discipline/SKILL.md") },
            { TEXT("Official getting started skill is generated"), TEXT(".opencode/skills/unreal-official-getting-started/SKILL.md") },
            { TEXT("Editor orientation skill is generated"), TEXT(".opencode/skills/unreal-editor-orientation/SKILL.md") },
            { TEXT("First playable loop skill is generated"), TEXT(".opencode/skills/unreal-first-playable-loop/SKILL.md") },
            { TEXT("C++ UObject lifecycle skill is generated"), TEXT(".opencode/skills/unreal-cpp-uobject-lifecycle-integrity/SKILL.md") },
            { TEXT("Guardrails plugin is generated"), TEXT(".opencode/plugins/unreal-agent-guardrails.ts") },
            { TEXT("Ship check command is generated"), TEXT(".opencode/commands/unreal-ship-check.md") },
            { TEXT("Inspect command is generated"), TEXT(".opencode/commands/unreal-inspect.md") },
            { TEXT("Tool inventory command is generated"), TEXT(".opencode/commands/unreal-tool-inventory.md") },
            { TEXT("Route card command is generated"), TEXT(".opencode/commands/unreal-route-card.md") },
            { TEXT("PIE/SIE check command is generated"), TEXT(".opencode/commands/unreal-pie-sie-check.md") },
            { TEXT("Editor control plan command is generated"), TEXT(".opencode/commands/unreal-editor-control-plan.md") },
            { TEXT("Content plan command is generated"), TEXT(".opencode/commands/unreal-content-plan.md") },
            { TEXT("Blueprint plan command is generated"), TEXT(".opencode/commands/unreal-blueprint-plan.md") },
            { TEXT("Level and Actor plan command is generated"), TEXT(".opencode/commands/unreal-level-actor-plan.md") },
            { TEXT("World building plan command is generated"), TEXT(".opencode/commands/unreal-world-building-plan.md") },
            { TEXT("Gameplay and input plan command is generated"), TEXT(".opencode/commands/unreal-gameplay-input-plan.md") },
            { TEXT("Animation and physics plan command is generated"), TEXT(".opencode/commands/unreal-animation-physics-plan.md") },
            { TEXT("VFX and material plan command is generated"), TEXT(".opencode/commands/unreal-vfx-material-plan.md") },
            { TEXT("Audio plan command is generated"), TEXT(".opencode/commands/unreal-audio-plan.md") },
            { TEXT("Cinematic sequence plan command is generated"), TEXT(".opencode/commands/unreal-cinematic-sequence-plan.md") },
            { TEXT("Networking and GAS plan command is generated"), TEXT(".opencode/commands/unreal-networking-gas-plan.md") },
            { TEXT("Character systems plan command is generated"), TEXT(".opencode/commands/unreal-character-systems-plan.md") },
            { TEXT("UI and HUD plan command is generated"), TEXT(".opencode/commands/unreal-ui-hud-plan.md") },
            { TEXT("AI and navigation plan command is generated"), TEXT(".opencode/commands/unreal-ai-navigation-plan.md") },
            { TEXT("Data save accessibility plan command is generated"), TEXT(".opencode/commands/unreal-data-save-accessibility-plan.md") },
            { TEXT("Source control plan command is generated"), TEXT(".opencode/commands/unreal-source-control-plan.md") },
            { TEXT("Performance Insights plan command is generated"), TEXT(".opencode/commands/unreal-performance-insights-plan.md") },
            { TEXT("System and project plan command is generated"), TEXT(".opencode/commands/unreal-system-project-plan.md") },
            { TEXT("Project setup plan command is generated"), TEXT(".opencode/commands/unreal-project-setup-plan.md") },
            { TEXT("Diagnostics crash recovery plan command is generated"), TEXT(".opencode/commands/unreal-diagnostics-plan.md") },
            { TEXT("Getting started command is generated"), TEXT(".opencode/commands/unreal-getting-started.md") },
            { TEXT("Editor tour command is generated"), TEXT(".opencode/commands/unreal-editor-tour.md") },
            { TEXT("First playable command is generated"), TEXT(".opencode/commands/unreal-first-playable.md") },
            { TEXT("C++ lifecycle command is generated"), TEXT(".opencode/commands/unreal-cpp-context.md") },
            { TEXT("OpenCode config is generated"), TEXT(".opencode/opencode.json") }
        };
        for (const FGeneratedArtifactExpectation& Expectation : Expectations)
        {
            bPassed &= Test.TestTrue(
                Expectation.Label,
                FPaths::FileExists(FPaths::Combine(TestDirectory, Expectation.RelativePath)));
        }
        return bPassed;
    }
}

#endif
