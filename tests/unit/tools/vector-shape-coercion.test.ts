// Dogfood #226: vector-shaped parameters are arrays on some parents and
// {x,y,z} / {width,height} objects on others, while every handler accepts both.
// The gateway converts between the shapes before validation instead of refusing.
//
// Matching is strict: partial vectors, over-long arrays, typo'd keys, and
// non-numeric components do NOT coerce. They keep their original shape so the
// schema validator reports a guided type error instead of the gateway shipping
// a silently truncated vector to a destructive editor path.
import { describe, expect, it } from 'vitest';
import { coerceVectorShapes, validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';

const ARRAY_LOCATION = {
  type: 'object',
  properties: {
    location: { type: 'array', items: { type: 'number' }, description: 'World location [x, y, z].' },
    name: { type: 'string' }
  },
  required: ['location'],
  additionalProperties: false
};

const OBJECT_LOCATION = {
  type: 'object',
  properties: {
    location: {
      type: 'object',
      additionalProperties: false,
      properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' } }
    },
    size: {
      type: 'object',
      additionalProperties: false,
      properties: { width: { type: 'number' }, height: { type: 'number' } }
    }
  },
  additionalProperties: false
};

const RGBA_OBJECT = {
  type: 'object',
  additionalProperties: false,
  properties: { color: { type: 'array', items: { type: 'number' } } }
};

describe('vector shape coercion', () => {
  it('turns an {x,y,z} object into the array the record declares', () => {
    const coerced = coerceVectorShapes({ location: { x: 1, y: 2, z: 3 }, name: 'A' }, ARRAY_LOCATION);
    expect(coerced).toEqual({ location: [1, 2, 3], name: 'A' });
    expect(validateAgainstCapabilitySchema(coerced, ARRAY_LOCATION)).toBeUndefined();
  });

  it('turns an [x,y,z] array into the object the record declares', () => {
    const coerced = coerceVectorShapes({ location: [4, 5, 6], size: [10, 20] }, OBJECT_LOCATION);
    expect(coerced).toEqual({ location: { x: 4, y: 5, z: 6 }, size: { width: 10, height: 20 } });
    expect(validateAgainstCapabilitySchema(coerced, OBJECT_LOCATION)).toBeUndefined();
  });

  it('leaves values that already match untouched', () => {
    const matching = { location: [1, 2, 3] };
    expect(coerceVectorShapes(matching, ARRAY_LOCATION)).toBe(matching);
  });

  it('refuses partial or malformed vectors instead of silently truncating them', () => {
    // A typo'd third component used to fall through to the [x,y] set and spawn
    // at z=0; it must keep the object shape and fail validation with a pointer.
    const malformed = { location: { x: 1, y: 2, z: '300' } };
    expect(coerceVectorShapes(malformed, ARRAY_LOCATION)).toBe(malformed);
    expect(validateAgainstCapabilitySchema(malformed, ARRAY_LOCATION)?.reason).toBe('type');

    const short = { location: [1] };
    expect(coerceVectorShapes(short, OBJECT_LOCATION)).toBe(short);
    const typoKey = { location: { x: 1, y: 2, z: 3, lbel: 'a' } };
    expect(coerceVectorShapes(typoKey, ARRAY_LOCATION)).toBe(typoKey);
    const overLong = { location: [1, 2, 3, 4] };
    expect(coerceVectorShapes(overLong, OBJECT_LOCATION)).toBe(overLong);
    expect(validateAgainstCapabilitySchema(overLong, OBJECT_LOCATION)?.reason).toBe('type');
  });

  it('accepts rotator and colour spellings too', () => {
    const schema = {
      type: 'object',
      properties: {
        rotation: { type: 'array', items: { type: 'number' } },
        color: { type: 'array', items: { type: 'number' } }
      },
      additionalProperties: false
    };
    expect(coerceVectorShapes({ rotation: { pitch: 1, yaw: 2, roll: 3 }, color: { r: 1, g: 0, b: 0 } }, schema))
      .toEqual({ rotation: [1, 2, 3], color: [1, 0, 0] });
  });

  it('treats the fourth RGBA component as optional and preserves it when present', () => {
    expect(coerceVectorShapes({ color: { r: 1, g: 0, b: 0, a: 0.5 } }, RGBA_OBJECT))
      .toEqual({ color: [1, 0, 0, 0.5] });
    expect(coerceVectorShapes({ color: { r: 1, g: 0, b: 0 } }, RGBA_OBJECT))
      .toEqual({ color: [1, 0, 0] });
    // A present-but-non-numeric alpha fails the set like any other component.
    const badAlpha = { color: { r: 1, g: 0, b: 0, a: 'x' } };
    expect(coerceVectorShapes(badAlpha, RGBA_OBJECT)).toBe(badAlpha);
  });

  it('does not fabricate an xyzw object for propertyless object parameters', () => {
    const schema = {
      type: 'object',
      properties: { payload: { type: 'object' } },
      additionalProperties: false
    };
    const bare = { payload: [1, 2, 3] };
    expect(coerceVectorShapes(bare, schema)).toBe(bare);
    expect(validateAgainstCapabilitySchema(bare, schema)?.reason).toBe('type');
  });

  it('coerces the xyzw object spelling when the record declares a w component', () => {
    const schema = {
      type: 'object',
      properties: { quaternion: { type: 'array', items: { type: 'number' } } },
      additionalProperties: false
    };
    expect(coerceVectorShapes({ quaternion: { x: 0, y: 0, z: 0, w: 1 } }, schema))
      .toEqual({ quaternion: [0, 0, 0, 1] });
  });
});
