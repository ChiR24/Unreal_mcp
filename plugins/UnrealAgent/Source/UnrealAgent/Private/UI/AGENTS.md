# Private/UI

Slate panel for `Window > Unreal Agent`. This subtree owns the visible chat surface: header, composer, model/agent menus, permission bar, transcript, quick prompts, sidebar/history, and context-window indicator.

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Build panel layout | `SUnrealAgentPanel.cpp` | `Construct`, cockpit, sidebar/composer/menu factories, quick prompts |
| Public test hooks | `SUnrealAgentPanel.h` | Automation-only transcript/sidebar/context helpers |
| ACP callback wiring | `SUnrealAgentPanel.cpp` | Status, transcript, permission, models changed, stopped |
| Transcript rendering | `SUnrealAgentPanel.cpp` | Markdown-ish tables, activity groups, streaming rows |
| Model/agent menus | `SUnrealAgentPanel.cpp` | Provider grouping, search, selection callbacks |

## UI CONVENTIONS
- Stable widget tags prefixed `UnrealAgent.*` are the Slate automation contract. Update `UnrealAgentAutomationTests.cpp` with any tag/layout change.
- `Construct()` owns ACP client creation and delegate binding; keep protocol behavior in `Private/Acp`.
- Transcript caps: `200` entries, `20000` chars per entry, flush interval `0.05s`.
- Conversation roles: `OpenCode`, `User`/`You`, `Thought`, `Tool`, `Permission`, `Plan`, `Error`.
- `System` rows are status/diagnostic messages, not normal transcript rows.
- Stream rows append for `OpenCode`, `User`, and `Thought`; tool/permission/plan rows group under working/activity UI.
- Model/agent controls are visible only after ACP readiness and option discovery.
- Quick prompts should reinforce the production workflow: MCP capability discovery, current-state inspection, prototype-to-release planning, and verification. They must still avoid claiming live editor state unless MCP tooling is explicitly configured for the ACP session.
- Context-window status prefers exact OpenCode ACP `usage_update` token usage when available, then falls back to local model/transcript estimates.
- The cockpit row owns prompt context toggling, context refresh, Studio Kit status, validation, and evidence status. Keep long details in tooltips/transcript instead of expanding the row.
- Prompt context attachment is enabled by default; toggling it only changes prompt payloads, not visible user transcript text.

## ANTI-PATTERNS
- Removing or renaming widget tags without updating automation tests.
- Moving protocol parsing, process IO, or executable resolution into Slate layout code.
- Claiming live editor selection, viewport, or level state from UI state alone.
- Rendering raw JSON-RPC frames, normal process noise, or hidden tool/status events as chat rows.
- Blocking `Tick()` with process waits or long formatting work.
- Removing `WITH_DEV_AUTOMATION_TESTS` helpers without replacing the coverage.
- Putting Studio Kit generation, context capture, or validation logic directly in Slate layout code; call the ACP helpers instead.
