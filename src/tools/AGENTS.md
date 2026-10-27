# src/tools

Contracts for the 23 **internal canonical parent tools**. The public surface is the single `unreal` gateway tool (`search`/`describe`/`execute`/`configure`), served by the plugin; the parents are reachable only through it. Their contracts live in capability records, which generate the plugin's `MCP/Generated/` shards. There is no TypeScript runtime behind them: execute runs `{ action, ...params }` on the record's parent tool inside the plugin.

## STRUCTURE
```
tools/
|-- catalog/
|   `-- capabilities/
|       |-- records/                 # CANONICAL CONTRACT SOURCE (per-parent dirs, parent-metadata.ts, folds/, aggregate.ts)
|       `-- generated/               # canonical registry, cost index, parent definitions (never hand-edit)
`-- definitions/shared/tool-definition.ts
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Change a contract (actions, schemas, cost, policy) | `catalog/capabilities/records/<parent>/`, then `npm run registry:generate` |
| Parent name/category/description | `catalog/capabilities/records/parent-metadata.ts` |
| Fold a family into one record | `catalog/capabilities/records/folds/<parent>.folds.ts` |
| Execute routing | `plugins/EngineWire/Source/EngineWire/Private/MCP/Execute/` |
| Enable/disable tools | `plugins/EngineWire/Source/EngineWire/Private/MCP/DynamicTools/` (categories `core`, `world`, `gameplay`, `utility`) |

## CANONICAL SURFACE (23 internal parents)
- Core: `manage_tools`, `manage_asset`, `manage_blueprint`, `control_actor`, `control_editor`, `manage_level`, `system_control`, `inspect`.
- World: `build_environment`, `manage_geometry`, `manage_pcg`, `manage_level_structure`.
- Gameplay: `animation_physics`, `manage_effect`, `manage_gas`, `manage_character`, `manage_combat`, `manage_ai`, `manage_inventory`, `manage_interaction`.
- Utility: `manage_sequence`, `manage_audio`, `manage_networking`.

## CONVENTIONS
- Records are the single source of truth; everything under `generated/` and the native registry shards derive from them. Run `registry:check` and `manifest:check` after a record change.
- A record declares only the params its C++ handler reads and the outputs it emits: the gateway validates strictly, so an undeclared param is refused (`UNDECLARED_PARAMETER`) and a declared-but-unread one is a lie.
- A folded family lists once in the parent's action enum; each member stays callable by its own `{tool, action}` pair.
- `manage_tools` and `inspect` are protected from disabling.

## ANTI-PATTERNS
- Hand-editing anything under `generated/`.
- A schema-only action with no C++ handler behind it.
