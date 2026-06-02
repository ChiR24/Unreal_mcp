import { afterEach, describe, expect, it, vi } from 'vitest';
import { AssetResources } from './assets.js';
import type { AutomationBridge } from '../automation/index.js';
import type { UnrealBridge } from '../unreal-bridge.js';

function createAssetResources(assets: Array<Record<string, unknown>> = []) {
  const sendAutomationRequest = vi.fn(async () => ({
    success: true,
    result: {
      folders_list: [],
      assets
    }
  }));

  const automationBridge = {
    isConnected: () => true,
    sendAutomationRequest
  } as unknown as AutomationBridge;

  const unrealBridge = {
    isConnected: true,
    getAutomationBridge: () => automationBridge
  } as unknown as UnrealBridge;

  return { resources: new AssetResources(unrealBridge), sendAutomationRequest };
}

describe('AssetResources cache TTL parsing', () => {
  afterEach(() => {
    vi.unstubAllEnvs();
  });

  it('falls back to the default TTL for invalid env strings', async () => {
    for (const value of ['5000ms', '5e3', '0x2710', '-1']) {
      vi.stubEnv('ASSET_LIST_TTL_MS', value);
      const { resources, sendAutomationRequest } = createAssetResources();

      await resources.list('/Game');
      await resources.list('/Game');

      expect(sendAutomationRequest).toHaveBeenCalledTimes(1);
    }
  });

  it('keeps zero as an explicit cache disable value', async () => {
    vi.stubEnv('ASSET_LIST_TTL_MS', '0');
    const { resources, sendAutomationRequest } = createAssetResources();

    await resources.list('/Game');
    await resources.list('/Game');

    expect(sendAutomationRequest).toHaveBeenCalledTimes(2);
  });

  it('normalizes invalid pagination inputs before slicing and caching', async () => {
    const assets = [
      { n: 'A', p: '/Game/A', c: 'Object' },
      { n: 'B', p: '/Game/B', c: 'Object' }
    ];
    const { resources, sendAutomationRequest } = createAssetResources(assets);

    const first = await resources.listPaged('/Game', -1, -5);
    const second = await resources.listPaged('/Game', -1, -5);

    expect(first).toMatchObject({ page: 0, pageSize: 30, count: 2, totalCount: 2, hasMore: false });
    expect(second).toBe(first);
    expect(sendAutomationRequest).toHaveBeenCalledTimes(1);
  });

  it('floors fractional pagination inputs and caps page size', async () => {
    const assets = Array.from({ length: 60 }, (_, index) => ({
      n: `Asset${index}`,
      p: `/Game/Asset${index}`,
      c: 'Object'
    }));
    const { resources } = createAssetResources(assets);

    const result = await resources.listPaged('/Game', 1.8, 200.5);

    expect(result).toMatchObject({ page: 1, pageSize: 50, count: 10, totalCount: 60, hasMore: false });
  });
});
