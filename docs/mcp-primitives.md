# MCP primitives — resources, prompts, completions, subscriptions, tasks, fallbacks

What the server advertises beyond `tools`, what each primitive actually answers,
and what a client that lacks a primitive gets instead.

TypeScript implementation lives in `src/server/mcp-primitives/`. The native
mirror lives in `plugins/.../Private/MCP/Primitives/`. Parity between them is
gated by `tests/unit/task-38/*-parity.test.ts`.

## Advertised capabilities

The server advertises exactly these capabilities at `initialize`
(`ADVERTISED_SESSION_CAPABILITIES` in
`src/server/mcp-primitives/primitive-registry.ts`, wired in
`src/server/server-factory.ts`):

```jsonc
{
  "tools": {},
  "resources": { "subscribe": true },
  "prompts": {},
  "completions": {},
  "tasks": { "list": {}, "cancel": {}, "requests": { "tools": { "call": {} } } }
}
```

Advertisement is **derived, not asserted**: `primitive-registry.ts` downgrades
`resources` to `{}` unless every subscribe/unsubscribe handler is actually
registered. A capability this server advertises has a handler behind it; that
coupling is the point of the module.

Registered primitive methods (`REGISTERED_PRIMITIVE_METHODS` in
`src/server/mcp-primitives/primitive-handlers.ts`):
`resources/subscribe`, `resources/unsubscribe`, `prompts/list`, `prompts/get`,
`completion/complete`. The `tasks/*` methods are registered by the MCP SDK
against the task store (below) rather than hand-wired here.

## Resources

`resources/list` and `resources/templates/list` answer from
`src/server/mcp-primitives/primitive-sources.ts`. Each entry carries a full
`{uri, name, description, mimeType}`.

Static resources include `ue://capability/catalog`, `ue://project`,
`ue://editor`, `ue://selection` and `ue://state/revisions`.

Four read-only templates are published:

| Template | Resolves |
| --- | --- |
| `ue://capability/{capabilityId}` | one capability record |
| `ue://knowledge/{engineVersion}/{topic}` | engine knowledge topic |
| `ue://object/{objectPath}` | an object reference |
| `ue://asset/{assetPath}` | an asset reference |

Reads are bounded and revisioned. `ue://capability/catalog` carries its revision
*inside* the returned text rather than as a top-level field — asserted by
`tests/unit/task-38/resources-baseline.test.ts`.

### The two transports advertise different resource sets, on purpose

The stdio transport advertises and serves eleven static resources. The native
`/mcp` transport advertises and serves **six**: it omits `ue://assets`,
`ue://actors`, `ue://level`, `ue://editor` and `ue://selection`.

Those five are live editor state — the asset registry, a world actor iteration,
the open map, PIE status and the editor selection — and are only valid to read on
the game thread. stdio can serve them because it is a separate process that
round-trips each read through the automation bridge; the native transport answers
`resources/read` on the socket thread, where it may neither block on editor work
nor serve a transport-thread cache of state that is stale by the time it is read.

The set native advertises is therefore exactly the set it can read, and it is
built by one filter shared with the read classifier
(`McpResourceCatalog::AllListedResources` / `IsNativeUnservedUri`) so the two
cannot drift. Asking native for one of the five returns a typed
`RESOURCE_UNAVAILABLE` naming the stdio transport as the surface that serves it.

Both transports answer the base-protocol `ping` method with an empty result.

The resource list is **profile-independent**: a minimal-capability client
observes the identical list (same test file). Resource listing does not shrink
to flatter clients.

## Subscriptions

Nine URIs are subscribable (`SUBSCRIBABLE_URIS` in
`src/server/mcp-primitives/resource-revision.ts`):

`ue://capability/catalog`, `ue://project`, `ue://level`, `ue://selection`,
`ue://asset-registry`, `ue://pie`, `ue://build`, `ue://render`, `ue://logs`.

`SubscriptionStore` (`subscriptions/subscription-store.ts`, native mirror
`McpSubscriptionStore.{h,cpp}`) holds pure per-session state — no timers, no
transport, no revisions. Its guarantees:

- Each session owns an **independent, insertion-ordered** set. One session's
  subscriptions can never be read or drained as another's.
- A URI outside the allowlist is rejected `NOT_SUBSCRIBABLE`; a blank session id
  is rejected `INVALID_SESSION`. Neither mutates any state.
- A duplicate subscribe is idempotent.
- At the per-session cap (default 9) a new subscription **deterministically
  evicts the oldest**, firing the release hook for it.
- `clearSession()` releases every URI so delegates and pending work drain to
  zero.

Revision tracking and coalesced notification live one layer up in
`subscriptions/notification-coalescer.ts`, not in the store.

## Prompts

`prompts/list` and `prompts/get` serve six workflow prompts
(`src/server/mcp-primitives/prompts/workflow-prompts.ts`):

`inspect-fix`, `asset-import`, `level-build`, `blueprint-edit`, `validation`,
`sequence-render`.

Prompt argument and lookup errors map to typed MCP errors through
`prompts/prompt-errors.ts` rather than leaking raw exceptions.

## Completions

`completion/complete` is served by
`src/server/mcp-primitives/completions/completion-provider.ts` over declared
slots (`completion-slots.ts`) with deterministic ranking
(`completion-ranking.ts`). Cross-transport agreement is pinned by
`completions/completion-parity.fixture.json`.

## Tasks

MCP Tasks (`2025-11-25`) are backed by `bounded-task-store.ts`, which implements
the MCP SDK's own `TaskStore` interface. That choice is deliberate: passing it
to `new Server(info, { taskStore })` makes the SDK auto-register the real
`tasks/get | list | cancel | result` handlers, so the store is reachable from
the wire rather than only from a unit test.

It deliberately does **not** reuse the SDK's `InMemoryTaskStore`, which is
unbounded, drives expiry from real `setTimeout` timers, and ignores `sessionId`
entirely. The three properties this server needs and that store lacks:

1. a hard cap, so a client cannot grow server memory without limit;
2. an injectable clock, so expiry is provable without sleeping;
3. session isolation, so one session cannot read, cancel, or evict another's
   task.

Native mirror: `Private/MCP/Primitives/McpTaskStore.{h,cpp}`. The two must agree
on cap semantics, eviction order, TTL clamping, and the terminal-state rule
(`completed`, `failed`, `cancelled` are unleavable).

> `docs/protocol.md` states that `execution.taskSupport` is not advertised as
> required or optional. That remains accurate: the server advertises the
> `tasks` **capability** with real handlers behind it, which is a different
> field from `execution.taskSupport`. Clients must not read one as the other.

## Fallbacks for clients missing a primitive

A session that lacks a primitive receives exactly **one** bounded, executable
pointer — never a schema or knowledge dump
(`src/server/mcp-primitives/fallback-pointers.ts`, native mirror
`McpSessionCapabilityProfile.h#McpFallbackPointerFor`).

Five primitives participate: `resources`, `prompts`, `completions`,
`subscriptions`, `tasks`. All five are **server-backed**, meaning the server
registers a real method for each:

| Primitive | Native method a capable client is pointed at |
| --- | --- |
| `resources` | `resources/list` |
| `prompts` | `prompts/list` |
| `completions` | `completion/complete` |
| `subscriptions` | `resources/subscribe` |
| `tasks` | `tasks/list` |

A client that **declares** the primitive is pointed at the native method
(`mode: "native"`). A client that does not is pointed at a single bounded
`unreal` gateway operation instead (`mode: "gateway"`).

Pointing a Tasks-declaring client at `tasks/list` is only safe because the
bounded task store made that method answer instead of returning `-32601`;
`tasks` joined `SERVER_BACKED_PRIMITIVES` at that point and not before.

## Client profiles

`session-capability-profile.ts` derives a **structural** profile from the
client's declared capabilities, not from its name or version. Two clients with
different name/version but identical capabilities resolve to the same profile —
asserted by `tests/unit/server/task-37-primitive-wiring.test.ts`. The profile is
surfaced through `configure`'s `get_status` result, hoisted to the gateway
envelope top level so a caller reads it without unwrapping `result`.

## What transport parity does and does not cover

Parity between the TypeScript and native surfaces is proven at the **contract**
level: `npm run test:native-parity` exits 0 over 23 native canonical tools with
0 action mismatches and 0 schema property mismatches, and the primitive mirrors
are gated by `tests/unit/task-38/*-parity.test.ts`.

> **The native runtime `describe` surface has never been successfully
> censused.** Two probe runs were attempted. The first was paging-limited (19
> of 23 tools pinned at exactly 20 names). The second fixed paging but still
> harvested dispatch-group names and tool names into the action list on 5 of 23
> tools, and carried non-canonical alias names on its declared side. Both runs
> measured the probe, not the product.

The consequence is symmetric and is stated in both directions: there is **no**
trustworthy measurement of what the native surface names at run time, so this
project claims neither that the two surfaces diverge nor that they agree. What
is proven is the contract-level parity above, plus the fact that two actions
the probe reported as missing were executed successfully on the native surface.

Building a trustworthy runtime census is an open item recorded in
`.omo/evidence/task-64-pure-unreal-mcp-implementation.json`. Until it exists, do
not cite either surface's runtime name set as verified.
