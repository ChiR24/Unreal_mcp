import type { EnvironmentArgs, Vector3 } from '../../../types/handlers/handler-types.js';
import { normalizePathFields } from '../foundation/dispatch/common-handlers.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

/** Location item in foliage locations array */
export interface LocationItem {
  x?: number;
  y?: number;
  z?: number;
}

interface ProceduralFoliageBounds {
  location?: Vector3;
  size?: Vector3;
  min?: Vector3;
  max?: Vector3;
}

/** Convert Vector3 to array format expected by some tools */
export function vec3ToArray(v: Vector3 | undefined): [number, number, number] | undefined {
  if (!v) return undefined;
  return [v.x ?? 0, v.y ?? 0, v.z ?? 0];
}

/** Convert Vector3 to object format expected by C++ handlers */
export function vec3ToObject(v: Vector3 | undefined): { x: number; y: number; z: number } | undefined {
  if (!v) return undefined;
  return { x: v.x ?? 0, y: v.y ?? 0, z: v.z ?? 0 };
}

function isVector3(value: unknown): value is Vector3 {
  return typeof value === 'object' && value !== null && (
    'x' in value || 'y' in value || 'z' in value
  );
}

export function getNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

export function getString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

const ENVIRONMENT_PATH_FIELDS_BY_ACTION: Record<string, readonly string[]> = {
  create_landscape: ['materialPath', 'path'],
  modify_heightmap: ['landscapePath'],
  sculpt: ['landscapePath'],
  sculpt_landscape: ['landscapePath'],
  import_heightmap: ['landscapePath', 'landscapeActorPath'],
  export_heightmap: ['landscapePath', 'landscapeActorPath'],
  add_foliage: ['foliageType', 'foliageTypePath', 'meshPath'],
  create_foliage_type: ['foliageType', 'foliageTypePath', 'meshPath', 'staticMesh', 'path'],
  configure_foliage_mesh: ['foliageType', 'foliageTypePath', 'meshPath', 'staticMesh'],
  configure_foliage_placement: ['foliageType', 'foliageTypePath'],
  configure_foliage_lod: ['foliageType', 'foliageTypePath'],
  configure_foliage_collision: ['foliageType', 'foliageTypePath'],
  configure_foliage_culling: ['foliageType', 'foliageTypePath'],
  paint_foliage: ['foliageType', 'foliageTypePath'],
  paint_foliage_instances: ['foliageType', 'foliageTypePath'],
  remove_foliage_instances: ['foliageType', 'foliageTypePath'],
  add_foliage_instances: ['foliageType', 'foliageTypePath', 'meshPath'],
  create_procedural_terrain: ['material', 'path'],
  create_procedural_foliage: ['path'],
  set_landscape_material: ['landscapePath', 'materialPath'],
  configure_landscape_material: ['landscapePath', 'landscapeActorPath', 'materialPath'],
  configure_landscape_splines: ['landscapePath', 'landscapeActorPath'],
  create_landscape_layer_info: ['path', 'physicalMaterialPath'],
  create_landscape_grass_type: ['meshPath', 'path', 'staticMesh'],
  generate_lods: ['assetPath', 'landscapePath', 'path'],
  configure_landscape_lod: ['assetPath', 'landscapePath', 'path'],
  create_sky_sphere: ['path'],
  configure_sky_light: ['cubemapPath'],
  create_fog_volume: ['path'],
  create_weather_system: ['particleSystemPath'],
  configure_rain_particles: ['particleSystemPath'],
  configure_snow_particles: ['particleSystemPath'],
  configure_lightning: ['particleSystemPath'],
  configure_light_color_curve: ['curvePath'],
  configure_sky_color_curve: ['curvePath'],
  create_water_body_ocean: ['materialPath'],
  create_water_body_lake: ['materialPath'],
  create_water_body_river: ['materialPath'],
  create_water_body_custom: ['materialPath'],
  configure_water_material: ['materialPath'],
  create_buoyancy_component: ['actorPath']
};

function normalizePathValue(value: unknown): unknown {
  if (typeof value !== 'string' || value.length === 0) return value;
  const normalized = normalizePathFields({ path: value }, ['path']).path;
  return typeof normalized === 'string' ? normalized : value;
}

function normalizePathArray(value: unknown): unknown {
  return Array.isArray(value) ? value.map(normalizePathValue) : value;
}

function normalizeFoliageTypes(value: unknown): unknown {
  if (!Array.isArray(value)) return value;
  return value.map(entry => {
    if (!isRecord(entry) || typeof entry.meshPath !== 'string') return entry;
    return { ...entry, meshPath: normalizePathValue(entry.meshPath) };
  });
}

export function normalizeEnvironmentPathArgs(action: string, args: Record<string, unknown>): Record<string, unknown> {
  const pathFields = ENVIRONMENT_PATH_FIELDS_BY_ACTION[action] ?? [];
  const normalized = pathFields.length > 0 ? normalizePathFields(args, pathFields) : { ...args };
  if (action === 'generate_lods') {
    normalized.assetPaths = normalizePathArray(normalized.assetPaths);
    normalized.assets = normalizePathArray(normalized.assets);
  } else if (action === 'create_procedural_foliage') {
    normalized.foliageTypes = normalizeFoliageTypes(normalized.foliageTypes);
    normalized.types = normalizeFoliageTypes(normalized.types);
  }
  return normalized;
}

export function buildRegionFromTopLevel(args: Record<string, unknown>): { minX: number; minY: number; maxX: number; maxY: number } | undefined {
  const minX = getNumber(args.minX);
  const minY = getNumber(args.minY);
  const maxX = getNumber(args.maxX);
  const maxY = getNumber(args.maxY);
  return minX !== undefined && minY !== undefined && maxX !== undefined && maxY !== undefined
    ? { minX, minY, maxX, maxY }
    : undefined;
}

export function normalizeProceduralFoliageBounds(value: unknown): { location?: Vector3; size?: Vector3 } | undefined {
  if (typeof value !== 'object' || value === null) return undefined;
  const bounds = value as ProceduralFoliageBounds;
  if (isVector3(bounds.location) || isVector3(bounds.size)) {
    return {
      location: isVector3(bounds.location) ? bounds.location : undefined,
      size: isVector3(bounds.size) ? bounds.size : undefined
    };
  }

  if (!isVector3(bounds.min) || !isVector3(bounds.max)) return undefined;

  return {
    location: bounds.min,
    size: {
      x: (bounds.max.x ?? 0) - (bounds.min.x ?? 0),
      y: (bounds.max.y ?? 0) - (bounds.min.y ?? 0),
      z: (bounds.max.z ?? 0) - (bounds.min.z ?? 0)
    }
  };
}

export type { EnvironmentArgs, Vector3 };
