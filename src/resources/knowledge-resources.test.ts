import { describe, expect, it } from 'vitest';
import {
  INITIAL_REVISION,
  InMemoryRevisionProvider,
  asResourceRevision,
} from '../server/mcp-primitives/resource-revision.js';
import { ResourceError } from './resource-errors.js';
import { KnowledgeResources, type AssetLookupSource } from './knowledge-resources.js';

function lookup(overrides: Partial<AssetLookupSource> = {}): AssetLookupSource {
  return {
    isAvailable: async () => true,
    objectExists: async () => true,
    assetExists: async () => false,
    ...overrides,
  };
}

function provider(): InMemoryRevisionProvider {
  const instance = new InMemoryRevisionProvider();
  instance.set('ue://asset-registry', asResourceRevision(7));
  return instance;
}

describe('knowledge-resources', () => {
  it('reads stable knowledge at the initial revision', () => {
    // Given
    const resources = new KnowledgeResources(lookup(), provider());

    // When
    const knowledge = resources.readKnowledge('ue://knowledge/5.7/paths', '5.7', 'paths');

    // Then
    expect(knowledge.revision).toBe(INITIAL_REVISION);
    expect(knowledge.data.engineVersion).toBe('5.7');
    expect(knowledge.data.topic).toBe('paths');
    expect(knowledge.data.title.length).toBeGreaterThan(0);
    expect(knowledge.data.references.length).toBeGreaterThan(0);
  });

  it('rejects an unknown topic and a malformed version', () => {
    // Given
    const resources = new KnowledgeResources(lookup(), provider());

    // When / Then
    expect(() => resources.readKnowledge('ue://knowledge/5.7/nope', '5.7', 'nope')).toThrowError(ResourceError);
    try {
      resources.readKnowledge('ue://knowledge/bad ver/paths', 'bad ver', 'paths');
      throw new Error('expected invalid version');
    } catch (error) {
      expect((error as ResourceError).code).toBe('RESOURCE_INVALID_URI');
    }
  });

  it('reads a normalized object handle tagged with the asset-registry revision', async () => {
    // Given
    const resources = new KnowledgeResources(lookup(), provider());

    // When
    const handle = await resources.readObject('ue://object/%2FGame%2FFoo', '%2FGame%2FFoo');

    // Then
    expect(handle.revision).toBe(7);
    expect(handle.data).toEqual({ kind: 'object', path: '/Game/Foo', exists: true });
  });

  it('reads an asset handle reflecting existence', async () => {
    // Given
    const resources = new KnowledgeResources(lookup({ assetExists: async () => true }), provider());

    // When
    const handle = await resources.readAsset('ue://asset//Game/Bar', '/Game/Bar');

    // Then
    expect(handle.data).toEqual({ kind: 'asset', path: '/Game/Bar', exists: true });
  });

  it('rejects traversal before touching the boundary', async () => {
    // Given
    const resources = new KnowledgeResources(lookup(), provider());

    // When / Then
    await expect(resources.readObject('ue://object/x', '/Game/../Engine/Secret')).rejects.toMatchObject({
      code: 'RESOURCE_TRAVERSAL_REJECTED',
    });
  });

  it('throws UNAVAILABLE when the boundary is disconnected', async () => {
    // Given
    const resources = new KnowledgeResources(lookup({ isAvailable: async () => false }), provider());

    // When / Then
    await expect(resources.readAsset('ue://asset//Game/Bar', '/Game/Bar')).rejects.toMatchObject({
      code: 'RESOURCE_UNAVAILABLE',
    });
  });
});
