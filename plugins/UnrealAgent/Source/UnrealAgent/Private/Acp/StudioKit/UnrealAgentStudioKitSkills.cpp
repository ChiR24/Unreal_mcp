#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    void AppendSkillTemplates(TArray<FStudioKitTemplateFile>& Templates)
    {
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md"), MakeToolPlaybookSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-mcp-route-card/SKILL.md"), MakeMcpRouteCardSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-pie-sie-safety/SKILL.md"), MakePieSieSafetySkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-editor-control-discipline/SKILL.md"), MakeEditorControlDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-content-browser-asset-discipline/SKILL.md"), MakeContentBrowserAssetDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-blueprint-compile-discipline/SKILL.md"), MakeBlueprintCompileDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-level-actor-discipline/SKILL.md"), MakeLevelActorDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-world-building-discipline/SKILL.md"), MakeWorldBuildingDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-gameplay-input-discipline/SKILL.md"), MakeGameplayInputDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-animation-physics-discipline/SKILL.md"), MakeAnimationPhysicsDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-vfx-material-discipline/SKILL.md"), MakeVfxMaterialDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-audio-discipline/SKILL.md"), MakeAudioDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-cinematic-sequence-discipline/SKILL.md"), MakeCinematicSequenceDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-networking-gas-discipline/SKILL.md"), MakeNetworkingGasDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-character-systems-discipline/SKILL.md"), MakeCharacterSystemsDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-ui-hud-discipline/SKILL.md"), MakeUiHudDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-ai-navigation-discipline/SKILL.md"), MakeAiNavigationDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-data-save-accessibility-discipline/SKILL.md"), MakeDataSaveAccessibilityDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-source-control-collaboration-discipline/SKILL.md"), MakeSourceControlCollaborationDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-performance-insights-discipline/SKILL.md"), MakePerformanceInsightsDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-system-project-discipline/SKILL.md"), MakeSystemProjectDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-project-bootstrap/SKILL.md"), MakeBootstrapSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-project-setup-template-discipline/SKILL.md"), MakeProjectSetupTemplateDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-diagnostics-crash-recovery-discipline/SKILL.md"), MakeDiagnosticsCrashRecoveryDisciplineSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-official-getting-started/SKILL.md"), MakeOfficialGettingStartedSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-editor-orientation/SKILL.md"), MakeEditorOrientationSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-first-playable-loop/SKILL.md"), MakeFirstPlayableLoopSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-prototype/SKILL.md"), MakePrototypeSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"), MakeValidationSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-release-readiness/SKILL.md"), MakeReleaseSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-debug-fix/SKILL.md"), MakeDebugFixSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-cpp-uobject-lifecycle-integrity/SKILL.md"), MakeCppUObjectLifecycleSkill());
    }
}
