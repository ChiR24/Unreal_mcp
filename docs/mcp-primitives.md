# MCP primitives — resources, prompts, completions, progress

What the server offers beyond `tools`, all served by the plugin's native `/mcp`: `plugins/.../Private/MCP/Primitives/` and `Private/MCP/Resources/`. The stdio proxy relays them unchanged.

## Advertised capabilities

```jsonc
{ "tools": {}, "resources": { "subscribe": true }, "prompts": {}, "completions": {} }
```

(native `McpNativeTransportToolDiscovery.cpp`.) `ue://changes` can be subscribed to: `subscriptions/listen` (2026-07-28) holds one SSE answer open, and `resources/subscribe` (earlier versions) sends updates on the session's GET stream (`Protocol/McpNativeTransportListen.cpp`). The stdio proxy opens no GET stream, so it leaves `subscribe` out of what it advertises.

## Resources

| URI | served |
| --- | --- |
| `ue://capability/catalog` | yes |
| `ue://project` | yes |
| `ue://state/revisions` | yes |
| `ue://health` | yes |
| `ue://automation-bridge` | yes |
| `ue://changes` | yes: where the change journal stands (`{cursor}`); read what changed with `inspect changes_since` |
| `ue://assets`, `ue://actors`, `ue://editor`, `ue://selection` | no (`RESOURCE_UNAVAILABLE`) |

Templates: `ue://capability/{capabilityId}`, `ue://knowledge/{topic}`, `ue://asset/{assetPath}`.

The four live-state URIs are only valid to read on the game thread. Native answers `resources/read` on the socket thread, where it may not block on editor work, so it neither lists nor serves them (`McpResourceCatalog::IsNativeUnservedUri`, the same list the read classifier uses). The same data comes from tools: `control_actor` find, `manage_asset` list, `control_editor` and `inspect`.

Reads are bounded (64 KiB). Payloads carry a revision; only `ue://capability/catalog` moves (it follows `configure` visibility changes).

## Prompts

`prompts/list` and `prompts/get` serve six workflow prompts (`inspect-fix`, `asset-import`, `level-build`, `blueprint-edit`, `validation`, `sequence-render`; `McpPromptCatalog`) and nine domain playbooks (`blueprint-logic`, `input-setup`, `widget-ui`, `niagara-vfx`, `material-authoring`, `world-building`, `animation-setup`, `cinematics`, `play-testing`; `McpPromptPlaybooks`): the capabilities to reach for in order, and the traps of the domain (what goes wrong without a word, and how to see it). A prompt returns text messages only; getting one never executes anything. A unit test holds every step to a capability the catalog has.

## Completions

`completion/complete` completes prompt arguments and the three template variables (`capabilityId`, `topic`, `assetPath`). Ranking is prefix > substring > subsequence > one typo; results are capped at 100 values and 8 KiB, prefixes over 128 characters return nothing, and host paths or traversal prefixes are refused.

## Progress

A client that sends `_meta.progressToken` on `tools/call` receives `notifications/progress` for that request, correlated from Unreal's own progress events. The token is read, never allocated: no token, no progress. The stream ends before the result is sent, and on cancellation.
