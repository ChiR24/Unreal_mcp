# Security model, scopes, consent, receipts and errors

Every automation request is gated **before it reaches the editor queue**. The
TypeScript layer fails fast; the plugin re-enforces independently.

> **The plugin is the sole authority.** A TypeScript-side check is a
> convenience, never a proof. Do not design a client on the assumption that
> passing the TS gate means the plugin will accept the request.

## Network posture

Both plugin listeners bind loopback-first. The npm package's stdio proxy opens
no socket of its own: it is an HTTP client of the native `/mcp`.

| Surface | Non-loopback opt-in | Notes |
| --- | --- | --- |
| Plugin WebSocket listener | `bAllowNonLoopback`, `bRequireCapabilityToken` **and** `bEnableTls` | **refuses to bind** non-loopback without the token requirement, and without TLS: the token travels in `bridge_hello` |
| Plugin native `/mcp` transport | none | **never binds** non-loopback: it is plain HTTP, so a token sent over a network is readable by anyone on the path. A remote client comes in through an SSH tunnel (`ssh -L 3000:127.0.0.1:3000`) or a TLS proxy that forwards to loopback |
| stdio proxy (`UNREAL_MCP_URL`) | `UNREAL_MCP_ALLOW_REMOTE_TOKEN=1` | sends the capability token only to a loopback upstream unless this is set |

The fail-closed coupling means a LAN-exposed surface can never start
unauthenticated, and no token crosses a network in clear text.

The WebSocket listener accepts at most 32 clients; a client that stops reading
is disconnected after a 2 s write stall instead of holding the game thread, and
a heartbeat timeout closes only the socket that missed it. A connection whose
token authority changed (a settings edit, a token file replaced) must say
`bridge_hello` again (`REAUTHENTICATION_REQUIRED`); a `requestId` still in
flight is refused (`REQUEST_ID_IN_USE`), and a cancel names only a request that
socket sent. Payload keys starting with `_` belong to the server (a handler's
own retry state) and are dropped at that door.

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

A parameter can demand more as well (`policy.raisedBy`): sending it adds that
scope to the capability's own. `sample_motion`'s `inputs` press keys (Write);
a screenshot's `path` and `export_mesh`'s `outputPath` pick where a file is
replaced (Write; the default folders hold only captures and exports). Inside a
folded family a name raises every sibling, so `set_view_target` checks its own
case in the handler: `location` or `rotation` move the target actor, which a
read-only caller is refused (`SCOPE_NOT_GRANTED`).

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
re-validated plugin-side. A failed request gives its nonce back only when the
change journal shows nothing changed since its dispatch began; a handler that
changed something and then failed spent it.

> Never infer consent from loopback, from a prior call, from an idempotency
> key, or from a preview. None of those are consent.

## Path gating

The permitted asset roots are `/Game`, `/Engine`, `/Script`, `/Temp`,
`/Niagara`, the content mounts the connected editor reports
(`bridge_ack.contentRoots`, kept current by `content_roots_changed`), plus
sanitized `MCP_ADDITIONAL_PATH_PREFIXES` entries. A key the plugin opens as a
file on disk keeps the static roots plus `/Saved` and `/tmp`, and never gets the
editor-reported mounts; a `/`-rooted path starting with two separators
(`//host/share`, `/\host\share`) is refused.

The gate **scans request values, not an allowlist of key names**, canonicalizing
first so it sees the same string the handler will resolve. The `/Content` alias
is mapped in one shared canonicalizer.

The scan is depth- and node-bounded and reports `bTruncated` honestly. **A
truncated scan proves nothing, so it fails closed.**

`CheckPathCoverage()` closes what value scanning structurally cannot see —
folder/name joins, an omitted optional path parameter that hits a server-side
default, bare-relative values. A path-restricted principal running a mutating
capability that declares a path parameter must present at least one provable
in-prefix target from a scan that **ran to completion**, and names every path
in full: a relative one under a path key is refused, not read as `/Game/...`.

The gate sees what a request names; a handler can still reach other packages
(a default folder, an actor by label, a reference, a dependent). So the request's
principal stays active while its handler runs
(`McpCapabilityPrincipal::FActiveRequestScope`, a latent reply included), and
the shared sinks ask it before they act: the asset save, level save and delete
wrappers, the reflection target check (`McpSafeReflectionTarget::IsAddressable`,
every object-reference hop of a property walk too), new-asset package creation,
and the actor finders, searches and listings, which leave out what lies outside
the prefixes. An actor counts as its level's map package. An action whose effect
spreads past what it names refuses before changing anything when the spread
leaves the prefixes (`recompile_struct` with a user of the struct elsewhere),
and `save_all` leaves another prefix's dirty packages unsaved, counted but not
named.

## Scoped tokens and quota

`FMcpScopedCapabilityToken` (`Public/McpCapabilityScopes.h`, configured through
`EngineWireSettings.h#ScopedCapabilityTokens`) carries a profile,
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

A principal-scoped ledger in the plugin, `Private/Foundation/McpIdempotencyLedger.{h,cpp}`
(4096 entries, default TTL 24 hours).

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

## Dependency advisory posture

`npm audit --omit=dev` is a blocking gate in CI at `--audit-level=moderate`, so
a moderate-or-worse advisory on the production dependency tree fails the build.
That gate, not this page, is the current answer to "does an advisory reach
shipped code" — its verdict changes whenever the advisory database does, which
is why no count is written down here.

The full-tree audit (`npm audit --audit-level=moderate`, dev dependencies
included) runs alongside it as `continue-on-error`. Dev-tree advisories are
reported, never gated: that tooling is not installed by a consumer of this
package. Both live in the `dependency-audit` job rather than inside `lint`,
so an advisory published upstream cannot take the deterministic gates red with
it.

This section previously recorded `GHSA-frvp-7c67-39w9` — path traversal in
`serve-static` on Windows via an encoded backslash (`%5C`) — on the production
path through `@modelcontextprotocol/sdk` (pinned at exactly 1.29.0) →
`@hono/node-server`, with its exploitability in this product explicitly
unassessed, and the blocking gate narrowed to `high` to tolerate it. It no
longer reports against this lockfile; the SDK pin is unchanged, so the resolved
transitive versions are what moved, not this project's direct dependencies. The
gate is back at `moderate`.

## Known security posture gaps

Gaps are carried openly rather than closed when closing one requires a decision
this project is not authorized to take alone. They are recorded in
`.omo/evidence/task-64-pure-unreal-mcp-implementation.json`. Remediated
advisories are recorded below rather than deleted, so the remediation stays on
the record.

### Advisory GHSA-x982-3jx2-x6q3 — loopback WS → Admin → `execute_python` / `console_command` RCE (patched 0.5.30)

A crafty LAN client with loopback access to the WebSocket bridge could invoke an
`Admin`-scoped action — `execute_python` or `console_command` — without presenting
a capability token. Because loopback is the default binding and no token was
required by default, any user on the same machine could reach the bridge and
escalate to arbitrary Python execution or console command invocation inside the
editor process.

**Fix (0.5.30):** `bRequireCapabilityToken` is now **on by default**. A 32-byte
random token (64 lowercase hex chars) is auto-generated by the plugin on first
use and persisted at `<ProjectRoot>/Saved/MCP/capability-token`. The stdio
proxy reads the same file on every request via `UE_PROJECT_PATH`, so the stdio
setup requires zero extra configuration. A manually configured
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
(`McpConstantTimeTokenEquals`), fail-closed LAN posture (native `/mcp` never
binds non-loopback; the WebSocket bridge only with token auth and TLS), loopback-first binding, and a
browser `Origin` the endpoint does not allow is refused. `bEnableNativeMCP` is on
by default, because the npm package is now only a stdio proxy in front of
`/mcp`. The token value is never logged, never
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
