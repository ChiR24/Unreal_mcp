// Task 28 — runtime catalog state revision rules.
//
// Two independently runnable groups:
//   npx vitest run src/tools/dynamic/catalog-state-revision.test.ts -t 'Task 28 baseline'
//   npx vitest run src/tools/dynamic/catalog-state-revision.test.ts -t 'Task 28 desired'
//
// Decisions (.omo/notepads/pure-unreal-mcp-implementation/decisions.md, 2026-07-20):
// the generated `catalogRevision` string stays an immutable contract
// fingerprint. `catalogStateRevision` is a NEW monotonic runtime counter that
// tracks visibility state: it moves exactly once per effective mutation batch
// and never for a no-op or for a rejected protected mutation. Task 28 exposes
// it on configure/manage-tools status only — never on search/describe/execute.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { searchGatewayCatalog } from '../../server/tool-registry-gateway.js';
import { handleManageToolsCall } from '../../server/tool-registry-manage-tools.js';
import { compareAscii } from '../../utils/serialization/ordering.js';
import { isRecord } from '../../utils/validation/type-guards.js';
import { CATALOG_REVISION } from '../catalog/capabilities/generated/canonical-registry.generated.js';
import { dynamicToolManager } from './dynamic-tool-manager.js';

beforeEach(() => {
    dynamicToolManager.reset();
});

afterEach(() => {
    dynamicToolManager.reset();
});

function status(): Record<string, unknown> {
    const snapshot: unknown = dynamicToolManager.getStatus();
    return isRecord(snapshot) ? snapshot : {};
}

// Baseline comparisons must survive a correct Task 28 implementation, so this
// picks the stable functional fields explicitly rather than serializing the
// whole status: an effective reset legitimately moves `catalogStateRevision`,
// and a whole-status diff would report that as lost functional state.
function functionalState(): string {
    const { totalTools, enabledTools, disabledTools, categories } = dynamicToolManager.getStatus();
    const tools = dynamicToolManager.listTools().map((tool) => ({
        name: tool.name,
        category: tool.category,
        enabled: dynamicToolManager.isToolEnabled(tool.name)
    }));
    tools.sort((left, right) => compareAscii(left.name, right.name));
    return JSON.stringify({ totalTools, enabledTools, disabledTools, categories, tools });
}

// Reads the counter the Task 28 implementation must add. The explicit type
// assertion is load-bearing: without it every delta comparison below would
// reduce to NaN vs NaN, which `Object.is` reports as equal, so the RED cases
// would pass vacuously while the field is still missing.
function stateRevision(): number {
    const value = status().catalogStateRevision;
    expect(typeof value).toBe('number');
    return typeof value === 'number' ? value : Number.NaN;
}

describe('Task 28 baseline — dynamic tool state rules Task 28 must preserve', () => {
    it('starts fully enabled and reset restores the exact functional state', () => {
        const pristine = functionalState();
        expect(dynamicToolManager.getStatus().disabledTools).toBe(0);

        dynamicToolManager.disableCategory('gameplay');
        expect(dynamicToolManager.getStatus().disabledTools).toBeGreaterThan(0);
        expect(functionalState()).not.toBe(pristine);

        dynamicToolManager.reset();
        expect(functionalState()).toBe(pristine);
    });

    it('refuses to disable protected tools and the core category', () => {
        const result = dynamicToolManager.disableTools(['manage_tools', 'inspect']);
        expect(result.disabled).toEqual([]);
        expect(result.protected).toEqual(expect.arrayContaining(['manage_tools', 'inspect']));

        dynamicToolManager.disableCategory('core');
        expect(dynamicToolManager.isToolEnabled('manage_tools')).toBe(true);
        expect(dynamicToolManager.isToolEnabled('inspect')).toBe(true);
    });

    it('treats enabling an already-enabled tool as a no-op that changes nothing', () => {
        const pristine = functionalState();
        expect(dynamicToolManager.isToolEnabled('manage_asset')).toBe(true);

        const result = dynamicToolManager.enableTools(['manage_asset']);
        expect(result.notFound).toEqual([]);
        expect(dynamicToolManager.isToolEnabled('manage_asset')).toBe(true);
        expect(functionalState()).toBe(pristine);
    });

    it('keeps the generated catalogRevision immutable across runtime mutation', () => {
        expect(CATALOG_REVISION).toMatch(/^[0-9a-f]{16}$/);
        const before = CATALOG_REVISION;

        dynamicToolManager.disableCategory('gameplay');
        dynamicToolManager.disableTools(['manage_asset']);

        expect(CATALOG_REVISION).toBe(before);
        const searched = searchGatewayCatalog({ query: 'import asset' });
        expect(isRecord(searched) ? searched.catalogRevision : undefined).toBe(CATALOG_REVISION);
    });
});

describe('Task 28 desired — catalogStateRevision runtime counter', () => {
    it('exposes a numeric state revision distinct from the immutable catalogRevision', () => {
        const snapshot = status();
        expect(typeof snapshot.catalogStateRevision).toBe('number');
        expect(snapshot.catalogStateRevision).not.toBe(CATALOG_REVISION);
    });

    it('starts deterministically at 0 on a freshly loaded catalog', async () => {
        // A fresh module generation is the only order-independent way to observe
        // the start value of a monotonic counter.
        vi.resetModules();
        const fresh = await import('./dynamic-tool-manager.js');
        const snapshot: unknown = fresh.dynamicToolManager.getStatus();
        expect(isRecord(snapshot) ? snapshot.catalogStateRevision : undefined).toBe(0);
    });

    it('increments exactly once per effective mutation batch', () => {
        const start = stateRevision();

        // One batch disabling two tools must count as one revision, not two.
        dynamicToolManager.disableTools(['manage_asset', 'manage_level']);
        expect(stateRevision()).toBe(start + 1);

        dynamicToolManager.disableCategory('gameplay');
        expect(stateRevision()).toBe(start + 2);
    });

    it('does not increment for no-op enable, no-op disable, or no-op reset', () => {
        const start = stateRevision();

        dynamicToolManager.enableTools(['manage_asset']); // already enabled
        dynamicToolManager.reset(); // already pristine
        expect(stateRevision()).toBe(start);

        dynamicToolManager.disableTools(['manage_asset']); // effective
        const afterEffective = stateRevision();
        expect(afterEffective).toBe(start + 1);

        dynamicToolManager.disableTools(['manage_asset']); // repeat, already disabled
        expect(stateRevision()).toBe(afterEffective);
    });

    it('does not increment for rejected protected mutations', () => {
        const start = stateRevision();

        dynamicToolManager.disableTools(['manage_tools', 'inspect']);
        dynamicToolManager.disableCategory('core');

        expect(stateRevision()).toBe(start);
    });

    it('increments once when reset actually restores disabled tools', () => {
        dynamicToolManager.disableCategory('gameplay');
        const beforeReset = stateRevision();

        dynamicToolManager.reset();

        expect(stateRevision()).toBe(beforeReset + 1);
    });

    it('manage-tools get_status reports both catalog revisions', async () => {
        const result = await handleManageToolsCall({ action: 'get_status' });
        expect(result.success).toBe(true);
        expect(result.catalogRevision).toBe(CATALOG_REVISION);
        expect(typeof result.catalogStateRevision).toBe('number');
    });

    it('keeps the state counter out of the search envelope', () => {
        dynamicToolManager.disableCategory('gameplay');
        const searched = searchGatewayCatalog({ query: 'import asset' });

        expect(isRecord(searched)).toBe(true);
        if (!isRecord(searched)) return;
        expect(searched.catalogRevision).toBe(CATALOG_REVISION);
        expect(Object.hasOwn(searched, 'catalogStateRevision')).toBe(false);
    });

    it('Given all categories disabled When disableCategory("all") Then the 8 core tools stay enabled and revision advances once', () => {
        const CORE_TOOLS = [
            'control_actor',
            'control_editor',
            'inspect',
            'manage_asset',
            'manage_blueprint',
            'manage_level',
            'manage_tools',
            'system_control'
        ];

        const start = stateRevision();

        const result = dynamicToolManager.disableCategory('all');

        for (const name of CORE_TOOLS) {
            expect(dynamicToolManager.isToolEnabled(name)).toBe(true);
        }
        expect(result.protected).toEqual(expect.arrayContaining(CORE_TOOLS));

        expect(dynamicToolManager.getStatus().disabledTools).toBeGreaterThan(0);
        const someNonCoreDisabled = dynamicToolManager
            .listTools()
            .some((tool) => tool.category !== 'core' && !dynamicToolManager.isToolEnabled(tool.name));
        expect(someNonCoreDisabled).toBe(true);

        const coreState = dynamicToolManager.getStatus().categories.find((cat) => cat.name === 'core');
        expect(coreState?.enabledCount).toBe(CORE_TOOLS.length);

        expect(stateRevision()).toBe(start + 1);
    });
});
