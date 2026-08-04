// requiredOneOf — at-least-one-of object-schema keyword.
//
// Both validators must agree: a schema declaring `requiredOneOf: ['a', 'b']`
// refuses a value carrying none of the listed properties with the shared
// MISSING_REQUIRED_ONEOF code, accepts a value carrying at least one, and treats
// the keyword as supported (never an 'unsupported-keyword' refusal).

import { describe, expect, it } from 'vitest';

import { validateAgainstSubset } from './schema-subset.js';
import { validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-execute-validate.js';

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

const run = (surface: 'spec' | 'gateway') =>
  surface === 'spec' ? validateAgainstSubset : validateAgainstCapabilitySchema;

describe('requiredOneOf (at-least-one semantics)', () => {
  it.each(['spec', 'gateway'] as const)(
    'refuses a value with none of the listed properties on the %s surface',
    (surface) => {
      const violation = run(surface)({}, AT_LEAST_ONE_SCHEMA);
      expect(violation?.reason).toBe('required-one-of');
      expect(violation?.pointer).toBe('/requiredOneOf');
      expect(violation?.message).toBe('At least one of [a, b] must be provided');
    }
  );

  it.each(['spec', 'gateway'] as const)(
    'accepts a value carrying at least one listed property on the %s surface',
    (surface) => {
      expect(run(surface)({ a: 'x' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
      expect(run(surface)({ b: 'y' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
      expect(run(surface)({ a: 'x', b: 'y' }, AT_LEAST_ONE_SCHEMA)).toBeUndefined();
    }
  );

  it.each(['spec', 'gateway'] as const)(
    'treats requiredOneOf as a supported keyword on the %s surface',
    (surface) => {
      const violation = run(surface)({ a: 'x' }, AT_LEAST_ONE_SCHEMA);
      expect(violation?.reason).not.toBe('unsupported-keyword');
    }
  );
});
