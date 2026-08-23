# Security model, scopes, consent, receipts and errors

Every automation request is gated **before it reaches the editor queue**. The
TypeScript layer fails fast; the plugin re-enforces independently.

> **The plugin is the sole authority.** A TypeScript-side check is a
> convenience, never a proof. Do not design a client on the assumption that
> passing the TS gate means the plugin will accept the request.

## Network posture

Both surfaces bind loopback-first.

| Surface | Non-loopback opt-in | Notes |
| --- | --- | --- |
| Plugin WebSocket listener | `bAllowNonLoopback` | plugin project setting |
| Plugin native `/mcp` transport | `bAllowNonLoopback` **and** `bRequireCapabilityToken` | the transport **refuses to bind** non-loopback without the token requirement |
| TypeScript stdio bridge | `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` + `MCP_AUTOMATION_HOST=0.0.0.0` | the bridge is a WebSocket *client*, not a second server |
| Prometheus metrics | `MCP_METRICS_ALLOW_NON_LOOPBACK=true` **and** `MCP_METRICS_TOKEN` | a separate surface with its own gate |

The native transport's fail-closed coupling means a LAN-exposed native surface
can never start unauthenticated. The other surfaces have no such interlock.

> **Loopback default-allow is a real exposure.** Once any surface is reachable
> from the LAN without a capability token, any client on that network can call
> any capability. Enable token auth before exposing anything.

Capability tokens arrive as the `X-MCP-Capability-Token` header (native MCP) or
`bridge_hello.capabilityToken` (WebSocket). Both transports compare them in
**constant time** (`McpConstantTimeTokenEquals` in
`Private/Foundation/McpSecureTokenCompare.h`), so comparison time never leaks
how much of a token matched.

## Scopes

Four scopes (`Public/McpCapabilityScopes.h`): `Read`, `Write`, `Destructive`,
`Admin`.

**Membership is exact-set with an `Admin` wildcard — it is NOT rank-based.**
Holding `Write` does **not** imply `Read`. A principal must hold precisely the
scope a capability declares.

A capability that does not resolve in the canonical catalogue demands `Admin`,
so an unknown action is refused by default.

Each capability's required scope is published per-row in
[`action-reference.generated.md`](action-reference.generated.md).

### Predicates vs composition

Pure, side-effect-free predicates live in
`Private/Foundation/McpCapabilityAuthorization.h`. They depend only on
Foundation, are shared by **both** transports, and are reproducible in a
no-editor test.

Composition with the canonical catalogue, the console-command policy and the
quota ledger happens one layer up in `Private/Core/Security/`. The predicate
header must not reach into `Domains/` or `MCP/`.

## Consent

Three modes (`CONSENT_MODES`): `none`, `explicit`, `elevated`. The mode a
capability declares is published per-row in the action reference.

Consent arrives as an **execute-envelope sibling — never a handler parameter**:

```json
{
  "operation": "execute",
  "tool": "manage_asset",
  "action": "delete_asset",
  "params": { "assetPath": "/Game/Old/mesh" },
  "consent": { "capability": "manage_asset.delete_asset", "acknowledge": "explicit" }
}
```

A grant is honored **only when it names that capability**. Consent is
re-validated plugin-side.

> Never infer consent from loopback, from a prior call, from an idempotency
> key, or from a preview. None of those are consent.

## Path gating

The permitted asset roots are `/Game`, `/Engine`, `/Script`, `/Temp`,
`/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES` entries.

The gate **scans request values, not an allowlist of key names**, canonicalizing
first so it sees the same string the handler will resolve. The `/Content` alias
is mapped in one shared canonicalizer.

The scan is depth- and node-bounded and reports `bTruncated` honestly. **A
truncated scan proves nothing, so it fails closed.**

`CheckPathCoverage()` closes what value scanning structurally cannot see —
folder/name joins, an omitted optional path parameter that hits a server-side
default, bare-relative values. A path-restricted principal running a mutating
capability that declares a path parameter must present at least one provable
in-prefix target from a scan that **ran to completion**.

## Scoped tokens and quota

`FMcpScopedCapabilityToken` (`Public/McpCapabilityScopes.h`, configured through
`McpAutomationBridgeSettings.h#ScopedCapabilityTokens`) carries a profile,
scopes, allowed path prefixes, allowed projects, and per-minute request and
tool-call quotas.

- A scoped token may list only `Read`, `Write`, `Destructive` — **never
  `Admin`**.
- A scoped token colliding with the legacy token **wins**, because it is
  narrower.
- The secret never appears in a log line, receipt, authority descriptor,
  principal identity, or evidence file.

## Refusal codes

These six strings are shared verbatim by both surfaces — each one is present in
both `src/` and the plugin source, so parity holds by construction. Add a code
to both surfaces or to neither.

| Code | Meaning |
| --- | --- |
| `SCOPE_NOT_GRANTED` | the principal does not hold the capability's exact required scope |
| `CONSENT_REQUIRED` | the capability declares `explicit`/`elevated` and no matching envelope grant was supplied |
| `PATH_NOT_PERMITTED` | a target path fell outside the permitted prefixes, or coverage could not be proven |
| `PROJECT_NOT_PERMITTED` | the request targeted a project outside the token's allowed set |
| `QUOTA_EXCEEDED` | the principal's per-minute request or tool-call quota was exhausted |
| `COMMAND_BLOCKED` | a console command matched the block policy |

Console commands are filtered by `src/utils/commands/command-validator.ts` on
the TypeScript side and by the generated native policy on the plugin side.

## Execute idempotency

A principal-scoped ledger, mirrored on both surfaces with **different caps**:

| Surface | Implementation | Cap |
| --- | --- | --- |
| TypeScript | `src/server/gateway/idempotency-ledger.ts` | 1024 entries |
| Native | `Private/Foundation/McpIdempotencyLedger.{h,cpp}` | 4096 entries |

Default TTL is 24 hours on both. Changing one without the other makes them
diverge.

Properties that are load-bearing, not incidental:

- **The raw idempotency key never enters the map.** The slot is
  `SHA-256(principal ‖ capabilityId ‖ key)`, so the key cannot reach a log line,
  a receipt, or an evidence file.
- **A failure is never cached.** `abandon` deletes the entry so the key stays
  retryable, and a refusal — which never reaches `begin` — can never be replayed
  as a success.
- **Eviction removes only COMPLETED entries** (`evictCompletedOverCap`).
  Evicting an in-flight entry would admit a concurrent duplicate as a second
  real mutation, which is the exact thing the ledger exists to prevent.
- A key replayed with a different request fingerprint is a `conflict` that
  discloses no prior receipt.

## Native execute stage order

The native validation stage order is normative
(`Private/MCP/Execute/McpNativeGatewayValidation.h`) and matches the TypeScript
reference:

1. resolve request form and alias
2. registry lookup
3. dynamic enabled state
4. options
5. declared defaults
6. the exact per-action input schema

**Nothing reaches the subsystem queue until every stage passes.**

On the way out, `McpProjectCanonicalOutput` keeps only the capability's declared
output properties, so a real success payload can never violate its closed output
schema; a violation returns an error receipt that preserves the raw handler
payload as structured detail rather than discarding what Unreal reported.

## Editor safety

Editor API work enters through the subsystem queue and runs on the game thread;
unsafe save/GC/async-load states are deferred
(`ProcessPendingAutomationRequests()`, 16 requests per tick).

Hazardous operations go through the wrappers in `Private/Safety/`
(`McpSafeAssetSave`, `McpSafeLevelSave`, `McpSafeLoadMap`). Direct
`UPackage::SavePackage()` calls are forbidden and a source-contract test fails
the build if one appears.

## Known security posture gaps

Two gaps are carried openly rather than closed, because closing either one
requires a decision this project is not authorized to take alone. Both are
recorded in `.omo/evidence/task-64-pure-unreal-mcp-implementation.json`. The
section below additionally records the advisory fixed in 0.5.30 so its
remediation stays on the record.

### A shipped dependency carries an advisory

`npm audit --audit-level=moderate` exits **1** against this tree: 7 advisories,
2 moderate and 5 high.

| Advisory | Path | Reaches users |
| --- | --- | --- |
| `GHSA-frvp-7c67-39w9` — path traversal in `serve-static` on Windows via an encoded backslash (`%5C`) | production: `@modelcontextprotocol/sdk` (pinned at exactly 1.29.0) → `@hono/node-server` | **yes** |
| `GHSA-mh99-v99m-4gvg` — unbounded expansion in `brace-expansion` | dev only: the ESLint `minimatch` chain | no |

The 5 high-severity findings are all the ESLint development chain and are never
installed by a consumer of this package. The moderate one is different: it sits
on the production path, so it ships in the bytes a user installs. **Its
exploitability in this product has not been assessed** — no lane audited
whether the vulnerable `serve-static` route is reachable here, and absence of
an assessment is not evidence of safety.

Clearing it requires moving off the pinned SDK version, which is a breaking
dependency change. That decision is not taken here.

### Advisory GHSA-x982-3jx2-x6q3 — loopback WS → Admin → `execute_python` / `console_command` RCE (patched 0.5.30)

A crafty LAN client with loopback access to the WebSocket bridge could invoke an
`Admin`-scoped action — `execute_python` or `console_command` — without presenting
a capability token. Because loopback is the default binding and no token was
required by default, any user on the same machine could reach the bridge and
escalate to arbitrary Python execution or console command invocation inside the
editor process.

**Fix (0.5.30):** `bRequireCapabilityToken` is now **on by default**. A 32-byte
random token (64 lowercase hex chars) is auto-generated by the plugin on first
use and persisted at `<ProjectRoot>/Saved/MCP/capability-token`. The TypeScript
stdio bridge reads the same file at handshake time via `UE_PROJECT_PATH`, so the
classic Option B setup requires zero extra configuration. A manually configured
`CapabilityToken` in Project Settings wins over the generated file. Existing
projects that never touched the setting are automatically protected; projects
that explicitly disabled `Require Capability Token` retain the opt-out (and the
documented risk).

**Token rotation:** delete `<Project>/Saved/MCP/capability-token` and restart the
editor — a new token is generated. The same effect is achieved by clearing
`CapabilityToken` in Project Settings and restarting.

Rotation pickup differs by transport. The WebSocket bridge resolves the token
once at connection-manager initialization, so it honors restart-scoped rotation
(delete + restart). The native `/mcp` transport re-resolves on every request, so
it picks up a rotated file mid-session; until the editor restarts, WebSocket
clients authenticated under the previous token are refused (fail closed).

The token file is created with the editor's default file permissions and is
readable by the editor's operating-system user, which matches the trusted
boundary documented in the plugin README (a process already able to mutate that
user's project files is outside the transport's remote-client threat model).
Tighten directory ACLs if `Saved/` sits on a shared location.

Security semantics are otherwise unchanged: constant-time comparison
(`McpConstantTimeTokenEquals`), fail-closed LAN coupling (native `/mcp` refuses
non-loopback without token auth enabled), loopback-first binding,
`bEnableNativeMCP` off by default. The token value is never logged, never
appears in health or telemetry bodies, and is never emitted in a receipt.

### The plugin authorization stack was not exercised end-to-end live

Scope, consent and revision refusals are exercised as corpus shapes and through
the TypeScript envelope. They are **not** driven end-to-end through the
plugin's own authorization stack against a running editor, because that lane is
blocked on engine availability — see
[`performance-and-evidence.md`](performance-and-evidence.md).

Native accept/reject parity is likewise decided against a *mirror* of the
plugin gate — its generated header parsed at run time, plus a source contract
on `IsBlockedCommand` — not against the compiled binary. Treat the refusal-code
parity above as a contract-level guarantee, not as a live one.
