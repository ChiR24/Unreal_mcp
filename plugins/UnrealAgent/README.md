# Unreal Agent Plugin

Editor-only Unreal plugin for an in-editor OpenCode ACP assistant panel.

The plugin gives the editor a focused OpenCode chat surface and can pass the configured `unreal-engine` MCP server into OpenCode sessions. The goal is not to pretend an autonomous swarm is running. The goal is a practical in-editor game-production assistant that discovers available MCP tools, inspects the current project/editor state, executes through safe tool domains, and verifies the result before claiming success.

## Current Surface

- `Window > Unreal Agent` opens the Slate panel.
- `Connect` starts `opencode acp` in the current Unreal project directory.
- On connect, the plugin writes a managed OpenCode Studio Kit into `.opencode/` and `Saved/UnrealAgent/`.
- Prompts can include a compact redacted editor context envelope with project, map, PIE, selection, dirty-package, and evidence-ledger facts.
- `session/new` receives the current project directory and any configured `unreal-engine` MCP server.
- The model, thinking, and agent selectors are populated from ACP config options when OpenCode advertises them.
- If OpenCode exposes an `unreal-agent` mode, the panel selects it automatically.
- `Send` creates an ACP prompt, attaches editor context when enabled, and streams assistant, thought, tool, plan, permission, and error activity into the panel.
- The cockpit row can refresh context and run a lightweight validation/evidence pass without asking OpenCode to guess.
- `Cancel turn` sends ACP cancellation for the active prompt without killing the process.
- Non-Unreal permission requests can be allowed once, allowed always when ACP exposes that option, or rejected. Unreal/editor requests intentionally remove persistent approval and remain one-shot.

## Runtime Prompt

The generated primary prompt is owned by `FUnrealAgentStudioKit::MakePrimaryAgentMarkdown()` and carries version markers:

```yaml
unreal_agent_prompt_version: 2
unreal_agent_studio_kit_version: 1
```

The prompt tells OpenCode to work like a compact Unreal game studio:

- Establish the production stage: concept, prototype, vertical slice, production, polish, release, or live support.
- Inspect before acting. Run `/unreal-tool-inventory` before broad production work, then use `inspect` when MCP is connected before claiming facts about assets, selected Content Browser folders, actors, levels, Blueprints, settings, tests, logs, screenshots, the viewport, or PIE state.
- For new or returning users, run `/unreal-getting-started` for official Epic onboarding concepts plus inspected editor context, `/unreal-project-setup-plan` before changing template defaults, maps/modes, starter/sample content, or first-map setup, `/unreal-editor-tour` for live orientation, or `/unreal-first-playable` for a beginner-friendly inspected editor-to-PIE loop grounded in viewport, World Outliner, Details, Content Browser, PIE, and log evidence.
- Offer 2-4 concrete options for vague or high-impact work, then execute after the user's direction is clear.
- Prefer small, reversible, Unreal-safe changes that fit existing project conventions, use MCP tools for editor/asset/project-setting mutations instead of direct `.uasset`, `.umap`, `.uproject`, or `Config/*.ini` edits, and state a compact MCP route card before mutating project state.
- Validate work through MCP inspection, asset compilation, PIE/editor checks, screenshots, automation tests, logs, profiling, or build output; use the focused generated plan command for the affected domain, including `/unreal-editor-control-plan` for viewport, selection, screenshot, tab, save, modal, transaction, and PIE/SIE lifecycle operations, `/unreal-data-save-accessibility-plan` for SaveGame, data assets/tables, localization, user settings, and accessibility work, `/unreal-source-control-plan` for checkout, submit, shared package, redirector, conflict, and team-collaboration risk, `/unreal-performance-insights-plan` for FPS, frame-time, memory, scalability, benchmark, and Unreal Insights trace work, `/unreal-diagnostics-plan` for crashes, asserts, ensures, Blueprint compiler errors, build/cook/package failures, PIE runtime failures, and log regressions, `/unreal-project-setup-plan` for template/default-map/mode/starter-content setup, and `/unreal-system-project-plan` for project settings, console/Python automation, packaging, cook, build, and platform operations.
- For system/project operations, inspect settings, config ownership, enabled plugins/modules, platform target, dirty state, and logs first; route through `system_control` plus `inspect` or `control_editor` as needed; obtain approval for destructive or long-running work; preserve exact output/log evidence; and do not infer packaging or performance success from config-only evidence.
- Improve prompts and workflows with a baseline-test, targeted-fix, retest loop.

## Studio Kit

`FUnrealAgentStudioKit` generates managed project-local OpenCode files:

- `.opencode/agents/unreal-agent.md` plus specialist agents for technical direction, gameplay, Blueprints, level/world building, UI/audio/VFX, networking/GAS, and QA/release.
- `.opencode/skills/*/SKILL.md` for MCP tool selection, route cards, PIE/SIE safety, editor-control operations, Content Browser asset planning, Blueprint compile discipline, level/actor placement discipline, world building, gameplay/input ownership, animation/physics motion, VFX/material presentation, audio, cinematic/Sequencer, networking/GAS, character systems, UI/HUD presentation, AI/navigation behavior, data/save/localization/accessibility discipline, source-control collaboration, performance/Insights measurement, diagnostics/crash recovery, system/project operations, project bootstrap, project setup/templates, official getting-started onboarding, editor orientation, first playable editor loops, prototype, validation, release readiness, and debug/fix loops.
- `.opencode/commands/unreal-*.md` for start, getting started, project setup, editor tour, first playable, PIE/SIE check, editor-control plan, content plan, Blueprint plan, level/actor plan, world-building plan, gameplay/input plan, animation/physics plan, VFX/material plan, audio plan, cinematic/Sequencer plan, networking/GAS plan, character-systems plan, UI/HUD plan, AI/navigation plan, data/save/accessibility plan, source-control plan, performance/Insights plan, diagnostics plan, system/project plan, tool inventory, route-card, inspect, C++ context, prototype, validate, ship-check, and fix-errors workflows.
- `.opencode/plugins/unreal-agent-guardrails.ts` for local OpenCode hook guardrails, MCP mutation preflight enforcement, direct binary-asset, project-state, Content directory, and `/Game`/`/Engine` package mutation blocking, destructive shell/source-control blocking, and redaction.
- `.opencode/opencode.json` with conservative default permissions.
- `Saved/UnrealAgent/` for `state.json`, `decisions.md`, and evidence records.

Managed Studio Kit files carry `unreal_agent_studio_kit_version: 1` where the target format permits plugin metadata. `.opencode/opencode.json` uses a JSONC comment marker so it remains upgradeable without adding unknown OpenCode config keys. Existing unmarked user-authored files are preserved, but ACP startup fails closed when the required guardrail plugin differs from the generated source or when another project, ancestor, global, managed, or configured OpenCode plugin could execute alongside it.

## Editor Context And Evidence

`FUnrealAgentEditorContext` captures a bounded, redacted prompt envelope. It is a fast starting snapshot, not a replacement for MCP `inspect`; the envelope states whether `unreal-engine` MCP was configured for the current ACP session, requires `/unreal-tool-inventory` before broad production work, and reminds the agent that availability still depends on successful tool responses.

`FUnrealAgentEvidenceLedger` records validation events under `Saved/UnrealAgent/evidence/` and maintains a compact `state.json` plus `decisions.md`. `FUnrealAgentValidationRunner` checks the Studio Kit, evidence writability, and current editor context, then records an evidence artifact.

## MCP Tool Playbook

The runtime prompt names the MCP tool domains the agent should reach for:

- `manage_tools`: run `/unreal-tool-inventory` before broad production work to record canonical parent tools, enabled categories, missing capabilities, and the planned domain mapping; refresh it after reconnects or MCP configuration changes.
- `inspect`: read project, world, actor, Blueprint CDO, class, component, viewport, selection, Content Browser state through `get_content_browser_state`, and runtime facts. Asset/editor changes should go through MCP domain tools rather than direct binary asset writes, with route cards naming the intended tool/action and validation evidence. Use `/unreal-content-plan` before asset/folder create, import, move, rename, duplicate, delete, or migration work so `/Game` package paths, selected folders, dependencies, redirectors, and validation are explicit.
- `control_editor`: control viewport/navigation, selection, screenshots, open asset editors/tabs, editor modes, saves, modal-sensitive actions, undo/redo transactions, and PIE/SIE lifecycle. Run `/unreal-editor-control-plan`, inspect before and after, and separate current editor state, screenshot pixels, preview-world behavior, and persisted asset/map evidence.
- `manage_asset`, `manage_blueprint`, `control_actor`, `manage_level`, `manage_level_structure`, `build_environment`, `manage_pcg`, `manage_geometry`: build the playable world, assets, Blueprints, actors, levels, lighting, landscape, foliage, PCG, and geometry. Blueprint work should run `/unreal-blueprint-plan`; actor placement, transform, attachment, collision, mobility, Data Layer, World Partition, or level-structure work should run `/unreal-level-actor-plan`; landscape, foliage, PCG, spline, generated geometry, HLOD, streaming, or broad environment work should run `/unreal-world-building-plan`.
- `animation_physics`, `manage_character`, `manage_combat`, `manage_ai`, `manage_gas`, `manage_networking`, `manage_inventory`, `manage_interaction`: implement player, combat, abilities, AI, multiplayer, inventory, and interaction systems. Animation Blueprint, Skeletal Mesh, Skeleton, Blend Space, State Machine, Montage, Notify, Control Rig, IK/retarget, Physics Asset, Chaos, collision, constraint, ragdoll, or root-motion work should run `/unreal-animation-physics-plan` before mutation; gameplay framework, possession, camera, HUD, and Enhanced Input work should run `/unreal-gameplay-input-plan` before mutation; UMG, Widget Blueprint, UserWidget, CommonUI, input mode, focus, menu, HUD, or viewport UI work should run `/unreal-ui-hud-plan` before mutation; AIController, Behavior Tree, Blackboard, Perception, EQS, NavMesh, patrol, chase, or pathfinding work should run `/unreal-ai-navigation-plan` before mutation.
- `manage_audio`, `manage_effect`, `manage_sequence`: author audio, VFX, materials, cinematics, and player-facing presentation. Use the corresponding audio, VFX/material, or cinematic/Sequencer plan before mutation.
- `manage_asset`, `manage_inventory`, `manage_blueprint`, `system_control`, plus UI/editor routes: author and validate SaveGame schemas, Data Assets, Data Tables, String Tables, user settings, localization keys, subtitles/captions, input remapping, and accessibility options. Run `/unreal-data-save-accessibility-plan`, inspect ownership and migration risk, then validate save/load, data read-back, localized text, settings persistence, focus/layout, accessibility toggles, and exact logs/reports.
- `manage_asset` source-control actions: inspect and coordinate Unreal package checkout, submit, and source-control state for `/Game` assets, maps, redirectors, and external actor packages. Run `/unreal-source-control-plan`, read `get_source_control_state` before mutation, use `source_control_checkout` or `source_control_submit` only with approval and exact assetPaths, and never substitute local git, shell moves, or raw filesystem writes for Unreal package source control.
- `system_control` performance and Insights actions: measure FPS, frame time, hitches, memory, streaming, LOD/Nanite, scalability, benchmarks, and Unreal Insights traces through `manage_performance` and `manage_insights` routes. Run `/unreal-performance-insights-plan`, inspect `get_performance_stats`, `get_memory_stats`, and `get_scene_stats`, capture baseline and after metrics, preserve `traceFile`/`.utrace` or memory report paths, and do not claim optimization from a config diff or quality reduction alone.
- `inspect`, `system_control`, `manage_blueprint`, `manage_asset`, and `control_editor`: diagnose crashes, asserts, ensures, editor hangs, Blueprint compiler failures, UBT/build/cook/package failures, PIE runtime failures, asset load errors, log regressions, and suspected regressions. Run `/unreal-diagnostics-plan`, preserve exact error text, call stack, `Saved/Crashes` or `Saved/Logs` path, reproduction step, recent changes, Output Log or Message Log evidence, route through `run_tests`, `validate_assets`, `run_ubt`, Blueprint compile, asset validation, or PIE routes as appropriate, and retest the original failure path before claiming fixed.
- `system_control`, `manage_level`, `manage_level_structure`, `manage_asset`, and `control_editor`: plan template-derived defaults, first-map setup, default maps and modes, GameMode/GameInstance defaults, target platform/quality presets, starter/sample content, plugin enablement, and project-content migration. Run `/unreal-project-setup-plan`, inspect `get_project_settings` read-back, Content Browser state, dependencies, dirty packages, restart/source-control risk, and logs; validate map/default read-back, package paths, open/save state, PIE startup behavior, and rollback.
- `system_control`: inspect and change project settings, maps and modes, plugin/module/config state, console variables and commands, Python/editor utility automation, automation tests, packaging, cook, build, deploy, and platform operations. Run `/unreal-system-project-plan`, use `inspect` or `control_editor` for supporting evidence, get approval for destructive or long-running operations, capture exact output/log paths, and do not claim packaging or performance from config-only evidence.

If a required MCP server or tool is unavailable, the agent should say exactly what is missing and continue from source, config, docs, or logs instead of inventing live editor state.

## Full Game Workflow

For a request like "make a complete game", the agent should work in staged increments:

- **Concept**: clarify genre, player fantasy, core loop, audience, platforms, production constraints, and the smallest fun prototype.
- **Design**: maintain a compact GDD, feature list, input scheme, UX flow, content needs, acceptance criteria, and non-goals.
- **Architecture**: define module ownership, GameMode/GameState/Pawn/Controller/HUD/GameInstance responsibilities, subsystem boundaries, C++ versus Blueprint boundaries, save/load, networking, data assets, and content folder conventions.
- **Prototype**: create the smallest playable loop and verify it in editor/PIE.
- **Official getting started loop**: for beginner questions, anchor concepts in Epic's Create your First Project, Unreal Editor Interface, Content Browser, and In-Editor Testing docs, then use MCP inventory and inspect before claiming the live project/template, viewport, selection, Content Browser, PIE state, dirty packages, or logs.
- **Project setup and template loop**: before changing template-derived defaults, starter/sample content, first-map setup, Maps & Modes, GameMode/GameInstance defaults, target platform/quality presets, plugin enablement, or migrated project content, inspect project settings through `get_project_settings`, Content Browser/package paths, dependencies, dirty packages, restart/source-control risk, and logs; route through `system_control`, map/asset/editor MCP tools; then verify settings read-back, map load/open/save state, package paths, PIE startup behavior, and rollback.
- **First playable editor loop**: for new projects or tutorials, inventory tools, inspect the project/template, viewport, World Outliner, Details, Content Browser, PIE state, dirty packages, and logs; make one reversible MCP-backed change; then validate in editor/PIE with evidence.
- **Editor control loop**: before viewport, selection, screenshot, open-tab, editor-mode, save, modal-dialog, transaction/undo/redo, or PIE/SIE lifecycle changes, inspect the current editor world/map, viewport, selection, active asset/tab, preview session, dirty packages, modal and overwrite risk, then route through `control_editor`; verify the resulting editor state and distinguish it from screenshot-only, preview-only, or persisted asset/map evidence.
- **Content Browser asset loop**: before asset or folder changes, inspect selected Content Browser context, choose canonical `/Game` package paths, account for dependencies, redirectors, Developers folder scope, and source/destination validation, then mutate through MCP asset tools.
- **Blueprint compile loop**: before Blueprint edits, inspect the Blueprint asset/class/CDO, state graph/default/component intent, mutate through MCP Blueprint tools, compile and save, review compiler/log errors, and validate runtime behavior in PIE when gameplay changed.
- **Level and actor loop**: before world edits, inspect the current map, selected actor, Outliner hierarchy, root component, transform, attachments, mobility, collision, level ownership, Data Layers, and World Partition state; mutate through actor/level MCP tools, save the map, capture viewport evidence, and validate collision, navigation, spawning, or streaming in PIE when relevant.
- **World building loop**: before landscape, foliage, PCG, spline, geometry, Data Layer, HLOD, Runtime Grid, or World Partition streaming edits, inspect the current map, World Partition state, Data Layers, landscape actor/material/layers, foliage types/instances, PCG graph/component/nodes, spline or geometry targets, collision/nav/performance risk, viewport state, dirty packages, and logs, then validate terrain visibility, layer paint, foliage density/culling/collision, PCG generation bounds, geometry collision, Data Layer ownership, streaming/nav behavior, performance impact, screenshots, persisted save state, and logs.
- **Gameplay and input loop**: before playable-system changes, inspect GameMode, Pawn/Character, PlayerController, PlayerState/GameState, HUD/UI, camera, Enhanced Input Actions and Mapping Contexts, then validate possession, input events, camera/HUD feedback, and logs in PIE.
- **Animation and physics loop**: before motion or simulation changes, inspect Skeletal Mesh, Skeleton, Animation Blueprint, AnimGraph, State Machine, Blend Space, Montage, Notifies, Control Rig/IK/retarget assets, Physics Asset, collision bodies, constraints, Chaos settings, root motion, and logs, then validate animation playback, notify timing, collision/trace behavior, physics stability, root motion, networking assumptions, and logs in PIE.
- **VFX and material loop**: before visual-effect or material changes, inspect Niagara Systems, emitters, modules, user parameters, scalability, bounds, attached Actors, Materials, Material Instances, textures, shader compile state, decals, post process, Lumen/Nanite assumptions, viewport visibility, and logs, then validate effect spawning, parameter updates, material assignment, shader/texture correctness, bounds/culling, performance impact, screenshots, and runtime logs in PIE.
- **Audio loop**: before sound or mix changes, inspect Sound Waves, Sound Cues, MetaSound Sources/Presets, Audio Components, attenuation, concurrency, Submix/Sound Class routing, Audio Volumes/Reverb, listener context, gameplay trigger path, dirty packages, and logs, then validate audible playback, listener-relative spatialization, mix routing, concurrency, trigger timing, multiplayer listener assumptions, and runtime logs in PIE.
- **Cinematic and Sequencer loop**: before timeline or render changes, inspect Level Sequence Asset, Level Sequence Actor, bound actors, spawnables/possessables, tracks, keyframes, Camera Cut track, CineCameraActors, playback settings, Movie Render Queue config/output, dirty packages, and logs, then validate sequence playback, camera cuts, actor bindings, audio/cinematic track alignment, restore-state behavior, render queue settings, and output evidence.
- **Networking and GAS loop**: before multiplayer or ability-system changes, inspect NetMode, PIE client count, GameMode/GameState/PlayerState, replicated Actors/properties/RPC paths, Ability System Component owner/avatar, Attribute Sets, Gameplay Abilities, Gameplay Effects, Gameplay Tags/Cues, prediction assumptions, dirty packages, and logs, then validate server authority, client replication, RPC direction, ability activation, costs, cooldowns, effects, cues, prediction, late-join/respawn assumptions, and logs in multiplayer PIE.
- **Character systems loop**: before Character, combat, inventory, or interaction changes, inspect Character/Pawn, PlayerController, Character Movement Component, camera, capsule/collision, mesh/Animation Blueprint, health/damage, weapon/projectile assets, inventory/equipment data, interactable Actor/interface/trace or overlap path, dirty packages, and logs, then validate movement, camera, collision, damage, inventory/equipment state, interaction prompts/activation, UI/audio/VFX feedback, authority assumptions, and logs in PIE.
- **UI and HUD loop**: before user-interface changes, inspect HUD class, Widget Blueprint/UserWidget hierarchy, bindings, animations, CommonUI activatable widget stack, input mode, focus target, DPI scale, anchors, localization, accessibility, viewport state, and logs, then validate visible layout, focus/navigation, controller/mouse/touch input, responsive DPI, and runtime logs in PIE.
- **Data, save, localization, and accessibility loop**: before persistent-data or inclusive-UX changes, inspect SaveGame class/slot schema, GameInstance or subsystem owner, Data Assets, Data Tables, String Tables, user settings/config, Widget text, localization namespace/key/table, input remapping, subtitle/caption behavior, color/contrast, DPI/font scaling, focus/navigation, dirty packages, migration risk, and logs; route through existing MCP asset/inventory/Blueprint/system/UI/editor tools; then validate create-save-load-migrate behavior, data read-back, localized text, settings persistence, accessibility toggles, target input modes, and exact automation/log evidence.
- **AI and navigation loop**: before AI behavior changes, inspect AIController, Pawn, Behavior Tree, Blackboard keys, Perception, EQS queries, Nav Mesh Bounds Volume, RecastNavMesh, collision, patrol targets, and logs, then validate possession, blackboard updates, behavior-tree branch/task execution, path following, perception/EQS events, and nav/path logs in PIE.
- **Source-control collaboration loop**: before checkout, submit, shared asset/map mutation, redirector cleanup, external actor package work, or provider changes, inspect Content Browser state, dirty packages, dependencies, redirectors, current map/external actor packages, source-control provider availability, and logs; read `get_source_control_state` for exact `/Game` assetPaths; ask before locks, submit, provider changes, conflicts, out-of-date packages, checked-out-by-other packages, or shared-folder bulk operations; then validate state read-back, dependency/redirector health, exact submit/checkout output, and remaining dirty/unsubmitted packages.
- **Performance and Insights loop**: before FPS, frame-time, hitch, memory, streaming, scalability, benchmark, or Unreal Insights trace claims, inspect target platform, editor/PIE scenario, scalability/device profile, performance stats, memory stats, scene stats, logs, and content scope; route through `system_control` performance or Insights actions; capture baseline and after metrics, `traceFile`/`.utrace` or memory report paths, visual-quality tradeoffs, and exact log evidence; and do not claim optimization from lower settings, resolution changes, editor-only viewport state, or config-only evidence.
- **Diagnostics and crash recovery loop**: before crashes, fatal errors, asserts, ensures, editor hangs, Blueprint compiler failures, UBT/build/cook/package failures, PIE runtime errors, asset load failures, log errors, or suspected regressions, preserve exact error text, call stack, crash/log/report paths, reproduction step, dirty packages, current map/asset/Blueprint/PIE state, and recent changes; classify the failure; route through `inspect`, `system_control run_tests`, `system_control validate_assets`, `system_control run_ubt`, `manage_blueprint` compile, `manage_asset` validation, or `control_editor` PIE reproduction; avoid cache deletion or destructive cleanup without explicit approval; and retest the original failure path before claiming fixed.
- **System and project operations loop**: before project-wide settings or automation changes, inspect Project Settings, maps and modes, enabled plugins/modules, config ownership, console variables, Python/editor utility scope, automation state, packaging/cook/build settings, target platform, dirty packages, source-control risk, and logs; route through `system_control`; capture exact output or report paths; distinguish warnings from blockers; and verify settings read-back, command results, automation counts, or package/cook/build output rather than inferring success from configuration.
- **PIE/SIE safety loop**: before claiming runtime success or persisted changes, separate editor-world facts from PIE/SIE preview-session facts, stop preview cleanly, refresh inspect/logs, and confirm save/compile or Keep Simulation Changes evidence when persistence matters.
- **Vertical slice**: add representative art/audio/UI/VFX/AI/gameplay quality and prove the pipeline.
- **Production**: expand content and systems through the right MCP domains.
- **Polish**: profile, fix bugs, improve UX, tune controls, add accessibility/localization readiness, and remove placeholder content.
- **Release**: verify packaging readiness, platform settings, scalability, input, save migration, logs, crash risk, source-control state, documentation, changelog, and known issues.

The Claude-Code-Game-Studios repo is useful as a reference for studio structure, specialist coverage, stage gates, QA sign-off, release checklists, and improvement loops. Do not copy its Claude Code mechanics directly; translate useful habits into OpenCode ACP plus `unreal-engine` MCP behavior.

## Built-In Quick Prompts

- **Architecture review**: reviews shippable-game architecture, ownership boundaries, content conventions, and risks.
- **Gameplay plan**: turns a concept into prototype, vertical-slice, production, polish, and release-readiness stages mapped to MCP tool domains.
- **QA risk pass**: defines ship-readiness criteria, regression risks, deterministic verification, and release blockers.
- **Editor tooling**: designs or executes reversible MCP-backed automation workflows with explicit validation.

Quick prompts may use source, config, docs, and logs. They may use live editor state only when OpenCode is configured with the `unreal-engine` MCP server.

## OpenCode Resolution

Executable lookup order:

1. Absolute `OPENCODE_ACP_COMMAND`
2. `~/.opencode/bin/opencode`
3. Absolute `PATH` entries outside the current project directory

Project-relative executables are intentionally rejected.

## MCP Configuration

The panel does not expose MCP tools itself. It injects the bridge's native MCP endpoint into `session/new` only when all of these conditions hold:

- Settings section: `/Script/McpAutomationBridge.McpAutomationBridgeSettings`
- `bEnableNativeMCP=true`
- The configured endpoint normalizes to loopback. Opted-in non-loopback endpoints are not injected into ACP sessions.
- `bRequireCapabilityToken=true`
- `CapabilityToken` is nonempty
- URL shape: `http://<ListenHost>:<NativeMCPPort>/mcp`
- Server name: `unreal-engine`
- Capability token header: `X-MCP-Capability-Token`

Non-loopback and token policy live in `plugins/McpAutomationBridge`; keep this panel as the ACP client and UI layer.

## File Map

| Task | Location |
| --- | --- |
| Register/open panel | `Source/UnrealAgent/Private/UnrealAgentModule.cpp` |
| ACP process, JSON-RPC, context attachment, MCP injection | `Source/UnrealAgent/Private/Acp/Client/` |
| Studio Kit generation, context, evidence, validation | `Source/UnrealAgent/Private/Acp/` responsibility folders |
| Slate panel, quick prompts, model/agent menus, transcript | `Source/UnrealAgent/Private/UI/` responsibility folders |
| Automation coverage | `Source/UnrealAgent/Private/Tests/` |
| Module deps | `Source/UnrealAgent/UnrealAgent.Build.cs` |
| Packaging filter | `Config/FilterPlugin.ini` |

## Transcript Rules

- Raw JSON-RPC frames are not shown as chat rows.
- Normal process stdout/stderr noise is not shown as chat rows.
- Startup, timeout, and exit failures show actionable diagnostics.
- Tool/status events are grouped as activity rows.
- Stable Slate tags prefixed `UnrealAgent.*` are part of the automation-test contract.

## Verification

Run the generated guardrail runtime regression harness:

```bash
npm run test:unreal-agent-guardrails
```

Compile the plugin module:

```bash
SESSION="unreal-agent-build-$(date +%Y%m%d-%H%M%S)"
tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh UnrealEditor Linux Development -Plugin=/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin -NoHotReloadFromIDE 2>&1 | tee /tmp/$SESSION.log"
```

Package the plugin:

```bash
SESSION="unreal-agent-package-$(date +%Y%m%d-%H%M%S)"
tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin=/data/GitHub/Unreal_mcp_main/plugins/UnrealAgent/UnrealAgent.uplugin -Package=/tmp/opencode/UnrealAgentPackage-final -TargetPlatforms=Linux -Rocket -WaitForUATMutex 2>&1 | tee /tmp/$SESSION.log"
```

Run ACP automation tests from a host project with the plugin enabled:

```bash
SESSION="unreal-agent-tests-$(date +%Y%m%d-%H%M%S)"
tmux new-session -d -s "$SESSION" "/data/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd /path/to/HostProject.uproject -nosplash -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests UnrealAgent.Acp' -TestExit='Automation Test Queue Empty' -ReportExportPath=/tmp/opencode/unreal-agent-report-final 2>&1 | tee /tmp/$SESSION.log"
```

All Unreal build, package, test, and editor commands must run in uniquely named tmux sessions. Use `tmux attach-session -t "$SESSION"` while a command is active and retain `/tmp/$SESSION.log` as evidence.

Expected report: `/tmp/opencode/unreal-agent-report-final/index.json` with `UnrealAgent.Acp.ClientProtocol`, `UnrealAgent.Acp.PanelOpens`, `UnrealAgent.Acp.StudioKitAndContext`, `UnrealAgent.Acp.StudioKitGuardrails`, `UnrealAgent.Acp.Security`, `UnrealAgent.Acp.PermissionSafety`, `UnrealAgent.Acp.PermissionSyntax`, and `UnrealAgent.Acp.PermissionSources` passing.

## Safety

- Do not claim live editor state without MCP output.
- Do not treat viewport position, selection, open tabs, screenshot capture, or PIE/SIE preview-session state as persisted editor/map/asset state; use fresh inspect plus save/compile or Keep Simulation Changes evidence when persistence matters.
- Do not create, import, move, rename, duplicate, migrate, or delete assets from raw filesystem paths when MCP asset/content tools can operate on `/Game` package paths and inspected Content Browser folders.
- Do not claim Blueprint graph, default, variable, function, event, component, construction-script, or parent-class changes are valid until Blueprint compile/save plus fresh inspect/log evidence prove it; use PIE/runtime evidence for behavior changes.
- Do not claim actor placement, transform, attachment, collision, mobility, Data Layer, World Partition, or level-structure changes persisted until the intended editor world is inspected, the map or external actor package is saved, and viewport/PIE evidence proves the relevant behavior.
- Do not claim gameplay or input works until GameMode/Pawn/Controller ownership, Enhanced Input actions/mapping contexts, possession, camera/HUD feedback, runtime logs, and PIE behavior are verified.
- Do not claim animation or physics works until Animation Blueprint/Skeleton/Skeletal Mesh ownership, Blend Space or Montage state, Notify timing, Physics Asset/collision/constraint behavior, root motion, runtime logs, and PIE behavior are verified.
- Do not claim VFX or material visuals work until Niagara System/emitter ownership, user parameters, Material/Material Instance/texture assignment, shader compile state, bounds/culling, screenshots, runtime logs, and PIE behavior are verified.
- Do not claim UI or HUD works until Widget Blueprint/UserWidget ownership, CommonUI or viewport ownership, input mode/focus, layout/DPI behavior, accessibility/localization risk, runtime logs, and PIE viewport behavior are verified.
- Do not claim SaveGame, data assets/tables, user settings, localization, subtitles/captions, input remapping, or accessibility behavior works until schema ownership, migration/default compatibility, save/load or data read-back, localized namespace/key/table resolution, settings persistence, focus/layout/contrast/input-mode checks, exact logs, and PIE or automation evidence are verified.
- Do not claim AI or navigation works until AIController/Pawn ownership, Behavior Tree and Blackboard state, Perception or EQS evidence, NavMesh coverage/path following, runtime logs, and PIE behavior are verified.
- Do not claim source-control checkout, submit, or shared-package safety until exact `/Game` assetPaths, `get_source_control_state` read-back, checked-out-by-other/conflict/out-of-date status, dependency and redirector impact, user approval, exact submit/checkout output, and remaining dirty/unsubmitted package state are verified.
- Do not claim performance optimization, FPS/frame-time improvement, hitch removal, memory improvement, scalability readiness, or Unreal Insights trace conclusions until the same scenario has baseline and after metrics, target hardware/editor mode context, `get_performance_stats` or `get_memory_stats` read-back, trace/report paths when used, accepted visual-quality tradeoffs, and clean relevant logs.
- Do not claim a crash, assert, ensure, hang, Blueprint compiler error, UBT/build/cook/package failure, PIE runtime error, asset load failure, or log regression is fixed until exact baseline error text, call stack or crash/log path, failure class, reproduction step, relevant MCP route output, and original-failure retest evidence show the original exact error is absent. Do not clear caches, delete `Intermediate/`, `Saved/`, or `DerivedDataCache`, disable plugins, change engine association, delete assets, or run destructive shell/source-control recovery without explicit approval.
- Do not claim template selection, project setup, default maps/modes, GameMode/GameInstance defaults, starter/sample content, plugin enablement, migrated content, or first-map startup behavior is correct until `get_project_settings` and Content Browser/package read-back, dependency validation, save/open state, restart requirements, logs, and PIE startup behavior have been verified.
- Do not claim project settings, console/Python automation, profiling, packaging, cook, build, deploy, or platform operations succeeded without exact MCP/editor read-back, command output, report/log paths, or runtime evidence; configuration-only inspection does not prove packaging or performance.
- Direct `.uasset` and `.umap` filesystem permission requests are auto-rejected. Local mutation aliases including `edit`, `write`, `apply_patch`, `bash`, `shell`, `command`, `execute`, and `execute_command` are rejected when they target Unreal `Content/`, `/Game`, `/Engine`, `.uproject`, or `Config/*.ini` state; safe source/config/docs/log reads remain available. The generated OpenCode guardrail enforces the same boundary before local tool execution. Destructive local shell/source-control commands such as project-root recursive deletes, `git reset --hard`, `git clean`, and `git checkout --` are blocked. Protected MCP editor mutation tools, including action-scoped `system_control` project operations, require successfully completed `/unreal-tool-inventory` plus `inspect` preflight first in the same OpenCode session, then a complete route card emitted by the assistant. The guardrail accepts route-card evidence only from assistant message events, invalidates stale cards after a fresh inspect, and bounds protected mutation batches by both inspection and route-card freshness. Failed preflight tool results and user-authored route-card text do not unlock mutations. Use `/Game` package paths and project-setting routes through `unreal-engine` MCP tools instead.
- Local mutation guardrails resolve existing path components and reject symbolic links on every guarded request. This is permission-time preflight, not an atomic filesystem open: a hostile same-user process could replace a path after approval but before another executor opens it. Executors used in adversarial concurrent environments must additionally use descriptor-based no-follow APIs.
- Do not hide destructive or bulk operations behind broad prompts.
- Do not spawn project-relative executables or trust `PATH` entries inside the current project.
- Do not block Slate/UI code on ACP process IO.
- Do not move protocol parsing into UI layout code.
- Do not edit generated `Binaries/`, `Intermediate/`, `Saved/`, packaged zips, or temporary host projects.
