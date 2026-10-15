import { describe, expect, it } from 'vitest';

import { validateAgainstCapabilitySchema } from './gateway-schema-validate.js';

// manage_blueprint create sent { path } answered only "Missing required parameter 'savePath'", and path looked accepted.
describe('a missing required parameter names the keys sent in its place', () => {
  const schema = {
    type: 'object',
    properties: { name: { type: 'string' }, savePath: { type: 'string' } },
    required: ['name', 'savePath'],
    additionalProperties: false
  };

  it('says a stray key is not a parameter of the action', () => {
    const violation = validateAgainstCapabilitySchema({ name: 'BP_A', path: '/Game/Temp' }, schema);
    expect(violation?.reason).toBe('missing-required');
    expect(violation?.message).toBe("Missing required parameter 'savePath'; sent path, which this action does not take");
  });

  it('stays plain when every key sent is a parameter', () => {
    expect(validateAgainstCapabilitySchema({ name: 'BP_A' }, schema)?.message).toBe("Missing required parameter 'savePath'");
  });
});
