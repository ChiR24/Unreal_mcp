// Task 37 — the startup fail-closed contract for the primitive registration
// PARITY seam. Sibling of primitive-wiring.test.ts, but a DIFFERENT gate: not
// end-to-end protocol behaviour over a transport, but the pre-connect, typed
// registration-table validator that keeps the advertised capability surface
// from diverging from the handlers that actually back it.
//
// WHY THIS SEAM EXISTS (proven, not assumed — see the GUARD block below): the
// MCP SDK's `Server.assertRequestHandlerCapability` (server/index.js) guards
// `completion/complete`, `prompts/*`, `resources/list|read|templates`,
// `tools/*` and `tasks/*` — but has NO case for `resources/subscribe` or
// `resources/unsubscribe`. So a server may advertise `resources.subscribe:
// true`, register no subscribe handler at all, connect happily, and only fail
// at RUNTIME with -32601 when a client subscribes. `PrimitiveRegistry` closes
// that gap by refusing to construct — BEFORE connect — when an advertised
// capability lacks its backing handler(s).
//
// The contract constants below are a HARD-CODED independent oracle, so a bug
// can never make the suite pass by echoing production's own data back at it.
//
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
import {
    createPrimitiveRegistry,
    deriveAdvertisedCapabilities,
    PrimitiveRegistrationError,
} from '../../../src/server/mcp-primitives/primitive-registry.js';
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

// --- Baseline guards ---
// They pin the SDK behaviour that justifies the seam, and the oracle's shape,
// without touching the registry itself.

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

describe('Task 37 — PrimitiveRegistry public surface', () => {
    it('exports createPrimitiveRegistry, deriveAdvertisedCapabilities, and PrimitiveRegistrationError', () => {
        expect(typeof createPrimitiveRegistry).toBe('function');
        expect(typeof deriveAdvertisedCapabilities).toBe('function');
        expect(typeof PrimitiveRegistrationError).toBe('function');
    });
});

describe('Task 37 — capability derivation from a complete handler table', () => {
    it('deriveAdvertisedCapabilities(complete table) derives exactly the oracle surface', () => {
        // When: a COMPLETE method->handler table is fed to the derivation.
        const caps = deriveAdvertisedCapabilities(completeHandlerTable());
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

describe('Task 37 — createPrimitiveRegistry fail-closed parity', () => {
    it('accepts a complete table matching its advertised capabilities and derives the oracle surface', () => {
        // When: the factory receives a handler table plus advertised capabilities
        // that are fully backed.
        const registry = createPrimitiveRegistry({
            handlers: completeHandlerTable(),
            capabilities: EXPECTED_SERVER_CAPABILITIES,
        });
        // Then: it constructs and exposes exactly the validated oracle surface.
        expect(registry.capabilities).toEqual(EXPECTED_SERVER_CAPABILITIES);
    });

    it('deleting the resources/subscribe handler throws PrimitiveRegistrationError before connect (SDK-unasserted seam)', () => {
        // When: subscribe is advertised but its handler is removed from the table.
        let caught: unknown;
        try {
            createPrimitiveRegistry({
                handlers: tableWithout('resources/subscribe'),
                capabilities: EXPECTED_SERVER_CAPABILITIES,
            });
        } catch (error) {
            caught = error;
        }
        // Then: construction fails closed with the stable code and the exact method,
        // proving the registry guards the very seam the SDK leaves open.
        expect(caught).toBeInstanceOf(PrimitiveRegistrationError);
        const record = isRecord(caught) ? caught : {};
        expect(record.name).toBe('PrimitiveRegistrationError');
        expect(record.code).toBe('PRIMITIVE_HANDLER_MISSING');
        expect(record.method).toBe('resources/subscribe');
    });

    it('deleting the resources/unsubscribe handler throws PRIMITIVE_HANDLER_MISSING for resources/unsubscribe', () => {
        let caught: unknown;
        try {
            createPrimitiveRegistry({
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

    it('deleting the prompts/get handler throws PRIMITIVE_HANDLER_MISSING (the guard is uniform, not subscribe-only)', () => {
        let caught: unknown;
        try {
            createPrimitiveRegistry({
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
