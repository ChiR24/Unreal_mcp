// Task 36 — explicit-session, policy-bounded, revisioned configure overlay.
//
// RED-first: this suite is written before session-configure-store.ts and the
// manage-tools session seam exist, so it fails to import until Task 36 lands.
// It proves per-session independence of visibility/preferences/limits, the exact
// revision rules (+1 per effective visibility batch; never for a no-op, a
// rejected protected mutation, or a limit/preference-only change), protected/core
// invariants, clearSession/reseed, and that the global manager stays an
// untouched immutable seed on both the store and the injected manage-tools seam.

import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { CATALOG_REVISION } from '../../tools/catalog/capabilities/generated/canonical-registry.generated.js';
import { dynamicToolManager } from '../../tools/dynamic/dynamic-tool-manager.js';
import {
	clearManageToolsSession,
	handleManageToolsCall,
	resetManageToolsSessionResolver,
	STDIO_SESSION_ID,
	sessionConfigureStore,
	setManageToolsSessionResolver,
} from '../tool-registry-manage-tools.js';
import {
	BASELINE_CATALOG_STATE_REVISION,
	type CatalogRevisionReader,
} from './catalog-revision-reader.js';
import { SessionConfigureStore } from './session-configure-store.js';

const A = 'session-A';
const B = 'session-B';

beforeEach(() => {
	dynamicToolManager.reset();
	resetManageToolsSessionResolver();
});

afterEach(() => {
	dynamicToolManager.reset();
	resetManageToolsSessionResolver();
	for (const id of [A, B, 'session-X', 'session-Y']) {
		clearManageToolsSession(id);
	}
});

describe('SessionConfigureStore — per-session overlay over an immutable seed', () => {
	it('satisfies the C1 CatalogRevisionReader contract, baseline for an unknown session', () => {
		const store: CatalogRevisionReader = new SessionConfigureStore();
		expect(typeof store.getCatalogStateRevision).toBe('function');
		expect(store.getCatalogStateRevision('never-seen')).toBe(
			BASELINE_CATALOG_STATE_REVISION,
		);
	});

	it('seeds each fresh session fully enabled at revision 0', () => {
		const store = new SessionConfigureStore();
		const status = store.getStatus(A);
		expect(status.disabledTools).toBe(0);
		expect(status.catalogStateRevision).toBe(0);
		expect(store.isToolEnabled(A, 'manage_ai')).toBe(true);
	});

	it("isolates one session's visibility mutation from another", () => {
		const store = new SessionConfigureStore();
		store.disableCategory(A, 'gameplay');

		expect(store.isToolEnabled(A, 'manage_ai')).toBe(false);
		// B never seeded a mutation: it stays pristine and at the baseline revision.
		expect(store.isToolEnabled(B, 'manage_ai')).toBe(true);
		expect(store.getCatalogStateRevision(A)).toBe(1);
		expect(store.getCatalogStateRevision(B)).toBe(0);
	});

	it("advances each session's revision exactly once per effective batch, independently", () => {
		const store = new SessionConfigureStore();

		store.disableTools(A, ['manage_asset', 'manage_level']); // one batch, two tools
		expect(store.getCatalogStateRevision(A)).toBe(1);
		store.disableCategory(A, 'gameplay');
		expect(store.getCatalogStateRevision(A)).toBe(2);

		store.disableCategory(B, 'gameplay');
		expect(store.getCatalogStateRevision(B)).toBe(1);
		expect(store.getCatalogStateRevision(A)).toBe(2);
	});

	it('never advances the revision for a no-op enable, disable, or reset', () => {
		const store = new SessionConfigureStore();
		const start = store.getCatalogStateRevision(A);

		store.enableTools(A, ['manage_asset']); // already enabled
		store.reset(A); // already pristine
		expect(store.getCatalogStateRevision(A)).toBe(start);

		store.disableTools(A, ['manage_asset']); // effective
		const afterEffective = store.getCatalogStateRevision(A);
		expect(afterEffective).toBe(start + 1);

		store.disableTools(A, ['manage_asset']); // repeat: already disabled
		expect(store.getCatalogStateRevision(A)).toBe(afterEffective);
	});

	it('rejects protected tools and the core category without advancing the revision', () => {
		const store = new SessionConfigureStore();
		const start = store.getCatalogStateRevision(A);

		const result = store.disableTools(A, ['manage_tools', 'inspect']);
		expect(result.disabled).toEqual([]);
		expect(result.protected).toEqual(
			expect.arrayContaining(['manage_tools', 'inspect']),
		);

		store.disableCategory(A, 'core');
		expect(store.isToolEnabled(A, 'manage_tools')).toBe(true);
		expect(store.isToolEnabled(A, 'inspect')).toBe(true);
		expect(store.getCatalogStateRevision(A)).toBe(start);
	});

	it('keeps core tools enabled when disabling every category and advances once', () => {
		const store = new SessionConfigureStore();
		const result = store.disableCategory(A, 'all');

		expect(store.isToolEnabled(A, 'manage_tools')).toBe(true);
		expect(store.isToolEnabled(A, 'inspect')).toBe(true);
		expect(result.protected).toEqual(
			expect.arrayContaining(['manage_tools', 'inspect']),
		);
		expect(store.getStatus(A).disabledTools).toBeGreaterThan(0);
		expect(store.getCatalogStateRevision(A)).toBe(1);
	});

	it('bumps the revision once when reset actually restores disabled tools', () => {
		const store = new SessionConfigureStore();
		store.disableCategory(A, 'gameplay');
		const beforeReset = store.getCatalogStateRevision(A);
		store.reset(A);
		expect(store.getCatalogStateRevision(A)).toBe(beforeReset + 1);
	});

	it('clamps bounded limits and never treats a limit change as a visibility change', () => {
		const store = new SessionConfigureStore();
		const start = store.getCatalogStateRevision(A);

		const clamped = store.setLimit(A, 'maxResults', 10_000_000);
		expect(clamped.accepted).toBe(true);
		expect(clamped.clamped).toBe(true);
		expect(clamped.value).toBeLessThan(10_000_000);

		const rejected = store.setLimit(A, 'not_a_real_limit', 5);
		expect(rejected.accepted).toBe(false);

		expect(store.getStatus(A).limits.maxResults).toBe(clamped.value);
		expect(store.getCatalogStateRevision(A)).toBe(start); // limit-only: no bump
	});

	it('bounds preferences and never treats a preference change as a visibility change', () => {
		const store = new SessionConfigureStore();
		const start = store.getCatalogStateRevision(A);

		expect(store.setPreference(A, 'verbosity', 'brief').accepted).toBe(true);
		expect(
			store.setPreference(A, 'verbosity', 'x'.repeat(100_000)).accepted,
		).toBe(false);

		expect(store.getStatus(A).preferences.verbosity).toBe('brief');
		expect(store.getCatalogStateRevision(A)).toBe(start); // preference-only: no bump
	});

	it('clears a session and reseeds it pristine on next access', () => {
		const store = new SessionConfigureStore();
		store.disableCategory(A, 'gameplay');
		expect(store.getCatalogStateRevision(A)).toBe(1);
		expect(store.hasSession(A)).toBe(true);

		expect(store.clearSession(A)).toBe(true);
		expect(store.hasSession(A)).toBe(false);
		expect(store.getCatalogStateRevision(A)).toBe(
			BASELINE_CATALOG_STATE_REVISION,
		);
		// hasSession/getCatalogStateRevision are pure reads that never reseed, so a
		// repeat clear before any effective access is a well-defined no-op.
		expect(store.clearSession(A)).toBe(false);
		// Re-access reseeds pristine: gameplay is enabled again, revision back to 0.
		expect(store.isToolEnabled(A, 'manage_ai')).toBe(true);
		expect(store.getStatus(A).catalogStateRevision).toBe(0);
		expect(store.hasSession(A)).toBe(true);
	});

	it('leaves the global dynamic manager an untouched immutable seed', () => {
		const store = new SessionConfigureStore();
		const beforeRevision = CATALOG_REVISION;
		const beforeDisabled = dynamicToolManager.getStatus().disabledTools;

		store.disableCategory(A, 'gameplay');
		store.disableTools(A, ['manage_asset']);

		expect(dynamicToolManager.getStatus().disabledTools).toBe(beforeDisabled);
		expect(dynamicToolManager.isToolEnabled('manage_ai')).toBe(true);
		expect(CATALOG_REVISION).toBe(beforeRevision);
	});
});

describe('manage_tools session seam — default stays on the global manager, injected sessions use the store', () => {
	it('keeps get_status reporting both revisions on the default (stdio) path', async () => {
		const result = await handleManageToolsCall({ action: 'get_status' });
		expect(result.success).toBe(true);
		expect(result.catalogRevision).toBe(CATALOG_REVISION);
		expect(typeof result.catalogStateRevision).toBe('number');
		expect(STDIO_SESSION_ID).toBe(STDIO_SESSION_ID); // stable constant exists
	});

	it('routes the default session to the global manager (preserved coupling)', async () => {
		await handleManageToolsCall({
			action: 'disable_category',
			category: 'gameplay',
		});
		expect(dynamicToolManager.isToolEnabled('manage_ai')).toBe(false);
	});

	it('routes an injected non-default session to its own store overlay, not the global', async () => {
		setManageToolsSessionResolver(() => 'session-X');
		await handleManageToolsCall({
			action: 'disable_category',
			category: 'gameplay',
		});

		// The injected session's overlay changed; the global manager did not.
		expect(sessionConfigureStore.isToolEnabled('session-X', 'manage_ai')).toBe(
			false,
		);
		expect(sessionConfigureStore.getCatalogStateRevision('session-X')).toBe(1);
		expect(dynamicToolManager.isToolEnabled('manage_ai')).toBe(true);
	});

	it('isolates two injected sessions from each other through the seam', async () => {
		setManageToolsSessionResolver(() => 'session-X');
		await handleManageToolsCall({
			action: 'disable_category',
			category: 'gameplay',
		});
		setManageToolsSessionResolver(() => 'session-Y');
		const status = await handleManageToolsCall({ action: 'get_status' });

		expect(sessionConfigureStore.isToolEnabled('session-Y', 'manage_ai')).toBe(
			true,
		);
		expect(status.catalogStateRevision).toBe(0);
		expect(sessionConfigureStore.getCatalogStateRevision('session-X')).toBe(1);
	});
});
