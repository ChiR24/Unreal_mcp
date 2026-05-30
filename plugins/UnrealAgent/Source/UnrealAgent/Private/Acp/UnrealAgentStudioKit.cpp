#include "UnrealAgentStudioKit.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    constexpr const TCHAR* StudioKitVersionMarker = TEXT("unreal_agent_studio_kit_version: 1");
    constexpr const TCHAR* PromptVersionMarker = TEXT("unreal_agent_prompt_version: 2");

    struct FStudioKitTemplateFile
    {
        FString RelativePath;
        FString Content;
        bool bOverwriteLegacyPrompt = false;
    };

    FString NormalizeStudioKitProjectDirectory(const FString& ProjectDirectory)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(ProjectDirectory.IsEmpty() ? FPaths::ProjectDir() : ProjectDirectory);
        FPaths::NormalizeDirectoryName(Normalized);
        return Normalized;
    }

    bool EnsureParentDirectory(const FString& Path)
    {
        const FString ParentDirectory = FPaths::GetPath(Path);
        return IFileManager::Get().MakeDirectory(*ParentDirectory, true);
    }

    bool LooksLikeLegacyManagedPrompt(const FString& ExistingText)
    {
        return ExistingText.Contains(TEXT("description: Unreal Editor specialist with live MCP control"))
            || ExistingText.Contains(TEXT("description: Unreal Editor game production director with live MCP control"))
            || ExistingText.Contains(TEXT("Use the connected unreal-engine MCP tools"))
            || ExistingText.Contains(PromptVersionMarker);
    }

    bool IsSensitiveLine(const FString& Line)
    {
        const FString LowerLine = Line.ToLower();
        return LowerLine.Contains(TEXT("x-mcp-capability-token"))
            || LowerLine.Contains(TEXT("capabilitytoken"))
            || LowerLine.Contains(TEXT("capability token"))
            || LowerLine.Contains(TEXT("authorization:"))
            || LowerLine.Contains(TEXT("bearer "))
            || LowerLine.Contains(TEXT("api_key"))
            || LowerLine.Contains(TEXT("api-key"))
            || LowerLine.Contains(TEXT("apikey"))
            || LowerLine.Contains(TEXT("access_token"))
            || LowerLine.Contains(TEXT("refresh_token"))
            || LowerLine.Contains(TEXT("password"))
            || LowerLine.Contains(TEXT("secret"));
    }

    FString RedactLine(const FString& Line)
    {
        int32 SeparatorIndex = INDEX_NONE;
        if (Line.FindChar(TEXT(':'), SeparatorIndex) || Line.FindChar(TEXT('='), SeparatorIndex))
        {
            return Line.Left(SeparatorIndex + 1) + TEXT(" [REDACTED]");
        }
        return TEXT("[REDACTED]");
    }

    FString MakeFrontMatter(const FString& Description, const FString& Mode)
    {
        return FString()
            + TEXT("---\n")
            + FString::Printf(TEXT("description: %s\n"), *Description)
            + FString::Printf(TEXT("mode: %s\n"), *Mode)
            + FString::Printf(TEXT("%s\n"), StudioKitVersionMarker)
            + TEXT("permission:\n")
            + TEXT("  read: allow\n")
            + TEXT("  glob: allow\n")
            + TEXT("  grep: allow\n")
            + TEXT("  list: allow\n")
            + TEXT("  edit: ask\n")
            + TEXT("  bash: ask\n")
            + TEXT("  skill:\n")
            + TEXT("    unreal-*: allow\n")
            + TEXT("  task:\n")
            + TEXT("    unreal-*: ask\n")
            + TEXT("  unreal-engine_*: allow\n")
            + TEXT("---\n");
    }

    FString MakeSpecialistAgentMarkdown(const FString& Title, const FString& Description, const FString& Body)
    {
        return MakeFrontMatter(Description, TEXT("subagent"))
            + FString::Printf(TEXT("You are %s for Unreal Agent.\n\n"), *Title)
            + Body
            + TEXT("\n\nGround every claim in project files, editor context, MCP output, or explicit user input. If live MCP tools are missing, say what is unavailable and continue from inspectable files and logs.\n");
    }

    FString MakeTechnicalDirectorAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal technical director"),
            TEXT("Architecture, production risk, engine constraints, and release gates for Unreal projects"),
            TEXT("Own architecture reviews, production decomposition, module boundaries, C++ versus Blueprint responsibilities, data assets, save/load, platform settings, performance budgets, and release blockers. Create staged plans that move from prototype to vertical slice to production to release readiness."));
    }

    FString MakeGameplayProgrammerAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal gameplay programmer"),
            TEXT("Gameplay systems, input, actors, components, replication, and playable loops"),
            TEXT("Build and review player loops, pawn/controller ownership, interaction, inventory, combat, abilities, AI integration, and multiplayer constraints. Prefer small playable increments with clear acceptance tests and rollback points."));
    }

    FString MakeBlueprintAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal Blueprint specialist"),
            TEXT("Blueprint structure, component ownership, compile health, and editor-safe automation"),
            TEXT("Design Blueprint class structure, component hierarchies, variable/function/event conventions, construction-script safety, compile checks, and asset repair workflows. Respect Unreal ownership rules and verify with Blueprint compile or MCP inspection when available."));
    }

    FString MakeLevelWorldAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal level and world builder"),
            TEXT("Levels, actors, lighting, landscape, environment composition, and viewport evidence"),
            TEXT("Plan and execute level layout, spawn placement, environment dressing, lighting, navigation, collision, checkpoints, and screenshot evidence. Keep world edits bounded and validate with viewport, PIE, map, and actor inspection."));
    }

    FString MakeUiAudioVfxAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal UI audio VFX specialist"),
            TEXT("UMG, feedback, audio, VFX, cinematics, and player-facing polish"),
            TEXT("Create shippable feedback loops: UI state, HUD flow, menus, accessibility hooks, audio cues, Niagara/VFX, camera/cinematics, and polish passes. Verify assets compile and player-facing changes have observable evidence."));
    }

    FString MakeQaReleaseAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal QA and release lead"),
            TEXT("Validation, regression risk, evidence capture, automation, packaging, and release sign-off"),
            TEXT("Define acceptance criteria, run deterministic validation, capture logs/screenshots/build output, update the evidence ledger, identify blockers, and produce release-readiness checklists with residual risk."));
    }

    FString MakeNetworkingGasAgent()
    {
        return MakeSpecialistAgentMarkdown(
            TEXT("the Unreal networking and GAS specialist"),
            TEXT("Replication, multiplayer authority, Gameplay Ability System, prediction, and network tests"),
            TEXT("Review authority, ownership, replicated state, RPCs, prediction windows, ability activation, gameplay effects, attributes, save migration, and multiplayer test plans. Ask before introducing network-wide architecture changes."));
    }

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

    FString MakeGuardrailsPlugin()
    {
        return FString()
            + TEXT("// unreal_agent_studio_kit_version: 1\n")
            + TEXT("import type { Plugin } from \"@opencode-ai/plugin\"\n\n")
            + TEXT("const SECRET_PATTERNS = [\n")
            + TEXT("  /x-mcp-capability-token/iu,\n")
            + TEXT("  /authorization\\s*:/iu,\n")
            + TEXT("  /bearer\\s+[a-z0-9._\\-]+/iu,\n")
            + TEXT("  /api[_-]?key/iu,\n")
            + TEXT("  /access[_-]?token/iu,\n")
            + TEXT("  /refresh[_-]?token/iu,\n")
            + TEXT("  /password/iu,\n")
            + TEXT("  /secret/iu,\n")
            + TEXT("]\n\n")
            + TEXT("function redact(value: unknown): unknown {\n")
            + TEXT("  if (typeof value === \"string\") {\n")
            + TEXT("    if (SECRET_PATTERNS.some((pattern) => pattern.test(value))) return \"[REDACTED]\"\n")
            + TEXT("    return value\n")
            + TEXT("  }\n")
            + TEXT("  if (Array.isArray(value)) return value.map(redact)\n")
            + TEXT("  if (value && typeof value === \"object\") {\n")
            + TEXT("    return Object.fromEntries(Object.entries(value as Record<string, unknown>).map(([key, item]) => {\n")
            + TEXT("      if (SECRET_PATTERNS.some((pattern) => pattern.test(key))) return [key, \"[REDACTED]\"]\n")
            + TEXT("      return [key, redact(item)]\n")
            + TEXT("    }))\n")
            + TEXT("  }\n")
            + TEXT("  return value\n")
            + TEXT("}\n\n")
            + TEXT("export const UnrealAgentGuardrails: Plugin = async () => ({\n")
            + TEXT("  \"tool.execute.before\": async (_input, output) => {\n")
            + TEXT("    if (output && typeof output === \"object\" && \"args\" in output) {\n")
            + TEXT("      ;(output as { args?: unknown }).args = redact((output as { args?: unknown }).args)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("  \"tool.execute.after\": async (_input, output) => {\n")
            + TEXT("    const redacted = redact(output)\n")
            + TEXT("    if (output && typeof output === \"object\" && redacted && typeof redacted === \"object\") {\n")
            + TEXT("      Object.assign(output as Record<string, unknown>, redacted as Record<string, unknown>)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("  \"experimental.session.compacting\": async (_input, output) => {\n")
            + TEXT("    const reminder = \"Unreal Agent reminder: inspect before live editor claims, prefer reversible changes, validate with evidence, and do not expose capability tokens or secrets.\"\n")
            + TEXT("    if (output && typeof output === \"object\" && Array.isArray((output as { context?: unknown }).context)) {\n")
            + TEXT("      ;((output as { context: string[] }).context).push(reminder)\n")
            + TEXT("    }\n")
            + TEXT("  },\n")
            + TEXT("})\n\n")
            + TEXT("export default UnrealAgentGuardrails\n");
    }

    FString MakeOpenCodeConfig()
    {
        return FString()
            + TEXT("// unreal_agent_studio_kit_version: 1\n")
            + TEXT("{\n")
            + TEXT("  \"$schema\": \"https://opencode.ai/config.json\",\n")
            + TEXT("  \"permission\": {\n")
            + TEXT("    \"read\": \"allow\",\n")
            + TEXT("    \"glob\": \"allow\",\n")
            + TEXT("    \"grep\": \"allow\",\n")
            + TEXT("    \"list\": \"allow\",\n")
            + TEXT("    \"edit\": \"ask\",\n")
            + TEXT("    \"bash\": \"ask\",\n")
            + TEXT("    \"skill\": {\n")
            + TEXT("      \"unreal-*\": \"allow\"\n")
            + TEXT("    }\n")
            + TEXT("  }\n")
            + TEXT("}\n");
    }

    FString MakeLegacyOpenCodeConfig()
    {
        FString Config = MakeOpenCodeConfig();
        Config.RemoveFromStart(TEXT("// unreal_agent_studio_kit_version: 1\n"));
        return Config;
    }

    FString MakeEvidenceReadme()
    {
        return FString()
            + TEXT("# Unreal Agent Evidence\n\n")
            + FString::Printf(TEXT("%s\n\n"), StudioKitVersionMarker)
            + TEXT("This folder is managed by the Unreal Agent editor plugin. It stores compact validation events, decisions, and release evidence so the agent can report what was actually checked instead of guessing.\n");
    }

    void AddTemplate(TArray<FStudioKitTemplateFile>& Templates, const FString& RelativePath, FString Content, bool bOverwriteLegacyPrompt = false)
    {
        Templates.Add({ RelativePath, MoveTemp(Content), bOverwriteLegacyPrompt });
    }

    TArray<FStudioKitTemplateFile> BuildTemplateFiles()
    {
        TArray<FStudioKitTemplateFile> Templates;
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-agent.md"), FUnrealAgentStudioKit::MakePrimaryAgentMarkdown(), true);
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-technical-director.md"), MakeTechnicalDirectorAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-gameplay-programmer.md"), MakeGameplayProgrammerAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-blueprint-specialist.md"), MakeBlueprintAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-level-world-builder.md"), MakeLevelWorldAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-ui-audio-vfx.md"), MakeUiAudioVfxAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-qa-release.md"), MakeQaReleaseAgent());
        AddTemplate(Templates, TEXT(".opencode/agents/unreal-networking-gas.md"), MakeNetworkingGasAgent());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-mcp-tool-playbook/SKILL.md"), MakeToolPlaybookSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-project-bootstrap/SKILL.md"), MakeBootstrapSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-prototype/SKILL.md"), MakePrototypeSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"), MakeValidationSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-release-readiness/SKILL.md"), MakeReleaseSkill());
        AddTemplate(Templates, TEXT(".opencode/skills/unreal-debug-fix/SKILL.md"), MakeDebugFixSkill());
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-start.md"), MakeCommandMarkdown(TEXT("Start or reset the Unreal project production plan."), TEXT("Use the Unreal project bootstrap skill. Inspect available context, identify the current production stage, and propose the smallest playable next milestone with acceptance criteria.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-inspect.md"), MakeCommandMarkdown(TEXT("Inspect current Unreal project, editor, map, selection, and risks."), TEXT("Use the MCP tool playbook. Discover available `unreal-engine` MCP tools, inspect the project/editor state, summarize what is known, and list anything still unverified.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-prototype.md"), MakeCommandMarkdown(TEXT("Build the smallest playable Unreal prototype loop."), TEXT("Use the Unreal prototype skill. Create or plan the minimum playable loop, then validate with editor/MCP evidence before expanding scope.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-validate.md"), MakeCommandMarkdown(TEXT("Run a focused Unreal validation pass."), TEXT("Use the Unreal validation loop skill. State acceptance criteria, run the strongest available checks, record evidence, and report blockers separately from warnings.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-ship-check.md"), MakeCommandMarkdown(TEXT("Check release readiness for an Unreal feature or game."), TEXT("Use the Unreal release readiness skill. Review packaging, platform settings, maps, inputs, logs, performance, accessibility/localization, known issues, source-control state, and release blockers.")));
        AddTemplate(Templates, TEXT(".opencode/commands/unreal-fix-errors.md"), MakeCommandMarkdown(TEXT("Diagnose and fix Unreal errors with baseline-fix-retest discipline."), TEXT("Use the Unreal debug fix skill. Preserve exact errors, inspect likely causes, make the smallest fix, and retest the original failure path.")));
        AddTemplate(Templates, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"), MakeGuardrailsPlugin());
        AddTemplate(Templates, TEXT(".opencode/opencode.json"), MakeOpenCodeConfig());
        AddTemplate(Templates, TEXT("Saved/UnrealAgent/evidence/README.md"), MakeEvidenceReadme());
        return Templates;
    }

    void MarkPreserved(FUnrealAgentStudioKitResult& Result, const FString& Path)
    {
        ++Result.FilesPreserved;
        Result.PreservedPaths.Add(Path);
    }

    void MarkFailed(FUnrealAgentStudioKitResult& Result, const FString& Path)
    {
        ++Result.FilesFailed;
        Result.FailedPaths.Add(Path);
    }

    bool WriteTemplateFile(const FString& ProjectDirectory, const FStudioKitTemplateFile& TemplateFile, FUnrealAgentStudioKitResult& Result)
    {
        const FString Path = FPaths::Combine(ProjectDirectory, TemplateFile.RelativePath);
        const bool bIsOpenCodeConfig = TemplateFile.RelativePath == TEXT(".opencode/opencode.json");
        FString ExistingText;
        if (FPaths::FileExists(Path) && FFileHelper::LoadFileToString(ExistingText, *Path))
        {
            if (ExistingText == TemplateFile.Content)
            {
                MarkPreserved(Result, Path);
                return true;
            }

            const bool bManaged = FUnrealAgentStudioKit::IsManagedFileText(ExistingText)
                || (bIsOpenCodeConfig && ExistingText.Contains(TEXT("\"unreal_agent_studio_kit_version\"")))
                || (bIsOpenCodeConfig && ExistingText == MakeLegacyOpenCodeConfig())
                || (TemplateFile.bOverwriteLegacyPrompt && LooksLikeLegacyManagedPrompt(ExistingText));
            if (!bManaged)
            {
                MarkPreserved(Result, Path);
                return true;
            }
        }

        if (!EnsureParentDirectory(Path)
            || !FFileHelper::SaveStringToFile(TemplateFile.Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            MarkFailed(Result, Path);
            return false;
        }

        ++Result.FilesWritten;
        Result.WrittenPaths.Add(Path);
        return true;
    }
}

FString FUnrealAgentStudioKit::GetStudioKitVersionMarker()
{
    return StudioKitVersionMarker;
}

FString FUnrealAgentStudioKit::GetPromptVersionMarker()
{
    return PromptVersionMarker;
}

FString FUnrealAgentStudioKit::MakePrimaryAgentMarkdown()
{
    return MakeFrontMatter(TEXT("Unreal Editor game production director with live MCP control"), TEXT("primary"))
        + FString::Printf(TEXT("%s\n\n"), PromptVersionMarker)
        + TEXT("You are Unreal Agent, an in-editor game-studio lead for Unreal Engine projects. Your job is to help take a user's idea from an empty project to a shippable game while staying grounded in MCP observations, the editor context envelope, project files, logs, validation evidence, and explicit user choices.\n\n")
        + TEXT("Core operating loop:\n")
        + TEXT("1. Establish the production stage: concept, prototype, vertical slice, production, polish, release, or live support.\n")
        + TEXT("2. Inspect before acting. When unreal-engine MCP is connected, use manage_tools and inspect to learn the available tool surface and current editor/project state before claiming facts about assets, actors, levels, Blueprints, settings, tests, logs, screenshots, PIE, or the viewport.\n")
        + TEXT("3. Use the editor context envelope as a fast starting snapshot, then confirm high-impact or stale facts with MCP inspection before changing the project.\n")
        + TEXT("4. For vague or high-impact work, offer 2-4 concrete options with tradeoffs, get the user's direction when needed, then execute through the available tools.\n")
        + TEXT("5. Prefer small, reversible, Unreal-safe changes that fit the current project conventions. Ask before destructive edits, bulk operations, large refactors, or source-control actions.\n")
        + TEXT("6. After implementation, validate through MCP inspection, asset compilation, PIE/editor checks, screenshots, automation tests, logs, profiling, build output, and the evidence ledger. Report evidence and residual risks.\n\n")
        + TEXT("Studio roles available through this kit:\n")
        + TEXT("- unreal-technical-director: architecture, production risk, platform/release gates.\n")
        + TEXT("- unreal-gameplay-programmer: playable loops, actors, components, systems, multiplayer constraints.\n")
        + TEXT("- unreal-blueprint-specialist: Blueprint structure, compile health, safe component ownership.\n")
        + TEXT("- unreal-level-world-builder: levels, actors, lighting, navigation, screenshots.\n")
        + TEXT("- unreal-ui-audio-vfx: UI, feedback, audio, VFX, cinematics, polish.\n")
        + TEXT("- unreal-networking-gas: replication, GAS, prediction, authority.\n")
        + TEXT("- unreal-qa-release: validation, evidence, packaging, ship-readiness.\n\n")
        + TEXT("Full-game workflow:\n")
        + TEXT("- Concept: clarify genre, player fantasy, core loop, audience, platform, production constraints, and the smallest fun prototype.\n")
        + TEXT("- Design: maintain a compact GDD, feature list, system map, input scheme, UX flow, content needs, acceptance criteria, and non-goals.\n")
        + TEXT("- Architecture: define modules, GameMode/GameState/Pawn/Controller/HUD/GameInstance ownership, subsystem boundaries, C++ versus Blueprint responsibilities, save/load, networking, data assets, and content-folder conventions.\n")
        + TEXT("- Prototype: create the smallest playable loop and verify it in editor or PIE.\n")
        + TEXT("- Vertical slice: add representative art/audio/UI/VFX/AI/gameplay quality and prove the pipeline.\n")
        + TEXT("- Production: create or modify assets, Blueprints, levels, gameplay systems, UI, AI, audio, VFX, cinematics, inventory, combat, interaction, networking, GAS, and tools through the relevant MCP domains.\n")
        + TEXT("- Quality: compile assets, run tests, exercise PIE, inspect runtime state, capture screenshots when visual work matters, profile performance, review accessibility/localization, and keep a release checklist.\n")
        + TEXT("- Shipping: confirm packaging readiness, platform settings, scalability, input, save migration, logs, crash risk, source-control state, documentation, changelog, and known issues.\n\n")
        + TEXT("MCP tool playbook:\n")
        + TEXT("- manage_tools: discover canonical tools, enabled categories, and missing capabilities before broad production work.\n")
        + TEXT("- inspect: read project, world, actor, Blueprint CDO, class, component, viewport, selection, PIE/runtime, and log facts.\n")
        + TEXT("- manage_asset, manage_blueprint, control_actor, manage_level, manage_level_structure, build_environment, and control_editor: build the playable world, assets, Blueprints, actors, levels, lighting, landscape, viewport, screenshots, and PIE/editor flow.\n")
        + TEXT("- animation_physics, manage_character, manage_combat, manage_ai, manage_gas, manage_networking, manage_inventory, and manage_interaction: implement player, combat, abilities, AI, multiplayer, inventory, and interaction systems.\n")
        + TEXT("- manage_audio, manage_effect, manage_sequence, manage_geometry, and system_control: author audio, VFX, cinematics, geometry, project settings, profiling, validation, console/Python automation, tests, and build checks.\n")
        + TEXT("If a needed MCP server or tool is missing, say exactly what capability is unavailable and continue with source/config/log analysis or ask the user to enable the bridge.\n\n")
        + TEXT("OpenCode kit discipline:\n")
        + TEXT("- Use the generated skills when they match the task: unreal-project-bootstrap, unreal-prototype, unreal-mcp-tool-playbook, unreal-validation-loop, unreal-release-readiness, and unreal-debug-fix.\n")
        + TEXT("- The local plugin hooks are guardrails, not magic. Still reason explicitly about destructive changes, privacy, validation, and user approval.\n")
        + TEXT("- Keep work traceable to user intent and observable evidence. Do not invent assets, editor state, test results, screenshots, performance numbers, or successful builds.\n")
        + TEXT("- Favor tool-backed creation and verification over manual instructions. When a complete game is requested, deliver in staged increments: playable prototype first, then vertical slice, then production polish and release readiness.\n");
}

FUnrealAgentStudioKitResult FUnrealAgentStudioKit::EnsureForProject(const FString& ProjectDirectory)
{
    FUnrealAgentStudioKitResult Result;
    const FString NormalizedProjectDirectory = NormalizeStudioKitProjectDirectory(ProjectDirectory);
    if (NormalizedProjectDirectory.IsEmpty() || !IFileManager::Get().MakeDirectory(*NormalizedProjectDirectory, true))
    {
        MarkFailed(Result, NormalizedProjectDirectory);
        Result.Summary = BuildStatusSummary(Result);
        return Result;
    }

    for (const FStudioKitTemplateFile& TemplateFile : BuildTemplateFiles())
    {
        WriteTemplateFile(NormalizedProjectDirectory, TemplateFile, Result);
    }

    Result.Summary = BuildStatusSummary(Result);
    return Result;
}

FString FUnrealAgentStudioKit::RedactSensitiveText(const FString& Text)
{
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, false);
    if (Lines.IsEmpty() && !Text.IsEmpty())
    {
        Lines.Add(Text);
    }

    FString Redacted;
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        if (Index > 0)
        {
            Redacted += LINE_TERMINATOR;
        }

        Redacted += IsSensitiveLine(Lines[Index]) ? RedactLine(Lines[Index]) : Lines[Index];
    }
    return Redacted;
}

bool FUnrealAgentStudioKit::IsManagedFileText(const FString& Text)
{
    return Text.Contains(StudioKitVersionMarker);
}

FString FUnrealAgentStudioKit::BuildStatusSummary(const FUnrealAgentStudioKitResult& Result)
{
    return FString::Printf(
        TEXT("Studio Kit: %d written, %d preserved, %d failed"),
        Result.FilesWritten,
        Result.FilesPreserved,
        Result.FilesFailed);
}
