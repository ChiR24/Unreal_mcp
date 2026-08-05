import { describe, expect, it } from 'vitest';

import { Draft202012ObjectSchemaSchema } from './json-schema.js';

const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema' as const;

function buildSchema(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties: { a: { type: 'string' }, b: { type: 'string' } },
    required: [],
    additionalProperties: false,
    ...overrides
  };
}

describe('Draft202012ObjectSchemaSchema requiredOneOf', () => {
  it('accepts members that are all declared in properties', () => {
    const result = Draft202012ObjectSchemaSchema.safeParse(
      buildSchema({ requiredOneOf: ['a', 'b'] })
    );
    expect(result.success).toBe(true);
  });

  it('rejects an empty requiredOneOf array (min 1)', () => {
    const result = Draft202012ObjectSchemaSchema.safeParse(buildSchema({ requiredOneOf: [] }));
    expect(result.success).toBe(false);
    expect(result.error?.issues.some((issue) => issue.code === 'too_small')).toBe(true);
  });

  it('rejects a member that does not reference a declared property', () => {
    const result = Draft202012ObjectSchemaSchema.safeParse(
      buildSchema({ requiredOneOf: ['a', 'missing'] })
    );
    expect(result.success).toBe(false);
    const message = result.error?.issues.map((issue) => issue.message).join(' ');
    expect(message).toContain('missing');
    expect(message).toContain('declared property');
  });

  it('parses when requiredOneOf is omitted (keyword optional)', () => {
    const result = Draft202012ObjectSchemaSchema.safeParse(buildSchema());
    expect(result.success).toBe(true);
  });

  it('accepts declared members when properties hold plain object schemas', () => {
    const result = Draft202012ObjectSchemaSchema.safeParse(
      buildSchema({
        properties: { a: { type: 'string' }, b: { type: 'string' } },
        requiredOneOf: ['a', 'b']
      })
    );
    expect(result.success).toBe(true);
  });
});
