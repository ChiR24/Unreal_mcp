/// <reference types="node" />

import fs from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';

import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import { createTrackedTempRoot, registerTempRootCleanup } from './audit-fixture-workspace.js';
import {
  canonicalSchema,
  createActionOverrideFixture,
  createDefinitionsFixture
} from './parameter-audit-schema-fixtures.js';

registerTempRootCleanup();

describe('parameter audit schema discovery', () => {
  it('matches every canonical tool schema exactly', async () => {
    const { extractToolSchemas } = await import('../parameter-audit-schema.mjs');
    const schemas = extractToolSchemas();

    expect(schemas).toEqual(
      consolidatedToolDefinitions
        .map(canonicalSchema)
        .sort((left, right) => left.name.localeCompare(right.name))
    );
  });

  it('discovers the whole canonical parent surface from the generated runtime facade', async () => {
    // Given the real repository, where parent definitions are generated rather than hand-written
    const { extractToolSchemas } = await import('../parameter-audit-schema.mjs');

    // When the audit extracts schemas with no pinned definitions root
    const schemas = extractToolSchemas();

    // Then every canonical parent is discovered with a usable action enum
    expect(schemas.length).toBe(consolidatedToolDefinitions.length);
    expect(schemas.length).toBeGreaterThan(0);
    expect(
      schemas.filter((schema: { actions: string[] }) => schema.actions.length === 0)
    ).toEqual([]);
    expect(
      schemas.filter((schema: { properties: string[] }) => !schema.properties.includes('action'))
    ).toEqual([]);
  });

  it('fails closed when the generated runtime facade declares no tools', async () => {
    // Given a gateway manifest whose tool list was emptied
    const { readRuntimeFacadeToolDefinitions } = await import('../parameter-audit-context.mjs');
    const root = createTrackedTempRoot('parameter-audit-empty-facade-');
    const manifestPath = path.join(root, 'gateway-manifest.generated.json');
    fs.writeFileSync(manifestPath, JSON.stringify({ version: 1, tools: [] }));

    // When the audit loads that facade
    // Then it raises instead of reporting a vacuous zero-coverage run
    expect(() => readRuntimeFacadeToolDefinitions(manifestPath)).toThrow(
      /zero parent tool definitions/
    );
  });

  it('resolves import aliases without conflating same-named helpers in nested modules', async () => {
    const { extractToolSchemas } = await import('../parameter-audit-schema.mjs');
    const definitionsRoot = createDefinitionsFixture();

    expect(extractToolSchemas({ definitionsRoot })).toEqual([
      {
        name: 'first_tool',
        actions: ['first_action'],
        properties: ['action', 'firstOnly'],
        required: ['action', 'firstOnly']
      },
      {
        name: 'second_tool',
        actions: ['second_action', 'shared_action'],
        properties: ['action', 'secondOnly'],
        required: ['action']
      }
    ]);
  });

  it('applies explicit action overrides after spread-provided enums', async () => {
    const { extractToolSchemas } = await import('../parameter-audit-schema.mjs');
    const definitionsRoot = createActionOverrideFixture();

    expect(extractToolSchemas({ definitionsRoot })).toEqual([
      {
        name: 'nested_spread',
        actions: [],
        properties: ['action', 'sharedOnly'],
        required: ['action']
      },
      {
        name: 'with_enum',
        actions: ['replacement_action'],
        properties: ['action', 'sharedOnly'],
        required: ['action']
      },
      {
        name: 'without_enum',
        actions: [],
        properties: ['action', 'sharedOnly'],
        required: ['action']
      }
    ]);
  });
});
