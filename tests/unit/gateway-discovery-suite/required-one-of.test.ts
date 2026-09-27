// requiredOneOf — at-least-one-of object-schema keyword.
//
// A schema declaring `requiredOneOf: ['a', 'b']` refuses a value carrying none
// of the listed properties, accepts a value carrying at least one, and treats
// the keyword as supported (never an 'unsupported-keyword' refusal).

import { describe, expect, it } from 'vitest';

import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';

const AT_LEAST_ONE_SCHEMA = {
  $schema: 'https://json-schema.org/draft/2020-12/schema',
  type: 'object',
  properties: {
    a: { type: 'string' },
    b: { type: 'string' }
  },
  required: [],
  additionalProperties: false,
  requiredOneOf: ['a', 'b']
};

describe('requiredOneOf (at-least-one semantics)', () => {
  it('refuses a value with none of the listed properties', () => {
      const violation = validateAgainstCapabilitySchema({}, AT_LEAST_ONE_SCHEMA);
      expect(violation?.reason).toBe('required-one-of');
      expect(violation?.pointer).toBe('/requiredOneOf');
      expect(violation?.message).toBe('At least one of [a, b] must be provided');
  });

  it('accepts a value carrying at least one listed property', () => {
      expect(validateAgainstCapabilitySchema({ a: 'x' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
      expect(validateAgainstCapabilitySchema({ b: 'y' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
      expect(validateAgainstCapabilitySchema({ a: 'x', b: 'y' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
  });

  it('treats requiredOneOf as a supported keyword', () => {
      const violation = validateAgainstCapabilitySchema({ a: 'x' }, AT_LEAST_ONE_SCHEMA);
      expect(violation?.reason).not.toBe('unsupported-keyword');
  });
});
