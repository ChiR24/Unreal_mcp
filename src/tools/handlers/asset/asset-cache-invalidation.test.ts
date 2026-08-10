import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { handleAssetTools } from './asset-handlers.js';
import { collectInvalidationPaths, listingNeutralAssetActions } from './asset-cache-invalidation.js';
import { isValidAssetAction } from './asset-validation.js';
import { AssetResources } from '../../../resources/assets.js';
import { AutomationBridge } from '../../../automation/index.js';
import { UnrealBridge } from '../../../unreal-bridge.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';

/**
 * The asset listing cache is TTL-only (ASSET_LIST_TTL_MS, default 10s). A client
 * that mutates an asset and immediately lists its directory must not be served
 * the pre-mutation listing while the TTL runs out.
 *
 * Every test below runs on a FROZEN fake clock: `vi.useFakeTimers()` pins
 * `Date.now()`, and the clock is never advanced. Zero milliseconds elapse
 * between the mutation and the follow-up read, so a cache hit can only mean
 * stale data and never "the TTL had not expired yet". No sleeps, no timers.
 */

const FROZEN_NOW = new Date('2026-07-26T00:00:00.000Z');

interface ListPayload {
  readonly folders: ReadonlyArray<Record<string, unknown>>;
  readonly assets: ReadonlyArray<Record<string, unknown>>;
}

function asset(name: string, dir: string): Record<string, unknown> {
  return { n: name, p: `${dir}/${name}`, c: 'Object' };
}

/**
 * Wires a real AssetResources (the cache owner) behind its own automation
 * bridge, and a separate bridge for the mutation path, so listing calls and
 * mutation calls can be counted independently.
 */
function createHarness(listings: Map<string, ListPayload[]>) {
  const listCallsByPath: string[] = [];

  const listBridge = new AutomationBridge({ enabled: false });
  vi.spyOn(listBridge, 'isConnected').mockReturnValue(true);
  vi.spyOn(listBridge, 'sendAutomationRequest').mockImplementation(
    async (_action: string, payload?: Record<string, unknown>) => {
      const path = typeof payload?.path === 'string' ? payload.path : '';
      listCallsByPath.push(path);
      const queued = listings.get(path) ?? [];
      // Serve successive listings so a refetch is observably fresher than the
      // cached copy; the last entry repeats once the queue is drained.
      const index = Math.min(
        listCallsByPath.filter((entry) => entry === path).length - 1,
        queued.length - 1
      );
      const payloadForCall = queued[index] ?? { folders: [], assets: [] };
      return {
        success: true,
        result: { folders_list: payloadForCall.folders, assets: payloadForCall.assets }
      };
    }
  );

  const unrealBridge = new UnrealBridge();
  unrealBridge.setAutomationBridge(listBridge);
  const assetResources = new AssetResources(unrealBridge);

  const mutationCalls: Array<{ toolName: string; payload: Record<string, unknown> }> = [];
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources,
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async (toolName: string, payload: Record<string, unknown>) => {
        mutationCalls.push({ toolName, payload });
        return { success: true };
      }
    }
  };

  return { assetResources, tools, listCallsByPath, mutationCalls };
}

function namesOf(listing: unknown): string[] {
  const assets = (listing as { assets?: Array<{ Name?: string }> }).assets ?? [];
  return assets.map((entry) => entry.Name ?? '');
}

function callsFor(listCallsByPath: readonly string[], path: string): number {
  return listCallsByPath.filter((entry) => entry === path).length;
}

describe('asset cache invalidation on mutation', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(FROZEN_NOW);
    // A long TTL guarantees nothing below can pass by TTL expiry.
    vi.stubEnv('ASSET_LIST_TTL_MS', '600000');
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.unstubAllEnvs();
    vi.restoreAllMocks();
  });

  it('serves fresh data for the mutated directory without waiting out the TTL', async () => {
    const listings = new Map<string, ListPayload[]>([
      [
        '/Game/Props',
        [
          { folders: [], assets: [asset('Crate', '/Game/Props'), asset('Barrel', '/Game/Props')] },
          { folders: [], assets: [asset('Barrel', '/Game/Props')] }
        ]
      ]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    const before = await assetResources.list('/Game/Props');
    expect(namesOf(before)).toEqual(['Crate', 'Barrel']);
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);

    const deletion = await handleAssetTools(
      'delete_asset',
      { assetPath: '/Game/Props/Crate' },
      tools
    );
    expect(deletion.success).not.toBe(false);

    // Zero milliseconds have elapsed. Freshness here can only come from
    // invalidation, never from the TTL.
    const after = await assetResources.list('/Game/Props');

    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(2);
    expect(namesOf(after)).toEqual(['Barrel']);
  });

  it('leaves an unrelated directory cached when a sibling directory is mutated', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Props', [{ folders: [], assets: [asset('Crate', '/Game/Props')] }]],
      ['/Game/Weapons', [{ folders: [], assets: [asset('Sword', '/Game/Weapons')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Props');
    await assetResources.list('/Game/Weapons');
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);
    expect(callsFor(listCallsByPath, '/Game/Weapons')).toBe(1);

    await handleAssetTools('delete_asset', { assetPath: '/Game/Props/Crate' }, tools);

    await assetResources.list('/Game/Props');
    await assetResources.list('/Game/Weapons');

    // Affected key refetched; unrelated key still served from cache.
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(2);
    expect(callsFor(listCallsByPath, '/Game/Weapons')).toBe(1);
  });

  it('does not invalidate anything when the mutation is a read', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Props', [{ folders: [], assets: [asset('Crate', '/Game/Props')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Props');
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);

    // A read that names a path inside the cached directory must not evict it.
    await handleAssetTools('get_metadata', { assetPath: '/Game/Props/Crate' }, tools);

    await assetResources.list('/Game/Props');
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);
  });

  it('invalidates both endpoints of a rename', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Props', [{ folders: [], assets: [asset('Crate', '/Game/Props')] }]],
      ['/Game/Archive', [{ folders: [], assets: [] }]],
      ['/Game/Weapons', [{ folders: [], assets: [asset('Sword', '/Game/Weapons')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Props');
    await assetResources.list('/Game/Archive');
    await assetResources.list('/Game/Weapons');

    await handleAssetTools(
      'rename_asset',
      { sourcePath: '/Game/Props/Crate', destinationPath: '/Game/Archive/Crate' },
      tools
    );

    await assetResources.list('/Game/Props');
    await assetResources.list('/Game/Archive');
    await assetResources.list('/Game/Weapons');

    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(2);
    expect(callsFor(listCallsByPath, '/Game/Archive')).toBe(2);
    expect(callsFor(listCallsByPath, '/Game/Weapons')).toBe(1);
  });

  it('invalidates the import destination only', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Imported', [{ folders: [], assets: [] }]],
      ['/Game/Weapons', [{ folders: [], assets: [asset('Sword', '/Game/Weapons')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Imported');
    await assetResources.list('/Game/Weapons');

    const imported = await handleAssetTools(
      'import',
      { sourcePath: 'Sword.fbx', destinationPath: '/Game/Imported/Sword' },
      tools
    );
    expect(imported).toMatchObject({ success: true });

    await assetResources.list('/Game/Imported');
    await assetResources.list('/Game/Weapons');

    expect(callsFor(listCallsByPath, '/Game/Imported')).toBe(2);
    expect(callsFor(listCallsByPath, '/Game/Weapons')).toBe(1);
  });

  it('leaves the whole cache intact when a mutation names no content path', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Props', [{ folders: [], assets: [asset('Crate', '/Game/Props')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Props');

    await handleAssetTools('fixup_redirectors', {}, tools);

    await assetResources.list('/Game/Props');
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);
  });

  it('does not invalidate when the mutation failed', async () => {
    const listings = new Map<string, ListPayload[]>([
      ['/Game/Props', [{ folders: [], assets: [asset('Crate', '/Game/Props')] }]]
    ]);
    const { assetResources, tools, listCallsByPath } = createHarness(listings);

    await assetResources.list('/Game/Props');

    const failing = tools.automationBridge as unknown as {
      sendAutomationRequest: (tool: string, payload: Record<string, unknown>) => Promise<unknown>;
    };
    failing.sendAutomationRequest = async () => ({ success: false, error: 'DELETE_FAILED' });

    await handleAssetTools('delete_asset', { assetPath: '/Game/Props/Crate' }, tools);

    await assetResources.list('/Game/Props');
    expect(callsFor(listCallsByPath, '/Game/Props')).toBe(1);
  });
});

describe('collectInvalidationPaths', () => {
  it('keeps content paths and drops a host filesystem path', () => {
    const paths = collectInvalidationPaths('import', {
      sourcePath: '/home/user/models/Sword.fbx',
      destinationPath: '/Game/Imported/Sword'
    });

    expect(paths).toEqual(['/Game/Imported/Sword']);
  });

  it('drops values that are not content paths at all', () => {
    const paths = collectInvalidationPaths('duplicate', {
      sourcePath: 'Sword.fbx',
      destinationPath: 'C:\\Users\\me\\Sword.uasset',
      assetPath: '/Game/Props/Crate'
    });

    expect(paths).toEqual(['/Game/Props/Crate']);
  });

  it('collects every entry of an array path field', () => {
    const paths = collectInvalidationPaths('delete', {
      paths: ['/Game/Props/Crate', '/Game/Weapons/Sword']
    });

    expect(paths).toEqual(['/Game/Props/Crate', '/Game/Weapons/Sword']);
  });

  it('returns nothing for a listing-neutral action even when it names a path', () => {
    for (const action of listingNeutralAssetActions()) {
      expect(collectInvalidationPaths(action, { assetPath: '/Game/Props/Crate' })).toEqual([]);
    }
  });

  it('only lists real asset actions as listing-neutral', () => {
    const unknown = listingNeutralAssetActions().filter((action) => !isValidAssetAction(action));

    expect(unknown).toEqual([]);
  });
});
