# `src/server/mcp-primitives/` — MCP Protocol Primitives

43 files: 16 at top level + `completions/` (12), `prompts/` (9), `subscriptions/` (6). Everything MCP exposes **besides tools**: resources, prompts, completions, subscriptions, plus client profiles and the session `configure` store.

## Wiring order (top level)

`primitive-wiring.ts` — **ENTRY POINT**. `wirePrimitives(server): WiredPrimitives`. Call this; do not hand-register a primitive elsewhere.
`primitive-sources.ts` — dependency builders: `buildCatalogRevisionReader()`, `buildEnabledCapabilityProvider()`, `buildPromptReferenceValidator()`.
`primitive-handlers.ts` — `registerPrimitiveHandlers(deps)` returns the method set it actually backed. `REGISTERED_PRIMITIVE_METHODS` = `resources/subscribe`, `resources/unsubscribe`, `prompts/list`, `prompts/get`, `completion/complete`.
`primitive-registry.ts` — `ADVERTISED_SESSION_CAPABILITIES`, `deriveAdvertisedCapabilities(handlers)`, `PrimitiveRegistrationError`. Advertised capabilities are **derived from handlers actually registered**, never hardcoded — that is what keeps `initialize` honest.
`primitive-notifications.ts` — `PrimitiveNotificationDriver` over a `NotifyingServer`.

## State + profiles

`resource-revision.ts` — branded `ResourceRevision`, `INITIAL_REVISION = 1`, `nextRevision()`, and `SUBSCRIBABLE_URIS` (`ue://capability/catalog`, `ue://project`, `ue://level`, `ue://selection`, `ue://asset-registry`, `ue://pie`, `ue://build`, `ue://render`, `ue://logs`). `ue://editor` is deliberately **absent** — it tracks `ue://pie`.
`catalog-revision-reader.ts` — `BASELINE_CATALOG_STATE_REVISION = 0`, `BASELINE_CATALOG_REVISION_READER`.
`session-capability-profile.ts` — `SessionCapabilityProfile`, `parseClientCapabilityProfile()`, `MINIMAL_PROFILE`.
`client-profile-store.ts` / `fallback-pointers.ts` — adaptive per-client profiles. `FALLBACK_PRIMITIVES` has **5** entries; `SERVER_BACKED_PRIMITIVES` has **4** (`tasks` is listed as a fallback but is NOT server-backed). `fallbackPointerFor()` answers what to tell a client whose profile lacks a primitive.
`session-configure-store.ts` — backs the gateway `configure` verb: session-scoped, revisioned tool-visibility overlay. `LIMIT_BOUNDS`, `MAX_PREFERENCE_KEYS = 16`, `MAX_PREFERENCE_VALUE_LENGTH = 256`.

## Subdirectory barrels

All three `index.ts` barrels add **no behavior and no import side effects** — keep them that way.
- `completions/` — `complete()`, `rankCandidates`/`applyBudget`, `COMPLETION_SLOTS`, `classifyUnsafe`. Bounded by `MAX_COMPLETION_ITEMS`, `MAX_COMPLETION_BYTES`, `MAX_PREFIX_LENGTH`; over-long prefixes return a `COMPLETION_GUIDANCE_CODES` reason, not a truncated guess.
- `prompts/` — `listPrompts()`, `getPrompt()`, `getWorkflowPrompt()`, `WORKFLOW_PROMPTS`. Pure catalog returning text messages. **Getting a prompt never executes anything**; bounded by `MAX_PROMPT_BYTES` / `MAX_ARGUMENT_LENGTH`, rooted at `PROMPT_CONTENT_ROOTS`, failing with typed `PromptError`.
- `subscriptions/` — `SubscriptionStore` (`DEFAULT_MAX_SUBSCRIPTIONS_PER_SESSION`) + `NotificationCoalescer` (`DEFAULT_COALESCE_WINDOW_MS`, `RESOURCE_CHANGE_KINDS`).

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Add a primitive method | `primitive-handlers.ts` + `REGISTERED_PRIMITIVE_METHODS` | Then confirm `deriveAdvertisedCapabilities()` reports it |
| Change what `initialize` advertises | `primitive-registry.ts` | Derived from registered handlers — fix the handler, not the advert |
| Make a URI subscribable | `resource-revision.ts` `SUBSCRIBABLE_URIS` + `subscriptions/` | Bump the revision on every mutation or clients never refetch |
| Change `configure` behavior | `session-configure-store.ts` | Session-scoped and revisioned; see `src/server/gateway/` for the verb |
| Read the data behind a resource | `src/resources/` | Providers live there; this directory is the protocol layer |

## ANTI-PATTERNS

- **Native parity is enforced.** `plugins/.../Private/MCP/Primitives/` (20 files) and `Private/MCP/Resources/` mirror this directory. Changing a primitive here without the native side fails `tests/unit/task-38/*-parity.test.ts` — the fixtures (`*-native-fixture.ts`, `*-native-oracle.json`) encode the expected native answer.
- Never advertise a capability whose handler was not registered — derive it, do not hardcode it.
- Never execute a tool from a prompt path. `prompts/get` returns messages only.
- Never emit unbounded completion/prompt payloads; the `MAX_*` caps are the contract, and exceeding one must return typed guidance rather than a truncated result.
- Do not mutate state from a resource read or a subscribe call.
- Do not add side effects or re-exported behavior to the three `index.ts` barrels.
- Do not treat `ue://editor` as independently subscribable — it follows `ue://pie`.
