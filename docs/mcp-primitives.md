# MCP primitives — resources, prompts, completions, progress

What the server offers beyond `tools`. TypeScript: `src/server/mcp-primitives/` and `src/resources/` (registration in `src/handlers/resource-handlers.ts`). Native `/mcp`: `plugins/.../Private/MCP/Primitives/` and `Private/MCP/Resources/`. There is no automated parity gate between the two; keep them in step by hand.

## Advertised capabilities

Both transports advertise exactly:

```jsonc
{ "tools": {}, "resources": {}, "prompts": {}, "completions": {} }
```

(`SERVER_CAPABILITIES` in `src/server/mcp-primitives/primitive-wiring.ts`; native `McpNativeTransportToolDiscovery.cpp`.) There are no resource subscriptions and no task API.

## Resources

| URI | stdio | native `/mcp` |
| --- | --- | --- |
| `ue://capability/catalog` | yes | yes |
| `ue://project` | yes | yes |
| `ue://state/revisions` | yes | yes |
| `ue://health` | yes | yes |
| `ue://automation-bridge` | yes | yes |
| `ue://assets`, `ue://actors`, `ue://editor`, `ue://selection` | yes | no (`RESOURCE_UNAVAILABLE`) |

Templates (both transports): `ue://capability/{capabilityId}`, `ue://knowledge/{topic}`, `ue://asset/{assetPath}`.

The four live-state URIs are only valid to read on the game thread. stdio serves them by round-tripping through the automation bridge; native answers `resources/read` on the socket thread, where it may not block on editor work, so it neither lists nor serves them (`McpResourceCatalog::IsNativeUnservedUri`, the same list the read classifier uses).

Reads are bounded (64 KiB) and path inputs are normalized and redacted (`src/resources/resource-errors.ts`). Payloads carry a revision; only `ue://capability/catalog` moves (it follows `configure` visibility changes).

## Prompts

`prompts/list` and `prompts/get` serve six workflow prompts: `inspect-fix`, `asset-import`, `level-build`, `blueprint-edit`, `validation`, `sequence-render` (`src/server/mcp-primitives/prompts.ts`; native `McpPromptCatalog`). A prompt returns text messages only; getting one never executes anything.

## Completions

`completion/complete` completes prompt arguments and the three template variables (`capabilityId`, `topic`, `assetPath`). Ranking is prefix > substring > subsequence > one typo; results are capped at 100 values and 8 KiB, prefixes over 128 characters return nothing, and host paths or traversal prefixes are refused.

## Progress

A client that sends `_meta.progressToken` on `tools/call` receives `notifications/progress` for that request, correlated from Unreal's own progress events. The token is read, never allocated: no token, no progress. The stream ends before the result is sent, and on cancellation.
