import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../catalog/consolidated-tool-definitions.js';
import { generatedParentToolDefinitions } from '../catalog/capabilities/generated/parent-tool-definitions.generated.js';
import { isRecord } from '../../utils/validation/type-guards.js';

function schemaProperties(inputSchema: Record<string, unknown>, label: string): Record<string, unknown> {
  const properties = inputSchema.properties;
  if (!isRecord(properties)) {
    throw new Error(`${label} properties must be a schema object`);
  }
  return properties;
}

// `params` is a TypeScript-only passthrough added by the runtime facade. The
// generated parent definitions feed the neutral/native surface and must stay
// free of it, so each half is asserted against the artifact that owns it.
describe('public facade and generated parent schemas', () => {
  it('advertises params on every public action tool', () => {
    const withoutParams = consolidatedToolDefinitions
      .filter((definition) => !('params' in schemaProperties(definition.inputSchema, definition.name)))
      .map((definition) => definition.name);

    expect(withoutParams).toEqual([]);
  });

  it('opens additionalProperties on every public action tool', () => {
    const closed = consolidatedToolDefinitions
      .filter((definition) => definition.inputSchema.additionalProperties !== true)
      .map((definition) => definition.name);

    expect(closed).toEqual([]);
  });

  it('keeps params out of the neutral generated parent definitions', () => {
    const withParams = generatedParentToolDefinitions
      .filter((definition) => 'params' in schemaProperties(definition.inputSchema, definition.name))
      .map((definition) => definition.name);

    expect(withParams).toEqual([]);
  });

  it('advertises only trace connection types backed by live bridge execution', () => {
    const definitions = [
      consolidatedToolDefinitions.find((definition) => definition.name === 'system_control'),
      generatedParentToolDefinitions.find((definition) => definition.name === 'system_control')
    ];

    for (const definition of definitions) {
      expect(definition).toBeDefined();
      if (definition === undefined) {
        throw new Error('system_control must be defined on both surfaces');
      }

      const connectionType = schemaProperties(definition.inputSchema, 'system_control').connectionType;
      expect(isRecord(connectionType)).toBe(true);
      if (!isRecord(connectionType)) {
        throw new Error('connectionType must be a schema object');
      }

      expect(connectionType.enum).toEqual(['file', 'network']);
    }
  });
});
