# src/tools

Canonical TypeScript contracts and routing for the 23 public parent tools. Definitions describe the MCP surface; orchestration selects a domain handler; handlers validate and send bridge requests to Unreal.

## STRUCTURE
```
tools/
|-- catalog/          # definition aggregate, action-set exports, schema augmentation
|-- definitions/      # core/world/gameplay/utility contracts and shared action sets
|-- dynamic/          # enable/disable state by parent tool and category
|-- orchestration/    # call normalization, registry, routing sets, registration, dispatch
|-- handlers/         # domain action implementations; see nested AGENTS
|-- schemas/          # legacy/shared core schema fragments
|-- editor/           # editor facade modules
|-- environment/      # environment facade tests/support
`-- level/            # level facade and operations
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Change the canonical tool list | `definitions/shared/all-tool-definitions.ts` | Exactly 23 definitions, in public list order |
| Change a tool contract | `definitions/<category>/`, `catalog/consolidated-tool-definitions.ts` | Aggregate copies definitions, then adds `actionParams` schemas |
| Register parent handlers | `orchestration/consolidated-handler-registration.ts` | `registerDefaultHandlers()` owns all 23 `toolRegistry.register()` calls |
| Change action-to-domain routing | `orchestration/consolidated-routing.ts` | Shared action sets and graph-action translations |
| Change call dispatch/errors | `orchestration/consolidated-handler-dispatcher.ts`, `consolidated-call-utils.ts` | Normalize call, resolve registry handler, preserve tool/action context |
| Bootstrap/export orchestration | `orchestration/consolidated-tool-handlers.ts` | Side-effect registration plus public exports; no routing logic |
| Enable/disable tools | `dynamic/` | Categories: `core`, `world`, `gameplay`, `utility`, `all` |
| Implement an action | `handlers/<domain>/` | Follow `handlers/AGENTS.md` |

## CANONICAL SURFACE
- Core: `manage_tools`, `manage_asset`, `manage_blueprint`, `control_actor`, `control_editor`, `manage_project_settings`, `manage_level`, `system_control`, `inspect`.
- World: `build_environment`, `manage_geometry`, `manage_pcg`, `manage_level_structure`.
- Gameplay: `animation_physics`, `manage_effect`, `manage_gas`, `manage_character`, `manage_combat`, `manage_ai`, `manage_inventory`, `manage_interaction`.
- Utility: `manage_sequence`, `manage_audio`, `manage_networking`.

## CONVENTIONS
- Parent names above are the public MCP surface. Do not expose routed child domains such as material, graph, lighting, session, or input handlers as new tools.
- A new parent requires a definition in `all-tool-definitions.ts` and a matching registration in `consolidated-handler-registration.ts`.
- Keep action strings aligned across the definition/action sets, routing predicates, handler switches, C++ bridge registration, native MCP schema, and tests.
- `manage_tools` and `inspect` are protected; preserve local TS dynamic-tool behavior and native parity.
- Keep output schemas schema-backed. Use `unknown`, narrow at boundaries, and preserve structured error context.

## ANTI-PATTERNS
- Adding routing logic to the `consolidated-tool-handlers.ts` bootstrap facade.
- Calling domain handlers outside the registry/`handleConsolidatedToolCall()` path.
- Adding schema-only actions without TS handling, C++ handling, and targeted tests.
- Sending paths or console commands around shared validation, or writing runtime `console.log` output.

## OWNERSHIP
- `src/server/AGENTS.md` owns MCP SDK list/call registration and the local `manage_tools` intercept.
- `handlers/AGENTS.md` owns handler signatures, normalization, dispatch, and tests.
