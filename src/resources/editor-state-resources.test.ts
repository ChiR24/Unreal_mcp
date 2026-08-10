import { describe, expect, it } from 'vitest';
import {
  InMemoryRevisionProvider,
  asResourceRevision,
} from '../server/mcp-primitives/resource-revision.js';
import { ResourceError } from './resource-errors.js';
import {
  EditorStateResources,
  type EditorStateSource,
  type SelectionEntry,
} from './editor-state-resources.js';

const LIVE_REVISIONS = { selection: 2, level: 3, assetRegistry: 4, package: 5 } as const;

function source(overrides: Partial<EditorStateSource> = {}): EditorStateSource {
  return {
    isAvailable: async () => true,
    engineVersion: async () => '5.7.4',
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'Main', path: '/Game/Maps/Main' }),
    selectedActors: async () => [{ name: 'Cube', path: '/Game/Cube' }],
    liveRevisions: async () => LIVE_REVISIONS,
    ...overrides,
  };
}

function provider(): InMemoryRevisionProvider {
  const instance = new InMemoryRevisionProvider();
  instance.set('ue://project', asResourceRevision(2));
  instance.set('ue://pie', asResourceRevision(3));
  instance.set('ue://selection', asResourceRevision(4));
  return instance;
}

describe('editor-state-resources', () => {
  it('reads a redacted, connected project with its revision', async () => {
    // Given
    const resources = new EditorStateResources(source(), provider(), 'MyGame');

    // When
    const project = await resources.readProject();

    // Then
    expect(project.revision).toBe(2);
    expect(project.data).toEqual({
      projectName: 'MyGame',
      engineVersion: '5.7.4',
      contentRoot: '/Game',
      connected: true,
    });
  });

  it('reads a project offline without an engine version and never throws', async () => {
    // Given
    const resources = new EditorStateResources(source({ isAvailable: async () => false }), provider(), null);

    // When
    const project = await resources.readProject();

    // Then
    expect(project.data.connected).toBe(false);
    expect(project.data.engineVersion).toBeNull();
    expect(project.data.projectName).toBeNull();
  });

  it('reads editor state tagged with the PIE revision', async () => {
    // Given
    const resources = new EditorStateResources(source({ pieActive: async () => true }), provider(), 'MyGame');

    // When
    const editor = await resources.readEditor();

    // Then
    expect(editor.revision).toBe(3);
    expect(editor.data.pieActive).toBe(true);
    expect(editor.data.currentLevel).toEqual({ name: 'Main', path: '/Game/Maps/Main' });
  });

  it('throws UNAVAILABLE for editor and selection when disconnected', async () => {
    // Given
    const resources = new EditorStateResources(source({ isAvailable: async () => false }), provider(), 'MyGame');

    // When / Then
    await expect(resources.readEditor()).rejects.toBeInstanceOf(ResourceError);
    await expect(resources.readSelection()).rejects.toMatchObject({ code: 'RESOURCE_UNAVAILABLE' });
  });

  it('caps the selection and reports truncation', async () => {
    // Given
    const many: SelectionEntry[] = Array.from({ length: 250 }, (_unused, index) => ({
      name: `Actor_${index}`,
      path: `/Game/Actor_${index}`,
    }));
    const resources = new EditorStateResources(source({ selectedActors: async () => many }), provider(), 'MyGame');

    // When
    const selection = await resources.readSelection();

    // Then
    expect(selection.revision).toBe(4);
    expect(selection.data.count).toBe(200);
    expect(selection.data.totalCount).toBe(250);
    expect(selection.data.truncated).toBe(true);
    expect(selection.data.actors).toHaveLength(200);
  });

  it('reads the exact live revision snapshot and uses its maximum as the resource revision', async () => {
    const resources = new EditorStateResources(source(), provider(), 'MyGame');

    const revisions = await resources.readStateRevisions();

    expect(revisions.revision).toBe(5);
    expect(revisions.data).toEqual(LIVE_REVISIONS);
  });
});
