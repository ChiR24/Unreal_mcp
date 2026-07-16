import { z } from 'zod';

// Wire-boundary geometry. Distinct from the permissive handler `Vector3`/`Rotator`/
// `Transform` runtime interfaces: these are branded (vector vs rotator vs color are
// not interchangeable) and finite/range-checked so a color channel can never be
// passed where a world-space vector is expected.

const FINITE = z.number().refine((n) => Number.isFinite(n), 'must be a finite number');

export const Vector3Schema = z
  .strictObject({ x: FINITE, y: FINITE, z: FINITE })
  .readonly()
  .brand<'Vector3'>();
export type Vector3 = z.infer<typeof Vector3Schema>;

export const RotatorSchema = z
  .strictObject({ pitch: FINITE, yaw: FINITE, roll: FINITE })
  .readonly()
  .brand<'Rotator'>();
export type Rotator = z.infer<typeof RotatorSchema>;

// Linear color channels are normalized 0..1 (not 0..255). Out-of-range input is a
// wrong-unit error, not a silent clamp.
const COLOR_CHANNEL = z.number().finite().min(0).max(1);

export const LinearColorSchema = z
  .strictObject({
    r: COLOR_CHANNEL,
    g: COLOR_CHANNEL,
    b: COLOR_CHANNEL,
    a: COLOR_CHANNEL.optional()
  })
  .readonly()
  .brand<'LinearColor'>();
export type LinearColor = z.infer<typeof LinearColorSchema>;

export const TransformSchema = z
  .strictObject({
    location: Vector3Schema.optional(),
    rotation: RotatorSchema.optional(),
    scale: Vector3Schema.optional()
  })
  .readonly()
  .brand<'Transform'>();
export type Transform = z.infer<typeof TransformSchema>;

export function parseVector3(input: unknown): Vector3 {
  return Vector3Schema.parse(input);
}

export function parseRotator(input: unknown): Rotator {
  return RotatorSchema.parse(input);
}

export function parseLinearColor(input: unknown): LinearColor {
  return LinearColorSchema.parse(input);
}

export function parseTransform(input: unknown): Transform {
  return TransformSchema.parse(input);
}
