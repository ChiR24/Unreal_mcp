// Task 37 — RED STARTUP fail-closed contract for the primitive registration
// PARITY seam. This suite is a sibling of primitive-wiring.test.ts but
// guards a DIFFERENT gate: not the end-to-end protocol behaviour over a
// transport, but the pre-connect, typed *registration-table validator* that
// Task 37 must introduce so the advertised capability surface can never diverge
// from the handlers that actually back it.
//
// WHY THIS SEAM MUST EXIST (proven, not assumed — see the GUARD block below):
// the MCP SDK's `Server.assertRequestHandlerCapability` (server/index.js) guards
// `completion/complete`, `prompts/*`, `resources/list|read|templates`,
// `tools/*`, and `tasks/*` — but has NO case for `resources/subscribe` or
// `resources/unsubscribe`. So a server may advertise `resources.subscribe: true`,
// register no subscribe handler at all, connect happily, and only fail at RUNTIME
// with -32601 when a client subscribes. Task 37 closes that gap with a
// fail-closed `PrimitiveRegistry` that refuses to construct — BEFORE connect —
// when an advertised capability lacks its backing handler(s).
//
// RED DISCIPLINE:
//  * The production module does NOT exist yet, so there is NO static import of
//    it (a static import would be an import-resolution COLLECTION error). The
//    module is loaded through an absence-tolerant dynamic import that turns a
//    missing module into a precise per-test failure — collection always succeeds.
//  * The contract constants are a HARD-CODED independent oracle, identical in
//    shape to the wiring suite's oracle, so a future bug can never make the suite
//    pass by echoing production's own data back at it.
//  * No fake production implementation lives in this file: the validator, the
//    derivation, and the error class are all loaded from the REAL (future)
//    module. Today they are absent, so every RED case fails on the absent seam;
//    when Task 37 lands the module at the canonical path with the exact public
//    contract below, this suite turns GREEN unchanged.
//
// CANONICAL FUTURE MODULE (Task 37 must create it, primary candidate first):
//   src/server/mcp-primitives/primitive-registry.ts
// exporting:
//   - class PrimitiveRegistrationError extends Error
//       { name: 'PrimitiveRegistrationError'; code: 'PRIMITIVE_HANDLER_MISSING';
//         capability: string; method: string }
//   - createPrimitiveRegistry(input: { handlers: ReadonlyMap<string, Handler>,
//         capabilities: ServerCapabilities }): { capabilities: ServerCapabilities }
//       — throws PrimitiveRegistrationError when an advertised capability lacks a
//         required handler in the table (fail-closed, pre-connect).
//   - deriveAdvertisedCapabilities(handlers: ReadonlyMap<string, Handler>):
//         ServerCapabilities
//       — a COMPLETE table derives EXACTLY { tools, resources.subscribe, prompts,
//         completions } and nothing else (no tasks, no logging, no listChanged).

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import {
    CompleteRequestSchema,
    ListResourcesRequestSchema,
    ListToolsRequestSchema,
    SubscribeRequestSchema,
    UnsubscribeRequestSchema,
    type ServerCapabilities,
} from '@modelcontextprotocol/sdk/types.js';
import { describe, expect, it } from 'vitest';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

// --- Contract constants (independent, hard-coded oracle) ---

/** The exact server capability surface a COMPLETE primitive table must derive. */
const EXPECTED_SERVER_CAPABILITIES = {
    tools: {},
    resources: { subscribe: true },
    prompts: {},
    completions: {},
} satisfies ServerCapabilities;

/**
 * Every MCP request method whose handler the PrimitiveRegistry governs. This is
 * exactly the set that backs the four advertised capabilities above: two tools
 * methods, three read-only resource methods, the subscribe/unsubscribe pair, the
 * two prompt methods, and completion. A complete handler table carries all ten.
 */
const COMPLETE_PRIMITIVE_METHODS = [
    'tools/list',
    'tools/call',
    'resources/list',
    'resources/templates/list',
    'resources/read',
    'resources/subscribe',
    'resources/unsubscribe',
    'prompts/list',
    'prompts/get',
    'completion/complete',
] as const;

// The handler table is TEST INPUT (a map of method -> no-op handler), never a
// fake validator: the code under test consumes it, it does not implement it.
type ProbeHandler = (request: unknown, extra: unknown) => unknown;
const probeHandler: ProbeHandler = () => ({});

/** A full method->handler table matching EXPECTED_SERVER_CAPABILITIES exactly. */
function completeHandlerTable(): Map<string, ProbeHandler> {
    return new Map(COMPLETE_PRIMITIVE_METHODS.map((method) => [method, probeHandler]));
}

/** The complete table with exactly one required method omitted (builder, not a re-check). */
function tableWithout(method: string): Map<string, ProbeHandler> {
    const table = completeHandlerTable();
    table.delete(method);
    return table;
}

// --- Absence-tolerant loader for the (future) production module ---

// Ordered candidate specifiers; the first is the canonical contract path. The
// specifier is a runtime variable (never a literal in the import call), so Vite
// cannot statically resolve it at transform time and collection never fails on
// the missing module.
const REGISTRY_MODULE_CANDIDATES = [
    '../../../src/server/mcp-primitives/primitive-registry.js',
    '../../../src/server/mcp-primitives/registration/primitive-registry.js',
    '../../../src/server/primitive-registry.js',
] as const;

// null = not yet attempted; undefined = attempted and absent; record = loaded.
let cachedModule: Record<string, unknown> | undefined | null = null;

/** True only for module-resolution failures, so real evaluation errors surface. */
function looksAbsent(error: unknown): boolean {
    const code = isRecord(error) ? error.code : undefined;
    if (code === 'ERR_MODULE_NOT_FOUND') {
        return true;
    }
    const message = isRecord(error) && typeof error.message === 'string' ? error.message : '';
    return /cannot find module|failed to load|could not resolve|no such file|does the file exist|err_module_not_found/i.test(
        message,
    );
}

async function loadRegistryModule(): Promise<Record<string, unknown> | undefined> {
    if (cachedModule !== null) {
        return cachedModule;
    }
    for (const candidate of REGISTRY_MODULE_CANDIDATES) {
        try {
            const loaded: unknown = await import(/* @vite-ignore */ candidate);
            if (isRecord(loaded)) {
                cachedModule = loaded;
                return cachedModule;
            }
        } catch (error) {
            if (!looksAbsent(error)) {
                throw error;
            }
        }
    }
    cachedModule = undefined;
    return cachedModule;
}

/** Resolve the module or fail the current RED case with a precise, non-import error. */
async function requireRegistryModule(): Promise<Record<string, unknown>> {
    const mod = await loadRegistryModule();
    if (mod === undefined) {
        throw new Error(
            `Task 37 PrimitiveRegistry seam absent: none of [${REGISTRY_MODULE_CANDIDATES.join(', ')}] resolved. ` +
                'Expected the canonical module src/server/mcp-primitives/primitive-registry.ts to export ' +
                'createPrimitiveRegistry, deriveAdvertisedCapabilities, and PrimitiveRegistrationError. ' +
                'RED until Task 37 implements the typed registration-table validator.',
        );
    }
    return mod;
}

// Invoke exports of the dynamically-loaded module. Each narrows the export with a
// runtime typeof guard and returns `unknown`, so no static type is fabricated for
// a module that does not exist at compile time.
function invokeDerive(mod: Record<string, unknown>, handlers: unknown): unknown {
    const fn = mod.deriveAdvertisedCapabilities;
    if (typeof fn !== 'function') {
        throw new Error('Task 37 seam incomplete: deriveAdvertisedCapabilities is not an exported function.');
    }
    return fn(handlers);
}

function invokeCreate(mod: Record<string, unknown>, input: unknown): unknown {
    const fn = mod.createPrimitiveRegistry;
    if (typeof fn !== 'function') {
        throw new Error('Task 37 seam incomplete: createPrimitiveRegistry is not an exported function.');
    }
    return fn(input);
}

// --- Baseline guards: pass TODAY and must remain true after Task 37 lands ---
// They pin the SDK behaviour that justifies the seam, and the oracle's shape.
// None of them touch the (absent) production module, so they are stable GREEN.

describe('Task 37 — startup parity seam guards (SDK behaviour + oracle)', () => {
    it('[GUARD] SDK advertises resources.subscribe with NO subscribe handler registered (the unguarded seam)', async () => {
        const server = new Server(
            { name: 'task-37-seam-probe', version: '1.0.0' },
            { capabilities: { tools: {}, resources: { subscribe: true } } },
        );
        // A realistically half-wired server: the list/read handlers are present
        // (their registration is asserted by the SDK, so it must not throw)...
        server.setRequestHandler(ListResourcesRequestSchema, () => Promise.resolve({ resources: [] }));
        server.setRequestHandler(ListToolsRequestSchema, () => Promise.resolve({ tools: [] }));
        // ...but the resources/subscribe and resources/unsubscribe handlers are
        // deliberately absent. The SDK never checks they exist.
        const client = new Client({ name: 'task-37-seam-client', version: '1.0.0' }, { capabilities: {} });
        const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
        await server.connect(serverTransport);
        await client.connect(clientTransport, { timeout: 15000 });
        try {
            // The client still observes subscribe:true: an advertised-but-unbacked
            // capability that only fails at runtime with -32601. That divergence is
            // exactly what the Task 37 startup registry must refuse pre-connect.
            expect(client.getServerCapabilities()).toEqual({ tools: {}, resources: { subscribe: true } });
        } finally {
            await clientTransport.close();
        }
    });

    it('[GUARD] SDK asserts completion/complete parity but NOT resources/subscribe (gap is real and seam-specific)', () => {
        const withoutCompletions = new Server(
            { name: 'task-37-seam-probe', version: '1.0.0' },
            { capabilities: { tools: {} } },
        );
        // The SDK DOES guard completions: registering without the capability throws pre-connect.
        expect(() =>
            withoutCompletions.setRequestHandler(CompleteRequestSchema, () =>
                Promise.resolve({ completion: { values: [] } }),
            ),
        ).toThrow(/completion/i);

        const withoutResources = new Server(
            { name: 'task-37-seam-probe', version: '1.0.0' },
            { capabilities: {} },
        );
        // The SDK does NOT guard the subscribe pair: registering them without any
        // resources capability never throws — the exact seam Task 37 must close.
        expect(() => withoutResources.setRequestHandler(SubscribeRequestSchema, () => Promise.resolve({}))).not.toThrow();
        expect(() =>
            withoutResources.setRequestHandler(UnsubscribeRequestSchema, () => Promise.resolve({})),
        ).not.toThrow();
    });

    it('[GUARD] the capability oracle is exactly {tools, resources.subscribe, prompts, completions} — no tasks/logging/listChanged', () => {
        expect(Object.keys(EXPECTED_SERVER_CAPABILITIES).sort()).toEqual([
            'completions',
            'prompts',
            'resources',
            'tools',
        ]);
        expect(EXPECTED_SERVER_CAPABILITIES.resources).toEqual({ subscribe: true });
        expect(EXPECTED_SERVER_CAPABILITIES.tools).toEqual({});
        expect(EXPECTED_SERVER_CAPABILITIES.prompts).toEqual({});
        expect(EXPECTED_SERVER_CAPABILITIES.completions).toEqual({});
        expect(Object.hasOwn(EXPECTED_SERVER_CAPABILITIES, 'tasks')).toBe(false);
        expect(Object.hasOwn(EXPECTED_SERVER_CAPABILITIES, 'logging')).toBe(false);
        expect(Object.hasOwn(EXPECTED_SERVER_CAPABILITIES.tools, 'listChanged')).toBe(false);
        expect(Object.hasOwn(EXPECTED_SERVER_CAPABILITIES.resources, 'listChanged')).toBe(false);
    });
});

// --- RED: fail TODAY on the absent seam; GREEN when Task 37 lands the module ---

describe('Task 37 — PrimitiveRegistry public surface (RED until implemented)', () => {
    it('exports createPrimitiveRegistry, deriveAdvertisedCapabilities, and PrimitiveRegistrationError', async () => {
        const mod = await requireRegistryModule();
        expect(typeof mod.createPrimitiveRegistry).toBe('function');
        expect(typeof mod.deriveAdvertisedCapabilities).toBe('function');
        expect(typeof mod.PrimitiveRegistrationError).toBe('function');
    });
});

describe('Task 37 — capability derivation from a complete handler table (RED until implemented)', () => {
    it('deriveAdvertisedCapabilities(complete table) derives exactly the oracle surface', async () => {
        const mod = await requireRegistryModule();
        // When: a COMPLETE method->handler table is fed to the derivation.
        const caps = invokeDerive(mod, completeHandlerTable());
        // Then: exactly {tools, resources.subscribe, prompts, completions}...
        expect(caps).toEqual(EXPECTED_SERVER_CAPABILITIES);
        // ...and never tasks, logging, or any listChanged member.
        const record = isRecord(caps) ? caps : {};
        expect(Object.hasOwn(record, 'tasks')).toBe(false);
        expect(Object.hasOwn(record, 'logging')).toBe(false);
        expect(Object.hasOwn(isRecord(record.tools) ? record.tools : {}, 'listChanged')).toBe(false);
        expect(Object.hasOwn(isRecord(record.resources) ? record.resources : {}, 'listChanged')).toBe(false);
    });
});

describe('Task 37 — createPrimitiveRegistry fail-closed parity (RED until implemented)', () => {
    it('accepts a complete table matching its advertised capabilities and derives the oracle surface', async () => {
        const mod = await requireRegistryModule();
        // When: the factory receives a handler table plus advertised capabilities
        // that are fully backed.
        const registry = invokeCreate(mod, {
            handlers: completeHandlerTable(),
            capabilities: EXPECTED_SERVER_CAPABILITIES,
        });
        // Then: it constructs and exposes exactly the validated oracle surface.
        const record = isRecord(registry) ? registry : {};
        expect(record.capabilities).toEqual(EXPECTED_SERVER_CAPABILITIES);
    });

    it('deleting the resources/subscribe handler throws PrimitiveRegistrationError before connect (SDK-unasserted seam)', async () => {
        const mod = await requireRegistryModule();
        // When: subscribe is advertised but its handler is removed from the table.
        let caught: unknown;
        try {
            invokeCreate(mod, {
                handlers: tableWithout('resources/subscribe'),
                capabilities: EXPECTED_SERVER_CAPABILITIES,
            });
        } catch (error) {
            caught = error;
        }
        // Then: construction fails closed with the stable code and the exact method,
        // proving the registry guards the very seam the SDK leaves open.
        expect(caught).toBeDefined();
        const record = isRecord(caught) ? caught : {};
        expect(record.name).toBe('PrimitiveRegistrationError');
        expect(record.code).toBe('PRIMITIVE_HANDLER_MISSING');
        expect(record.method).toBe('resources/subscribe');
        const ctor = mod.PrimitiveRegistrationError;
        if (typeof ctor === 'function') {
            expect(caught instanceof ctor).toBe(true);
        }
    });

    it('deleting the resources/unsubscribe handler throws PRIMITIVE_HANDLER_MISSING for resources/unsubscribe', async () => {
        const mod = await requireRegistryModule();
        let caught: unknown;
        try {
            invokeCreate(mod, {
                handlers: tableWithout('resources/unsubscribe'),
                capabilities: EXPECTED_SERVER_CAPABILITIES,
            });
        } catch (error) {
            caught = error;
        }
        expect(caught).toBeDefined();
        const record = isRecord(caught) ? caught : {};
        expect(record.code).toBe('PRIMITIVE_HANDLER_MISSING');
        expect(record.method).toBe('resources/unsubscribe');
    });

    it('deleting the prompts/get handler throws PRIMITIVE_HANDLER_MISSING (the guard is uniform, not subscribe-only)', async () => {
        const mod = await requireRegistryModule();
        let caught: unknown;
        try {
            invokeCreate(mod, {
                handlers: tableWithout('prompts/get'),
                capabilities: EXPECTED_SERVER_CAPABILITIES,
            });
        } catch (error) {
            caught = error;
        }
        expect(caught).toBeDefined();
        const record = isRecord(caught) ? caught : {};
        expect(record.code).toBe('PRIMITIVE_HANDLER_MISSING');
        expect(record.method).toBe('prompts/get');
    });
});
