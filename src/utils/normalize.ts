export type Vec3Array = [number, number, number];
export type Rot3Array = [number, number, number];
export interface Vec3Obj { x: number; y: number; z: number; }
export interface Rot3Obj { pitch: number; yaw: number; roll: number; }

export function toVec3Object(input: any): Vec3Obj | null {
  try {
    if (Array.isArray(input) && input.length === 3) {
      const [x, y, z] = input;
      if ([x, y, z].every(v => typeof v === 'number' && isFinite(v))) {
        return { x, y, z };
      }
    }
    if (input && typeof input === 'object') {
      const x = Number((input as any).x ?? (input as any).X);
      const y = Number((input as any).y ?? (input as any).Y);
      const z = Number((input as any).z ?? (input as any).Z);
      if ([x, y, z].every(v => typeof v === 'number' && !isNaN(v) && isFinite(v))) {
        return { x, y, z };
      }
    }
  } catch {}
  return null;
}

export function toVec3Array(input: any): Vec3Array | null {
  const obj = toVec3Object(input);
  return obj ? [obj.x, obj.y, obj.z] : null;
}

export function toRotObject(input: any): Rot3Obj | null {
  try {
    if (Array.isArray(input) && input.length === 3) {
      const [pitch, yaw, roll] = input;
      if ([pitch, yaw, roll].every(v => typeof v === 'number' && isFinite(v))) {
        return { pitch, yaw, roll };
      }
    }
    if (input && typeof input === 'object') {
      const pitch = Number((input as any).pitch ?? (input as any).Pitch);
      const yaw = Number((input as any).yaw ?? (input as any).Yaw);
      const roll = Number((input as any).roll ?? (input as any).Roll);
      if ([pitch, yaw, roll].every(v => typeof v === 'number' && !isNaN(v) && isFinite(v))) {
        return { pitch, yaw, roll };
      }
    }
  } catch {}
  return null;
}

export function toRotArray(input: any): Rot3Array | null {
  const obj = toRotObject(input);
  return obj ? [obj.pitch, obj.yaw, obj.roll] : null;
}
