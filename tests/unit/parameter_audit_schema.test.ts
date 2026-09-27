/// <reference types="node" />

import fs from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';

import { generatedParentToolDefinitions } from '../../src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.js';
import { createTrackedTempRoot, registerTempRootCleanup } from './audit-fixture-workspace.js';
import { ALL_CAPABILITY_RECORDS } from '../../src/tools/catalog/capabilities/records/aggregate.js';
import { compareAscii } from '../../src/utils/serialization/ordering.js';

registerTempRootCleanup();

const stringArray = (value: unknown): string[] =>
  Array.isArray(value) ? value.filter((item): item is string => typeof item === 'string') : [];

function canonicalSchema(tool: (typeof generatedParentToolDefinitions)[number]) {
  const properties = tool.inputSchema['properties'];
  const propertyRecord = properties && typeof properties === 'object' && !Array.isArray(properties) ? properties as Record<string, unknown> : {};
  const action = propertyRecord['action'];
  const actionRecord = action && typeof action === 'object' && !Array.isArray(action) ? action as Record<string, unknown> : {};
  // A folded family still serves the old names it replaced; the extractor lists
  // them beside the advertised enum, so the expected shape carries them too.
  const foldedActions = [...new Set(ALL_CAPABILITY_RECORDS
    .filter((record) => String(record.routing.parentTool) === tool.name)
    .flatMap((record) => record.legacyIds.filter((legacy) => legacy.folded !== undefined).map((legacy) => String(legacy.action))))].sort();
  return {
    name: tool.name,
    actions: stringArray(actionRecord['enum']).sort(compareAscii),
    foldedActions,
    properties: Object.keys(propertyRecord).sort(),
    required: stringArray(tool.inputSchema['required'])
  };
}

describe('parameter audit schema discovery', () => {
  it('matches every canonical tool schema exactly', async () => {
    const { extractToolSchemas } = await import('../parameter-audit-schema.mjs');
    const schemas = extractToolSchemas();

    expect(schemas).toEqual(
      generatedParentToolDefinitions
        .map(canonicalSchema)
        .sort((left, right) => compareAscii(left.name, right.name))
    );
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

});
