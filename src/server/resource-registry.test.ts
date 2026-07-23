import { describe, expect, it } from 'vitest';
import {
  ListResourceTemplatesRequestSchema,
  ListResourcesRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';
import { ResourceRegistry } from './resource-registry.js';

type CapturedHandler = () => Promise<Record<string, unknown>>;

function registerAndCapture(): Map<unknown, CapturedHandler> {
  const handlers = new Map<unknown, CapturedHandler>();
  const server = {
    setRequestHandler: (schema: unknown, handler: CapturedHandler) => {
      handlers.set(schema, handler);
    },
  };
  const stub = undefined as unknown;
  const registry = new ResourceRegistry(
    server as unknown as ConstructorParameters<typeof ResourceRegistry>[0],
    stub as ConstructorParameters<typeof ResourceRegistry>[1],
    stub as ConstructorParameters<typeof ResourceRegistry>[2],
    stub as ConstructorParameters<typeof ResourceRegistry>[3],
    stub as ConstructorParameters<typeof ResourceRegistry>[4],
    stub as ConstructorParameters<typeof ResourceRegistry>[5],
    stub as ConstructorParameters<typeof ResourceRegistry>[6],
    (async () => false) as ConstructorParameters<typeof ResourceRegistry>[7],
  );
  registry.register();
  return handlers;
}

describe('resource-registry listing', () => {
  it('keeps the six pre-existing resources and adds the four new ones', async () => {
    // Given
    const handlers = registerAndCapture();
    const listResources = handlers.get(ListResourcesRequestSchema);
    expect(listResources).toBeDefined();

    // When
    const result = (await listResources?.()) as { resources: Array<{ uri: string }> };
    const uris = result.resources.map((resource) => resource.uri);

    // Then — baseline: the six pre-existing resources remain compatible.
    for (const legacy of ['ue://assets', 'ue://actors', 'ue://level', 'ue://health', 'ue://automation-bridge', 'ue://version']) {
      expect(uris).toContain(legacy);
    }
    // And the four new version-aware resources are additive.
    for (const added of ['ue://capability/catalog', 'ue://project', 'ue://editor', 'ue://selection']) {
      expect(uris).toContain(added);
    }
    expect(uris).toHaveLength(10);
    expect(new Set(uris).size).toBe(10);
  });

  it('registers the four resource templates', async () => {
    // Given
    const handlers = registerAndCapture();
    const listTemplates = handlers.get(ListResourceTemplatesRequestSchema);
    expect(listTemplates).toBeDefined();

    // When
    const result = (await listTemplates?.()) as { resourceTemplates: Array<{ uriTemplate: string }> };
    const templates = result.resourceTemplates.map((template) => template.uriTemplate);

    // Then
    expect(templates).toEqual([
      'ue://capability/{capabilityId}',
      'ue://knowledge/{engineVersion}/{topic}',
      'ue://object/{objectPath}',
      'ue://asset/{assetPath}',
    ]);
  });
});
