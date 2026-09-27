import { describe, expect, it, vi } from 'vitest';
import { INITIAL_REVISION } from '../server/mcp-primitives/resource-revision.js';
import { ResourceError } from './resource-errors.js';
import { KnowledgeResources } from './knowledge-resources.js';
import { assetExistsBridge, revisionsAt } from './resources.test-support.js';

function knowledge(overrides: { isAvailable?: () => Promise<boolean>; assetExists?: boolean } = {}): KnowledgeResources {
  return new KnowledgeResources(assetExistsBridge(overrides.assetExists ?? false), overrides.isAvailable ?? (async () => true), revisionsAt({ 'ue://asset-registry': 7 }));
}

function errorCode(fn: () => unknown): string | undefined {
  try {
    fn();
  } catch (error) {
    return error instanceof ResourceError ? error.code : 'not-a-ResourceError';
  }
  return undefined;
}

describe('knowledge-resources', () => {
  it('reads stable knowledge at the initial revision', () => {
    const topic = knowledge().readKnowledge('ue://knowledge/paths', 'paths');
    expect(topic.revision).toBe(INITIAL_REVISION);
    expect(topic.data.topic).toBe('paths');
    expect(topic.data.title.length).toBeGreaterThan(0);
    expect(topic.data.references.length).toBeGreaterThan(0);
  });

  it('rejects an unknown topic as a typed NOT_FOUND', () => {
    expect(errorCode(() => knowledge().readKnowledge('ue://knowledge/nope', 'nope'))).toBe('RESOURCE_NOT_FOUND');
  });

  it('reads an asset handle reflecting existence at the asset-registry revision', async () => {
    const handle = await knowledge({ assetExists: true }).readAsset('ue://asset//Game/Bar', '/Game/Bar');
    expect(handle.revision).toBe(7);
    expect(handle.data).toEqual({ path: '/Game/Bar', exists: true });
  });

  it('is unavailable without a connected editor', async () => {
    await expect(knowledge({ isAvailable: async () => false }).readAsset('ue://asset//Game/Bar', '/Game/Bar'))
      .rejects.toMatchObject({ code: 'RESOURCE_UNAVAILABLE' });
  });

  it('rejects traversal before touching the bridge', async () => {
    const bridge = assetExistsBridge(true);
    const send = vi.spyOn(bridge, 'sendAutomationRequest');
    const resources = new KnowledgeResources(bridge, async () => true, revisionsAt());

    await expect(resources.readAsset('ue://asset//Game/../Secret', '/Game/../Secret')).rejects.toMatchObject({ code: 'RESOURCE_TRAVERSAL_REJECTED' });
    expect(send).not.toHaveBeenCalled();
  });
});
