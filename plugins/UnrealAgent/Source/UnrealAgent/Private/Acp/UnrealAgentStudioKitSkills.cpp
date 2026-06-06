#include "UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    FString MakeSkillMarkdown(const FString& Name, const FString& Description, const FString& Body)
    {
        return FString()
            + TEXT("---\n")
            + FString::Printf(TEXT("name: %s\n"), *Name)
            + FString::Printf(TEXT("description: %s\n"), *Description)
            + FString::Printf(TEXT("%s\n"), StudioKitVersionMarker)
            + TEXT("---\n")
            + Body
            + TEXT("\n");
    }

    FString MakeToolPlaybookSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-mcp-tool-playbook"),
            TEXT("Choose and sequence Unreal MCP tools for editor-backed game production tasks."),
            TEXT("# Unreal MCP Tool Playbook\n\nUse this when a request needs live Unreal work. First confirm the `unreal-engine` MCP server is connected. Use `manage_tools` to discover available canonical tools and `inspect` to establish current project, world, selection, viewport, Blueprint, CDO, runtime, and log facts. Then choose the narrowest domain tool: assets and Blueprints for content, actor/level/environment tools for world changes, character/combat/AI/GAS/networking/inventory/interaction for gameplay systems, audio/VFX/sequence/geometry for presentation, and `system_control` for tests, logs, profiling, screenshots, and build checks.\n\nNever claim live editor state without MCP evidence. If a tool is missing, name the missing capability and continue from files, config, docs, or logs."));
    }

    FString MakeBootstrapSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-project-bootstrap"),
            TEXT("Turn an empty Unreal project or vague game concept into the first playable production plan."),
            TEXT("# Unreal Project Bootstrap\n\nCapture genre, player fantasy, core loop, target platform, input scheme, art/audio constraints, multiplayer expectations, success criteria, and non-goals. Propose the smallest playable prototype first. Map responsibilities across GameMode, GameState, Pawn, Controller, HUD/UI, GameInstance, subsystems, data assets, save/load, and content folders. Keep the first plan shippable in increments: prototype, vertical slice, production content, polish, release."));
    }

    FString MakePrototypeSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-prototype"),
            TEXT("Create the smallest playable Unreal loop before expanding into a full game."),
            TEXT("# Unreal Prototype Loop\n\nImplement one controllable player loop, one win/lose or progress signal, one feedback channel, and one validation route. Prefer reusable C++/Blueprint boundaries and clear asset names. After each change, compile or inspect the touched assets, run PIE when possible, and capture evidence before expanding scope."));
    }

    FString MakeValidationSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-validation-loop"),
            TEXT("Validate Unreal Agent changes with editor, MCP, logs, screenshots, tests, and evidence ledger entries."),
            TEXT("# Unreal Validation Loop\n\nBefore approval, state what must be true. Validate with the strongest available evidence: MCP `inspect`, Blueprint compile, PIE/run checks, automation tests, screenshots for visual work, log scans, profiling, build/package output, and source-control diff review. Save concise evidence and residual risks. If validation cannot run, say exactly why and what remains unproven."));
    }

    FString MakeReleaseSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-release-readiness"),
            TEXT("Check whether an Unreal project or feature is ready to package, ship, or hand off."),
            TEXT("# Unreal Release Readiness\n\nCheck packaging settings, target platforms, maps, inputs, scalability, save compatibility, crash/log risk, localization/accessibility, performance budgets, multiplayer assumptions, content redirects, test coverage, documentation, changelog, known issues, and source-control cleanliness. Separate blockers from warnings and record the evidence used."));
    }

    FString MakeDebugFixSkill()
    {
        return MakeSkillMarkdown(
            TEXT("unreal-debug-fix"),
            TEXT("Diagnose Unreal editor, asset, gameplay, build, or runtime failures using a baseline-fix-retest loop."),
            TEXT("# Unreal Debug Fix\n\nReproduce or collect the baseline first: exact error text, logs, asset path, map, platform, and recent changes. Form one narrow hypothesis, make the smallest fix, then retest the original failure. Preserve user changes and avoid broad cleanup unless it is required for the verified fix."));
    }

    FString MakeCommandMarkdown(const FString& Description, const FString& Body)
    {
        return FString()
            + TEXT("---\n")
            + FString::Printf(TEXT("description: %s\n"), *Description)
            + FString::Printf(TEXT("%s\n"), StudioKitVersionMarker)
            + TEXT("agent: unreal-agent\n")
            + TEXT("---\n")
            + Body
            + TEXT("\n");
    }

    void AppendSkillTemplates(TArray<FStudioKitTemplateFile>& Templates)
    {
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md"), MakeToolPlaybookSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-project-bootstrap/SKILL.md"), MakeBootstrapSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-prototype/SKILL.md"), MakePrototypeSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"), MakeValidationSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-release-readiness/SKILL.md"), MakeReleaseSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-debug-fix/SKILL.md"), MakeDebugFixSkill());
    }

    void AppendCommandTemplates(TArray<FStudioKitTemplateFile>& Templates)
    {
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-start.md"), MakeCommandMarkdown(TEXT("Start or reset the Unreal project production plan."), TEXT("Use the Unreal project bootstrap skill. Inspect available context, identify the current production stage, and propose the smallest playable next milestone with acceptance criteria.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-inspect.md"), MakeCommandMarkdown(TEXT("Inspect current Unreal project, editor, map, selection, and risks."), TEXT("Use the MCP tool playbook. Discover available `unreal-engine` MCP tools, inspect the project/editor state, summarize what is known, and list anything still unverified.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-prototype.md"), MakeCommandMarkdown(TEXT("Build the smallest playable Unreal prototype loop."), TEXT("Use the Unreal prototype skill. Create or plan the minimum playable loop, then validate with editor/MCP evidence before expanding scope.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-validate.md"), MakeCommandMarkdown(TEXT("Run a focused Unreal validation pass."), TEXT("Use the Unreal validation loop skill. State acceptance criteria, run the strongest available checks, record evidence, and report blockers separately from warnings.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-ship-check.md"), MakeCommandMarkdown(TEXT("Check release readiness for an Unreal feature or game."), TEXT("Use the Unreal release readiness skill. Review packaging, platform settings, maps, inputs, logs, performance, accessibility/localization, known issues, source-control state, and release blockers.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-fix-errors.md"), MakeCommandMarkdown(TEXT("Diagnose and fix Unreal errors with baseline-fix-retest discipline."), TEXT("Use the Unreal debug fix skill. Preserve exact errors, inspect likely causes, make the smallest fix, and retest the original failure path.")));
    }
}
