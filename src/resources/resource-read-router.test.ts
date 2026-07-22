import { describe, expect, it } from 'vitest';
import type { GatewayManifest } from '../gateway/gateway-manifest-types.js';
import { InMemoryRevisionProvider } from '../server/mcp-primitives/resource-revision.js';
import { ResourceError } from './resource-errors.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from './capability-resources.js';
import { EditorStateResources, type EditorStateSource } from './editor-state-resources.js';
import { KnowledgeResources, type AssetLookupSource } from './knowledge-resources.js';
import { ResourceReadRouter } from './resource-read-router.js';

const MANIFEST: GatewayManifest = {
  version: 3,
  source: 'test',
  tools: [
    { name: 'manage_asset', category: 'core', description: 'd', actions: ['import'], parameterNames: ['p'], inputSchema: {}, perActionSchemas: false },
  ],
};

function editorSource(overrides: Partial<EditorStateSource> = {}): EditorStateSource {
  return {
    isAvailable: async () => true,
    engineVersion: async () => '5.7.4',
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'Main', path: '/Game/Maps/Main' }),
    selectedActors: async () => [{ name: 'Cube', path: '/Game/Cube' }],
    ...overrides,
  };
}

const assetLookup: AssetLookupSource = {
  isAvailable: async () => true,
  objectExists: async () => true,
  assetExists: async () => true,
};

function router(editor: EditorStateSource = editorSource()): ResourceReadRouter {
  const revisions = new InMemoryRevisionProvider();
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(MANIFEST), revisions),
    new EditorStateResources(editor, revisions, 'MyGame'),
    new KnowledgeResources(assetLookup, revisions),
  );
}

function parse(result: { contents: Array<{ text: string }> }): Record<string, unknown> {
  return JSON.parse(result.contents[0].text) as Record<string, unknown>;
}

describe('resource-read-router', () => {
  it('routes each static resource and template with a visible revision', async () => {
    // Given
    const seam = router();

    // When
    const catalog = parse(await seam.read('ue://capability/catalog'));
    const project = parse(await seam.read('ue://project'));
    const editor = parse(await seam.read('ue://editor'));
    const record = parse(await seam.read('ue://capability/manage_asset'));
    const knowledge = parse(await seam.read('ue://knowledge/5.7/paths'));
    const object = parse(await seam.read('ue://object/%2FGame%2FFoo'));

    // Then
    expect(catalog.uri).toBe('ue://capability/catalog');
    expect(typeof catalog.revision).toBe('number');
    expect((project.data as { projectName: string }).projectName).toBe('MyGame');
    expect((editor.data as { pieActive: boolean }).pieActive).toBe(false);
    expect((record.data as { id: string }).id).toBe('manage_asset');
    expect((knowledge.data as { topic: string }).topic).toBe('paths');
    expect((object.data as { path: string }).path).toBe('/Game/Foo');
  });

  it('returns typed errors for unknown, malformed, and traversal URIs with no payload', async () => {
    // Given
    const seam = router();

    // When / Then
    await expect(seam.read('ue://capability/ghost')).rejects.toMatchObject({ code: 'RESOURCE_NOT_FOUND' });
    await expect(seam.read('ue://totally-unknown')).rejects.toMatchObject({ code: 'RESOURCE_NOT_FOUND' });
    await expect(seam.read('ue://knowledge/5.7')).rejects.toMatchObject({ code: 'RESOURCE_INVALID_URI' });
    await expect(seam.read('ue://object/..%2F..%2Fetc')).rejects.toMatchObject({
      code: 'RESOURCE_TRAVERSAL_REJECTED',
    });
  });

  it('enforces the byte budget end to end', async () => {
    // Given
    const bloated = Array.from({ length: 200 }, (_unused, index) => ({
      name: 'x'.repeat(500),
      path: `/Game/Actor_${index}`,
    }));
    const seam = router(editorSource({ selectedActors: async () => bloated }));

    // When / Then
    await expect(seam.read('ue://selection')).rejects.toMatchObject({ code: 'RESOURCE_TOO_LARGE' });
  });

  it('reads are idempotent and non-mutating', async () => {
    // Given
    const seam = router();

    // When
    const first = await seam.read('ue://capability/catalog');
    const second = await seam.read('ue://capability/catalog');

    // Then
    expect(first.contents[0].text).toBe(second.contents[0].text);
    expect(first.contents[0].mimeType).toBe('application/json');
  });

  it('throws ResourceError (not a bare Error) so failures stay typed', async () => {
    // Given
    const seam = router();

    // When / Then
    await expect(seam.read('ue://nope')).rejects.toBeInstanceOf(ResourceError);
  });
});
