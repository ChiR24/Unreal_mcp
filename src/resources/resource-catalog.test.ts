import { describe, expect, it } from 'vitest';
import { NEW_RESOURCE_DEFINITIONS, RESOURCE_TEMPLATES } from './resource-catalog.js';

describe('resource-catalog', () => {
  it('defines exactly the four new static resources', () => {
    // Given / When
    const uris = NEW_RESOURCE_DEFINITIONS.map((definition) => definition.uri);

    // Then
    expect(uris).toEqual(['ue://capability/catalog', 'ue://project', 'ue://editor', 'ue://selection']);
    expect(new Set(uris).size).toBe(uris.length);
    for (const definition of NEW_RESOURCE_DEFINITIONS) {
      expect(definition.mimeType).toBe('application/json');
      expect(definition.name.length).toBeGreaterThan(0);
      expect(definition.description.length).toBeGreaterThan(0);
    }
  });

  it('defines exactly the four resource templates', () => {
    // Given / When
    const templates = RESOURCE_TEMPLATES.map((template) => template.uriTemplate);

    // Then
    expect(templates).toEqual([
      'ue://capability/{capabilityId}',
      'ue://knowledge/{engineVersion}/{topic}',
      'ue://object/{objectPath}',
      'ue://asset/{assetPath}',
    ]);
    for (const template of RESOURCE_TEMPLATES) {
      expect(template.mimeType).toBe('application/json');
    }
  });

  it('never leaks host-path or secret tokens in metadata', () => {
    // Given
    const serialized = JSON.stringify({ NEW_RESOURCE_DEFINITIONS, RESOURCE_TEMPLATES });

    // Then
    for (const forbidden of ['C:\\', '/home/', '/Users/', '.uproject', 'UE_PROJECT_PATH', 'token']) {
      expect(serialized).not.toContain(forbidden);
    }
  });
});
