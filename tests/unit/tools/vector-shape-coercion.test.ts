// Dogfood #226: vector-shaped parameters are arrays on some parents and
// {x,y,z} / {width,height} objects on others, while every handler accepts both.
// The gateway converts between the shapes before validation instead of refusing.
//
// Matching is strict: partial vectors, over-long arrays, typo'd keys, and
// non-numeric components do NOT coerce. They keep their original shape so the
// schema validator reports a guided type error instead of the gateway shipping
// a silently truncated vector to a destructive editor path.
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';
import { coerceVectorShapes, validateAgainstCapabilitySchema } from '../../../src/server/gateway/gateway-schema-validate.js';

const REGISTRY_PATH = resolve(
  dirname(fileURLToPath(import.meta.url)),
  '../../../src/tools/catalog/capabilities/generated/canonical-registry.generated.json'
);

const isPlainObject = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

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

  it('coerces vectors inside batch items the way it coerces the single form', () => {
    const schema = {
      type: 'object',
      properties: {
        actors: { type: 'array', items: ARRAY_LOCATION }
      },
      additionalProperties: false
    };
    const coerced = coerceVectorShapes({ actors: [{ location: { x: 1, y: 2, z: 3 } }, { location: [4, 5, 6] }] }, schema);
    expect(coerced).toEqual({ actors: [{ location: [1, 2, 3] }, { location: [4, 5, 6] }] });
    const untouched = { actors: [{ location: [4, 5, 6] }] };
    expect(coerceVectorShapes(untouched, schema)).toBe(untouched);
  });

  it('refuses a two-number array for an {x,y,z} object instead of emitting {x,y}', () => {
    // build_environment.create_light {location: [100, 200]} reached the handler as {x: 100, y: 200} — the
    // ['x','y'] set matched a schema that also declares z — and the light spawned at (100, 200, 0). A
    // shorter key set must never stand in for a longer one the schema declares.
    const twoNumbers = { location: [0, -980] };
    expect(coerceVectorShapes(twoNumbers, OBJECT_LOCATION)).toBe(twoNumbers);
    expect(validateAgainstCapabilitySchema(twoNumbers, OBJECT_LOCATION)?.reason).toBe('type');

    const xyzw = {
      type: 'object',
      properties: {
        vector4: {
          type: 'object',
          properties: { x: { type: 'number' }, y: { type: 'number' }, z: { type: 'number' }, w: { type: 'number' } }
        }
      },
      additionalProperties: false
    };
    const twoOfFour = { vector4: [0, 1] };
    expect(coerceVectorShapes(twoOfFour, xyzw)).toBe(twoOfFour);
    expect(coerceVectorShapes({ vector4: [0, 0, 0, 1] }, xyzw)).toEqual({ vector4: { x: 0, y: 0, z: 0, w: 1 } });
  });

  it('still coerces a two-number array when the schema declares only x and y', () => {
    const schema = {
      type: 'object',
      properties: { nodePosition: { type: 'object', properties: { x: { type: 'number' }, y: { type: 'number' } } } },
      additionalProperties: false
    };
    expect(coerceVectorShapes({ nodePosition: [120, -40] }, schema)).toEqual({ nodePosition: { x: 120, y: -40 } });
  });

  it('reads a three-number array as pitch, yaw, roll when a rotation also declares x,y,z aliases', () => {
    // sequence.cinematic.create_cinematic_asset declares rotation {pitch,yaw,roll,x,y,z}; its handler reads
    // pitch/yaw/roll only, so {x,y,z} spawned the camera unrotated while reporting rotationApplied: true.
    const schema = {
      type: 'object',
      properties: {
        rotation: {
          type: 'object',
          properties: Object.fromEntries(['pitch', 'yaw', 'roll', 'x', 'y', 'z'].map((key) => [key, { type: 'number' }]))
        }
      },
      additionalProperties: false
    };
    expect(coerceVectorShapes({ rotation: [1, 2, 3] }, schema)).toEqual({ rotation: { pitch: 1, yaw: 2, roll: 3 } });
    const twoNumbers = { rotation: [1, 2] };
    expect(coerceVectorShapes(twoNumbers, schema)).toBe(twoNumbers);
  });

  it('turns no two-number array into a partial object for any served {x,y,z} parameter', () => {
    // Every coercion-reachable object parameter of the generated canonical registry — the records the TS
    // gateway and the native capability shards both serve — at the top level and inside batch items.
    const registry = JSON.parse(readFileSync(REGISTRY_PATH, 'utf8')) as {
      records: ReadonlyArray<{ id: string; schemas: { input: Record<string, unknown> } }>;
    };
    type Wrap = (inner: Record<string, unknown>) => Record<string, unknown>;
    const sites: Array<{ id: string; name: string; declared: string[]; input: Record<string, unknown>; wrap: Wrap }> = [];
    const walk = (id: string, input: Record<string, unknown>, schema: unknown, wrap: Wrap): void => {
      if (!isPlainObject(schema) || !isPlainObject(schema.properties)) return;
      for (const [name, property] of Object.entries(schema.properties)) {
        if (!isPlainObject(property)) continue;
        const types = typeof property.type === 'string' ? [property.type] : Array.isArray(property.type) ? property.type : [];
        const declared = isPlainObject(property.properties) ? Object.keys(property.properties) : [];
        if (types.includes('object') && !types.includes('array') && ['x', 'y', 'z'].every((key) => declared.includes(key))) {
          sites.push({ id, name, declared, input, wrap });
        } else if (types.includes('array') && isPlainObject(property.items)) {
          walk(id, input, property.items, (inner) => wrap({ [name]: [inner] }));
        }
      }
    };
    for (const record of registry.records) walk(record.id, record.schemas.input, record.schemas.input, (inner) => inner);

    expect(sites.some((site) => site.id === 'build_environment.create_light' && site.name === 'location')).toBe(true);
    const fabricated: string[] = [];
    const lostThreeNumbers: string[] = [];
    for (const site of sites) {
      const twoNumbers = site.wrap({ [site.name]: [0, -980] });
      if (coerceVectorShapes(twoNumbers, site.input) !== twoNumbers) fabricated.push(`${site.id} ${site.name}`);
      // A rotation that also declares x/y/z aliases reads [pitch, yaw, roll]; every other site reads [x, y, z].
      const expected = site.declared.includes('pitch') ? { pitch: 1, yaw: 2, roll: 3 } : { x: 1, y: 2, z: 3 };
      const threeNumbers = coerceVectorShapes(site.wrap({ [site.name]: [1, 2, 3] }), site.input);
      if (JSON.stringify(threeNumbers) !== JSON.stringify(site.wrap({ [site.name]: expected }))) {
        lostThreeNumbers.push(`${site.id} ${site.name}`);
      }
    }
    expect(fabricated).toEqual([]);
    expect(lostThreeNumbers).toEqual([]);
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
