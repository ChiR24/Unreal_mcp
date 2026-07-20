/**
 * tests/unit/parameter-audit-schema-fixtures.ts
 *
 * Fixture builders and the canonical-schema projection for
 * parameter_audit_schema.test.ts. Extracted verbatim so the test file keeps
 * only its assertions and stays under the project 250 pure-LOC ceiling.
 */
/// <reference types="node" />

import { createTrackedTempRoot, writeFixtureFiles } from './audit-fixture-workspace.js';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';

/** Two parent tools whose schemas arrive through aliased, same-named nested helpers. */
export function createDefinitionsFixture(): string {
  const root = createTrackedTempRoot('parameter-audit-schema-');

  const files = new Map<string, string>([
    ['shared/actions.ts', "export const SHARED_ACTIONS = ['shared_action'] as const;\n"],
    [
      'first/properties.ts',
      [
        'export const commonProperties = {',
        "  action: { type: 'string', enum: ['first_action'] },",
        "  firstOnly: { type: 'string' }",
        '};',
        ''
      ].join('\n')
    ],
    [
      'first/schema.ts',
      [
        "import { commonProperties as importedProperties } from './properties.js';",
        'export const inputSchema = {',
        "  type: 'object',",
        '  properties: { ...importedProperties },',
        "  required: ['action', 'firstOnly']",
        '};',
        ''
      ].join('\n')
    ],
    [
      'first/first-tool.ts',
      [
        "import { inputSchema as aliasedSchema } from './schema.js';",
        'export const firstToolDefinition = {',
        "  name: 'first_tool',",
        '  inputSchema: aliasedSchema',
        '};',
        ''
      ].join('\n')
    ],
    [
      'second/properties.ts',
      [
        "import { SHARED_ACTIONS as aliasedActions } from '../shared/actions.js';",
        'export const commonProperties = {',
        "  action: { type: 'string', enum: [...aliasedActions, 'second_action'] },",
        "  secondOnly: { type: 'number' }",
        '};',
        ''
      ].join('\n')
    ],
    [
      'second/schema.ts',
      [
        "import { commonProperties as importedProperties } from './properties.js';",
        'export const inputSchema = {',
        "  type: 'object',",
        '  properties: { ...importedProperties },',
        "  required: ['action']",
        '};',
        ''
      ].join('\n')
    ],
    [
      'second/second-tool.ts',
      [
        "import { inputSchema as aliasedSchema } from './schema.js';",
        'export const secondToolDefinition = {',
        "  name: 'second_tool',",
        '  inputSchema: aliasedSchema',
        '};',
        ''
      ].join('\n')
    ]
  ]);

  writeFixtureFiles(root, files);

  return root;
}

/** Three parent tools whose explicit action entries contend with spread-provided enums. */
export function createActionOverrideFixture(): string {
  const root = createTrackedTempRoot('parameter-audit-action-override-');

  const files = new Map<string, string>([
    [
      'shared/base-properties.ts',
      [
        'export const baseProperties = {',
        "  action: { type: 'string', enum: ['spread_action'] },",
        "  sharedOnly: { type: 'boolean' }",
        '};',
        ''
      ].join('\n')
    ],
    [
      'shared/enumless-properties.ts',
      [
        'export const enumlessProperties = {',
        "  action: { type: 'string', description: 'Replacement without enum' }",
        '};',
        ''
      ].join('\n')
    ],
    [
      'without-enum/without-enum-tool.ts',
      [
        "import { baseProperties } from '../shared/base-properties.js';",
        'export const withoutEnumToolDefinition = {',
        "  name: 'without_enum',",
        '  inputSchema: {',
        '    properties: {',
        '      ...baseProperties,',
        "      action: { type: 'string', description: 'Replacement without enum' }",
        '    },',
        "    required: ['action']",
        '  }',
        '};',
        ''
      ].join('\n')
    ],
    [
      'nested-spread/nested-spread-tool.ts',
      [
        "import { baseProperties } from '../shared/base-properties.js';",
        "import { enumlessProperties } from '../shared/enumless-properties.js';",
        'export const nestedSpreadToolDefinition = {',
        "  name: 'nested_spread',",
        '  inputSchema: {',
        '    properties: {',
        '      ...baseProperties,',
        '      ...enumlessProperties',
        '    },',
        "    required: ['action']",
        '  }',
        '};',
        ''
      ].join('\n')
    ],
    [
      'with-enum/with-enum-tool.ts',
      [
        "import { baseProperties } from '../shared/base-properties.js';",
        'export const withEnumToolDefinition = {',
        "  name: 'with_enum',",
        '  inputSchema: {',
        '    properties: {',
        '      ...baseProperties,',
        "      action: { type: 'string', enum: ['replacement_action'] }",
        '    },',
        "    required: ['action']",
        '  }',
        '};',
        ''
      ].join('\n')
    ]
  ]);

  writeFixtureFiles(root, files);

  return root;
}

function stringArray(value: unknown): string[] {
  if (!Array.isArray(value) || !value.every((entry) => typeof entry === 'string')) {
    return [];
  }
  return [...value].sort();
}

/** Projects a canonical parent definition into the shape extractToolSchemas() reports. */
export function canonicalSchema(tool: (typeof consolidatedToolDefinitions)[number]) {
  const properties = tool.inputSchema['properties'];
  const propertyRecord =
    properties && typeof properties === 'object' && !Array.isArray(properties)
      ? properties as Record<string, unknown>
      : {};
  const action = propertyRecord['action'];
  const actionRecord =
    action && typeof action === 'object' && !Array.isArray(action)
      ? action as Record<string, unknown>
      : {};

  return {
    name: tool.name,
    actions: stringArray(actionRecord['enum']),
    properties: Object.keys(propertyRecord).sort(),
    required: stringArray(tool.inputSchema['required'])
  };
}
