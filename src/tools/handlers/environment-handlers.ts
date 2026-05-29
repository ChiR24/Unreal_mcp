import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EnvironmentArgs, Vector3 } from '../../types/handler-types.js';
import { executeAutomationRequest, normalizePathFields, validateArgsSecurity } from './common-handlers.js';
import { exportEnvironmentSnapshot, importEnvironmentSnapshot } from '../../utils/environment-snapshot.js';

/** Location item in foliage locations array */
interface LocationItem {
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
function vec3ToArray(v: Vector3 | undefined): [number, number, number] | undefined {
  if (!v) return undefined;
  return [v.x ?? 0, v.y ?? 0, v.z ?? 0];
}

/** Convert Vector3 to object format expected by C++ handlers */
function vec3ToObject(v: Vector3 | undefined): { x: number; y: number; z: number } | undefined {
  if (!v) return undefined;
  return { x: v.x ?? 0, y: v.y ?? 0, z: v.z ?? 0 };
}

function isVector3(value: unknown): value is Vector3 {
  return typeof value === 'object' && value !== null && (
    'x' in value || 'y' in value || 'z' in value
  );
}

function getNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function getString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

const ENVIRONMENT_PATH_FIELDS_BY_ACTION: Record<string, readonly string[]> = {
  create_landscape: ['materialPath', 'path'],
  modify_heightmap: ['landscapePath'],
  sculpt: ['landscapePath'],
  sculpt_landscape: ['landscapePath'],
  add_foliage: ['foliageType', 'foliageTypePath', 'meshPath'],
  paint_foliage: ['foliageType', 'foliageTypePath'],
  add_foliage_instances: ['foliageType', 'foliageTypePath', 'meshPath'],
  create_procedural_terrain: ['material', 'path'],
  create_procedural_foliage: ['path'],
  set_landscape_material: ['landscapePath', 'materialPath'],
  create_landscape_grass_type: ['meshPath', 'path', 'staticMesh'],
  generate_lods: ['assetPath', 'landscapePath', 'path'],
  create_sky_sphere: ['path'],
  create_fog_volume: ['path']
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

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

function normalizeEnvironmentPathArgs(action: string, args: Record<string, unknown>): Record<string, unknown> {
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

function buildRegionFromTopLevel(args: Record<string, unknown>): { minX: number; minY: number; maxX: number; maxY: number } | undefined {
  const minX = getNumber(args.minX);
  const minY = getNumber(args.minY);
  const maxX = getNumber(args.maxX);
  const maxY = getNumber(args.maxY);
  return minX !== undefined && minY !== undefined && maxX !== undefined && maxY !== undefined
    ? { minX, minY, maxX, maxY }
    : undefined;
}

function normalizeProceduralFoliageBounds(value: unknown): { location?: Vector3; size?: Vector3 } | undefined {
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

export async function handleEnvironmentTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  // SECURITY: Validate raw args FIRST before constructing payloads
  // This catches path traversal and other security violations in params that handlers might not use
  validateArgsSecurity(args);

  const envAction = String(action || '').toLowerCase();
  const argsRecord = normalizeEnvironmentPathArgs(envAction, args as Record<string, unknown>);
  const argsTyped = argsRecord as EnvironmentArgs;

  switch (envAction) {
    case 'create_landscape': {
      const componentCount = argsTyped.componentCount;
      const componentsX = typeof componentCount === 'object' ? componentCount.x : undefined;
      const componentsY = typeof componentCount === 'object' ? componentCount.y : undefined;
      return cleanObject(await executeAutomationRequest(tools, 'create_landscape', {
        name: argsTyped.name ?? '',
        location: vec3ToArray(argsTyped.location),
        sizeX: argsRecord.sizeX as number | undefined,
        sizeY: argsRecord.sizeY as number | undefined,
        quadsPerSection: (argsRecord.quadsPerSection as number | undefined) ?? argsTyped.sectionSize,
        sectionSize: argsTyped.sectionSize,
        sectionsPerComponent: argsTyped.sectionsPerComponent,
        componentCount: componentsX,
        componentsX,
        componentsY,
        materialPath: argsTyped.materialPath
      }) as Record<string, unknown>);
    }
    case 'modify_heightmap':
      return cleanObject(await executeAutomationRequest(tools, 'modify_heightmap', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        landscapePath: argsTyped.landscapePath || '',
        operation: (argsRecord.operation as string) || 'set',
        heightData: argsTyped.heightData ?? [],
        minX: (argsRecord.minX as number) ?? 0,
        minY: (argsRecord.minY as number) ?? 0,
        maxX: (argsRecord.maxX as number) ?? 0,
        maxY: (argsRecord.maxY as number) ?? 0,
        region: (argsRecord.region as { minX?: number; minY?: number; maxX?: number; maxY?: number } | undefined) ?? buildRegionFromTopLevel(argsRecord),
        updateNormals: argsRecord.updateNormals as boolean | undefined,
        skipFlush: argsRecord.skipFlush as boolean | undefined
      }) as Record<string, unknown>);
    case 'sculpt':
    case 'sculpt_landscape': {
      // Default to 'Raise' tool if not specified
      const tool = (argsRecord.tool as string) || 'Raise';
      return cleanObject(await executeAutomationRequest(tools, 'sculpt_landscape', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        landscapePath: argsTyped.landscapePath || '',
        tool,
        // C++ expects location as object {x, y, z}, not array
        location: vec3ToObject(argsTyped.location),
        radius: argsTyped.radius || 500,
        brushRadius: argsTyped.radius || 500,
        strength: (argsRecord.strength as number) || 0.5,
        falloff: argsRecord.falloff as number | undefined,
        brushFalloff: argsRecord.falloff as number | undefined,
        toolMode: tool,
        skipFlush: argsRecord.skipFlush as boolean | undefined
      }) as Record<string, unknown>);
    }
    case 'add_foliage': {
      // Check if this is adding a foliage TYPE (has meshPath) or INSTANCES (has locations/position)
      if (argsTyped.meshPath) {
        // Derive a better default name from mesh path if not provided
        const defaultName = argsTyped.meshPath.split('/').pop()?.split('.')[0] + '_Foliage_Type';
        return cleanObject(await executeAutomationRequest(tools, 'add_foliage_type', {
          name: argsTyped.foliageType || argsTyped.name || defaultName || 'NewFoliageType',
          meshPath: argsTyped.meshPath,
          density: argsTyped.density,
          minScale: argsTyped.minScale,
          maxScale: argsTyped.maxScale,
          alignToNormal: argsTyped.alignToNormal,
          randomYaw: argsTyped.randomYaw,
          cullDistance: argsTyped.cullDistance
        }) as Record<string, unknown>);
      } else {
        // Validate foliageType is provided
        const foliageType = argsTyped.foliageType || argsTyped.foliageTypePath;
        if (!foliageType) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: 'add_foliage requires either: (1) meshPath to create a new foliage type, or (2) foliageType/foliageTypePath to place instances of an existing type. Example foliage assets: /Game/StarterContent/Props/SM_Bush, /Engine/BasicShapes/Sphere'
          });
        }

        // Support location+radius to generate locations if explicit array not provided
        let locations = argsTyped.locations as Vector3[] | undefined;
        if (!locations && argsTyped.location && argsTyped.radius) {
          // Generate locations around the center point within radius
          const center = argsTyped.location;
          const radius = argsTyped.radius || 500;
          const count = argsTyped.density || (argsRecord.count as number) || 10;
          locations = [];
          for (let i = 0; i < count; i++) {
            const angle = Math.random() * Math.PI * 2;
            const dist = Math.random() * radius;
            locations.push({
              x: (center.x || 0) + Math.cos(angle) * dist,
              y: (center.y || 0) + Math.sin(angle) * dist,
              z: center.z || 0
            });
          }
        } else if (!locations && argsRecord.position) {
          locations = [argsRecord.position as Vector3];
        }

        // Validate we have locations to place
        if (!locations || locations.length === 0) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: 'add_foliage requires locations to place foliage instances. Provide: locations array, or location+radius, or position'
          });
        }

        return cleanObject(await executeAutomationRequest(tools, 'paint_foliage', {
          foliageType,
          locations
        }) as Record<string, unknown>);
      }
    }

    case 'add_foliage_instances': {
      const locationsRaw = argsTyped.locations as LocationItem[] | undefined;
      // C++ accepts location as object {x, y, z} or array [x, y, z]
      const transformsRaw = argsTyped.transforms ||
        (locationsRaw ? locationsRaw.map((l: LocationItem) => ({
          location: { x: l.x ?? 0, y: l.y ?? 0, z: l.z ?? 0 }
        })) : []);
      return cleanObject(await executeAutomationRequest(tools, 'add_foliage_instances', {
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || argsTyped.meshPath || '',
        transforms: transformsRaw as { location: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; scale?: { x: number; y: number; z: number } }[]
      }) as Record<string, unknown>);
    }
    case 'paint_foliage': {
      // Get locations array if provided
      const locations = argsTyped.locations as Vector3[] | undefined;
      // Get position/location object, default to {0,0,0} if not provided
      const position = vec3ToObject(argsRecord.position as Vector3 | undefined) ??
                       vec3ToObject(argsTyped.location) ??
                       { x: 0, y: 0, z: 0 };

      return cleanObject(await executeAutomationRequest(tools, 'paint_foliage', {
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || '',
        // C++ expects locations array of objects {x, y, z}
        locations: locations?.map(l => ({ x: l.x ?? 0, y: l.y ?? 0, z: l.z ?? 0 })),
        // C++ expects position/location as object, not array
        position,
        brushSize: (argsRecord.brushSize as number) || argsTyped.radius,
        paintDensity: argsTyped.density || (argsRecord.strength as number),
        eraseMode: argsRecord.eraseMode as boolean | undefined
      }) as Record<string, unknown>);
    }
    case 'create_procedural_terrain': {
      // Generate default name if not provided (C++ requires non-empty name)
      const defaultName = argsTyped.name || argsTyped.actorName || `ProceduralTerrain_${Date.now()}`;
      return cleanObject(await executeAutomationRequest(tools, 'create_procedural_terrain', {
        name: defaultName,
        actorName: defaultName,
        location: vec3ToObject(argsTyped.location),
        sizeX: argsRecord.sizeX as number | undefined,
        sizeY: argsRecord.sizeY as number | undefined,
        heightScale: argsRecord.heightScale as number | undefined,
        subdivisions: argsRecord.subdivisions as number | undefined,
        rotation: argsRecord.rotation as Record<string, unknown> | undefined,
        material: argsRecord.material as string | undefined
      }) as Record<string, unknown>);
    }
    case 'create_procedural_foliage': {
      // Generate default name if not provided (C++ will auto-generate if empty)
      const volumeName = getString(argsRecord.volumeName);
      const defaultName = argsTyped.name || volumeName || `ProceduralFoliage_${Date.now()}`;
      // Accept both 'foliageTypes' and 'types' parameter names
      const foliageTypes = (argsRecord.foliageTypes || argsRecord.types) as { meshPath: string; density: number }[] | undefined;
      return cleanObject(await executeAutomationRequest(tools, 'create_procedural_foliage', {
        name: defaultName,
        foliageTypes: foliageTypes,
        // Pass 'types' as well for C++ handler that accepts both
        types: foliageTypes,
        volumeName: argsRecord.volumeName as string | undefined,
        bounds: normalizeProceduralFoliageBounds(argsRecord.bounds),
        seed: argsTyped.seed,
        tileSize: argsRecord.tileSize as number | undefined
      }) as Record<string, unknown>);
    }

    case 'bake_lightmap':
      return cleanObject(await executeAutomationRequest(tools, 'bake_lightmap', {
        quality: (argsRecord.quality as string) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      }) as Record<string, unknown>);
    case 'create_landscape_grass_type':
      return cleanObject(await executeAutomationRequest(tools, 'create_landscape_grass_type', {
        name: argsTyped.name || '',
        meshPath: argsTyped.meshPath || (argsRecord.staticMesh as string) || '',
        path: argsRecord.path as string | undefined,
        staticMesh: argsRecord.staticMesh as string | undefined
      }) as Record<string, unknown>);
    case 'export_snapshot':
      return cleanObject(await exportEnvironmentSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as Record<string, unknown>;
    case 'import_snapshot':
      return cleanObject(await importEnvironmentSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as Record<string, unknown>;
    case 'set_landscape_material':
      return cleanObject(await executeAutomationRequest(tools, 'set_landscape_material', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        materialPath: argsTyped.materialPath ?? ''
      }) as Record<string, unknown>);
    case 'set_time_of_day': {
      const time = getNumber(argsRecord.time) ?? getNumber(argsRecord.hour) ?? getNumber(argsRecord.propertyValue) ?? 12;
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        action: 'set_time_of_day',
        time,
        hour: argsRecord.hour as number | undefined
      }) as Record<string, unknown>);
    }
    case 'generate_lods':
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        action: 'generate_lods',
        assetPaths: (argsRecord.assetPaths as string[]) || (argsRecord.assets as string[]) || (argsRecord.path ? [argsRecord.path as string] : []),
        numLODs: argsRecord.numLODs as number | undefined
      }, 'Bridge unavailable')) as Record<string, unknown>;
    case 'delete': {
      const names: string[] = Array.isArray(argsRecord.names)
        ? argsRecord.names as string[]
        : (Array.isArray(argsRecord.actors) ? argsRecord.actors as string[] : []);
      if (argsTyped.name) {
        names.push(argsTyped.name);
      }
      const res = await executeAutomationRequest(tools, 'build_environment', {
        action: 'delete',
        names
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
    default: {
      const res = await executeAutomationRequest(tools, 'build_environment', args, 'Automation bridge not available for environment building operations');
      return cleanObject(res) as Record<string, unknown>;
    }
  }
}
