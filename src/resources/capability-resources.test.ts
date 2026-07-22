import { describe, expect, it } from 'vitest';
import type { GatewayManifest } from '../gateway/gateway-manifest-types.js';
import {
  InMemoryRevisionProvider,
  asResourceRevision,
} from '../server/mcp-primitives/resource-revision.js';
import { ResourceError } from './resource-errors.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from './capability-resources.js';

function manifestWith(toolCount: number): GatewayManifest {
  const tools = Array.from({ length: toolCount }, (_unused, index) => ({
    name: `tool_${index}`,
    category: 'core' as const,
    description: 'desc',
    actions: ['a', 'b', 'c'],
    parameterNames: ['p1', 'p2'],
    inputSchema: {},
    perActionSchemas: false,
  }));
  return { version: 3, source: 'test', tools };
}

function providerAt(revision: number): InMemoryRevisionProvider {
  const provider = new InMemoryRevisionProvider();
  provider.set('ue://capability/catalog', asResourceRevision(revision));
  return provider;
}

describe('capability-resources', () => {
  it('reads a bounded catalog carrying the injected revision', () => {
    // Given
    const resources = new CapabilityResources(
      new GatewayManifestCapabilitySource(manifestWith(3)),
      providerAt(5),
    );

    // When
    const catalog = resources.readCatalog();

    // Then
    expect(catalog.uri).toBe('ue://capability/catalog');
    expect(catalog.revision).toBe(5);
    expect(catalog.data.count).toBe(3);
    expect(catalog.data.totalCount).toBe(3);
    expect(catalog.data.truncated).toBe(false);
    expect(catalog.data.capabilities[0]).toEqual({ id: 'tool_0', category: 'core', actionCount: 3 });
    expect(JSON.stringify(catalog)).not.toContain('inputSchema');
  });

  it('truncates the catalog beyond the entry cap', () => {
    // Given
    const resources = new CapabilityResources(new GatewayManifestCapabilitySource(manifestWith(70)), providerAt(1));

    // When
    const catalog = resources.readCatalog();

    // Then
    expect(catalog.data.count).toBe(64);
    expect(catalog.data.totalCount).toBe(70);
    expect(catalog.data.truncated).toBe(true);
  });

  it('reads a bounded record for a known capability', () => {
    // Given
    const resources = new CapabilityResources(new GatewayManifestCapabilitySource(manifestWith(2)), providerAt(9));

    // When
    const record = resources.readRecord('ue://capability/tool_1', 'tool_1');

    // Then
    expect(record.revision).toBe(9);
    expect(record.data.id).toBe('tool_1');
    expect(record.data.actionCount).toBe(3);
    expect(record.data.parameterCount).toBe(2);
    expect(record.data.actions).toEqual(['a', 'b', 'c']);
    expect(JSON.stringify(record)).not.toContain('inputSchema');
  });

  it('throws a typed NOT_FOUND for an unknown capability', () => {
    // Given
    const resources = new CapabilityResources(new GatewayManifestCapabilitySource(manifestWith(1)), providerAt(1));

    // When / Then
    try {
      resources.readRecord('ue://capability/ghost', 'ghost');
      throw new Error('expected NOT_FOUND');
    } catch (error) {
      expect(error).toBeInstanceOf(ResourceError);
      expect((error as ResourceError).code).toBe('RESOURCE_NOT_FOUND');
      expect((error as ResourceError).uri).toBe('ue://capability/ghost');
    }
  });
});
