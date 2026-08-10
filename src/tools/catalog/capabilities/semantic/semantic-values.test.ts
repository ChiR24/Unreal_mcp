import { describe, expect, it } from 'vitest';
import { FrameRangeSchema, TimeRangeSchema } from './frame-time.js';
import {
  LinearColorSchema,
  RotatorSchema,
  TransformSchema,
  Vector3Schema
} from './geometry.js';
import { PaginationSchema } from './pagination.js';
import { MetadataSchema, PropertyAssignmentSchema } from './property-assignment.js';
import { SavePolicySchema } from './save-policy.js';

void RotatorSchema;
void TransformSchema;

describe('geometry boundary parsing', () => {
  it('rejects out-of-range color channels (wrong-unit 0-255 input)', () => {
    const result = LinearColorSchema.safeParse({ r: 5, g: 0, b: 0 });
    expect(result.success).toBe(false);
  });

  it('accepts in-range linear color with optional alpha', () => {
    expect(LinearColorSchema.parse({ r: 1, g: 0.5, b: 0, a: 1 })).toEqual({
      r: 1,
      g: 0.5,
      b: 0,
      a: 1
    });
    expect(LinearColorSchema.parse({ r: 0, g: 0, b: 0 })).toEqual({ r: 0, g: 0, b: 0 });
  });

  it('rejects non-finite vector components', () => {
    expect(Vector3Schema.safeParse({ x: 1, y: NaN, z: 0 }).success).toBe(false);
    expect(Vector3Schema.safeParse({ x: 1, y: Infinity, z: 0 }).success).toBe(false);
  });

  it('accepts a finite transform', () => {
    const transform = TransformSchema.parse({
      location: { x: 0, y: 0, z: 0 },
      rotation: { pitch: 0, yaw: 90, roll: 0 },
      scale: { x: 1, y: 1, z: 1 }
    });
    expect(transform.rotation?.yaw).toBe(90);
  });

  it('rejects an unknown field on a vector (strict object)', () => {
    expect(Vector3Schema.safeParse({ x: 0, y: 0, z: 0, w: 1 }).success).toBe(false);
  });

  it('rejects a missing required color channel', () => {
    expect(LinearColorSchema.safeParse({ r: 0, g: 0 }).success).toBe(false);
  });
});

describe('frame / time range ordering', () => {
  it('rejects a reversed frame range', () => {
    expect(FrameRangeSchema.safeParse({ startFrame: 10, endFrame: 5 }).success).toBe(false);
  });

  it('accepts an equal frame range (boundary)', () => {
    expect(FrameRangeSchema.safeParse({ startFrame: 5, endFrame: 5 }).success).toBe(true);
  });

  it('accepts an ascending frame range', () => {
    expect(FrameRangeSchema.parse({ startFrame: 1, endFrame: 5 })).toEqual({
      startFrame: 1,
      endFrame: 5
    });
  });

  it('rejects a reversed time range', () => {
    expect(TimeRangeSchema.safeParse({ startMs: 5000, endMs: 1000 }).success).toBe(false);
  });

  it('accepts an equal time range (boundary)', () => {
    expect(TimeRangeSchema.safeParse({ startMs: 1000, endMs: 1000 }).success).toBe(true);
  });
});

describe('bounded pagination', () => {
  it('rejects an oversized page size', () => {
    expect(PaginationSchema.safeParse({ page: 1, pageSize: 10000 }).success).toBe(false);
  });

  it('rejects a non-positive page (lower bound)', () => {
    expect(PaginationSchema.safeParse({ page: 0, pageSize: 10 }).success).toBe(false);
  });

  it('accepts a bounded page size', () => {
    expect(PaginationSchema.parse({ page: 2, pageSize: 50 })).toEqual({ page: 2, pageSize: 50 });
  });
});

describe('save policy', () => {
  it('accepts every save policy value', () => {
    for (const value of ['immediate', 'deferred', 'none', 'user_prompt'] as const) {
      expect(SavePolicySchema.parse(value)).toBe(value);
    }
  });

  it('rejects an unknown save policy', () => {
    expect(SavePolicySchema.safeParse('autosave').success).toBe(false);
  });
});

describe('property assignment / metadata JSON boundary', () => {
  it('rejects a non-JSON-safe property value', () => {
    expect(PropertyAssignmentSchema.safeParse({ name: 'health', value: () => 1 }).success).toBe(false);
  });

  it('accepts a JSON-safe property value', () => {
    expect(PropertyAssignmentSchema.parse({ name: 'health', value: { max: 100 } })).toEqual({
      name: 'health',
      value: { max: 100 }
    });
  });

  it('rejects an empty metadata key', () => {
    expect(MetadataSchema.safeParse({ '': 1 }).success).toBe(false);
  });

  it('accepts structured metadata', () => {
    expect(MetadataSchema.parse({ owner: 'level', tags: ['a', 'b'] })).toEqual({
      owner: 'level',
      tags: ['a', 'b']
    });
  });
});
