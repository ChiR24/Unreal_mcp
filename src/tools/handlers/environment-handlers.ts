import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EnvironmentArgs, Vector3 } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Location item in foliage locations array */
interface LocationItem {
  x?: number;
  y?: number;
  z?: number;
}

/** Convert Vector3 to array format expected by some tools */
function vec3ToArray(v: Vector3 | undefined): [number, number, number] | undefined {
  if (!v) return undefined;
  return [v.x ?? 0, v.y ?? 0, v.z ?? 0];
}

export async function handleEnvironmentTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = args as EnvironmentArgs;
  const argsRecord = args as Record<string, unknown>;
  const envAction = String(action || '').toLowerCase();
  
  switch (envAction) {
    case 'create_landscape':
      return cleanObject(await executeAutomationRequest(tools, 'create_landscape', {
        name: argsTyped.name ?? '',
        location: vec3ToArray(argsTyped.location),
        sizeX: argsRecord.sizeX as number | undefined,
        sizeY: argsRecord.sizeY as number | undefined,
        quadsPerSection: argsRecord.quadsPerSection as number | undefined,
        sectionsPerComponent: argsTyped.sectionsPerComponent,
        componentCount: typeof argsTyped.componentCount === 'object' ? argsTyped.componentCount.x : undefined,
        materialPath: argsTyped.materialPath,
        enableWorldPartition: argsRecord.enableWorldPartition as boolean | undefined,
        runtimeGrid: argsRecord.runtimeGrid as string | undefined,
        isSpatiallyLoaded: argsRecord.isSpatiallyLoaded as boolean | undefined,
        dataLayers: argsRecord.dataLayers as string[] | undefined
      }) as Record<string, unknown>);
    case 'modify_heightmap':
      return cleanObject(await executeAutomationRequest(tools, 'modify_heightmap', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        heightData: argsTyped.heightData ?? [],
        minX: (argsRecord.minX as number) ?? 0,
        minY: (argsRecord.minY as number) ?? 0,
        maxX: (argsRecord.maxX as number) ?? 0,
        maxY: (argsRecord.maxY as number) ?? 0,
        updateNormals: argsRecord.updateNormals as boolean | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      }) as Record<string, unknown>);
    case 'sculpt':
    case 'sculpt_landscape': {
      // Default to 'Raise' tool if not specified
      const tool = (argsRecord.tool as string) || 'Raise';
      return cleanObject(await executeAutomationRequest(tools, 'sculpt_landscape', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        tool,
        location: vec3ToArray(argsTyped.location),
        radius: argsTyped.radius || 500,
        strength: (argsRecord.strength as number) || 0.5
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
          density: argsTyped.density
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
      const transformsRaw = argsTyped.transforms || 
        (locationsRaw ? locationsRaw.map((l: LocationItem) => ({ location: [l.x ?? 0, l.y ?? 0, l.z ?? 0] as [number, number, number] })) : []);
      return cleanObject(await executeAutomationRequest(tools, 'add_foliage_instances', {
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || argsTyped.meshPath || '',
        transforms: transformsRaw as { location: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }[]
      }) as Record<string, unknown>);
    }
    case 'paint_foliage':
      return cleanObject(await executeAutomationRequest(tools, 'paint_foliage', {
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || '',
        position: vec3ToArray(argsRecord.position as Vector3 | undefined) ?? vec3ToArray(argsTyped.location) ?? [0, 0, 0],
        brushSize: (argsRecord.brushSize as number) || argsTyped.radius,
        paintDensity: argsTyped.density || (argsRecord.strength as number),
        eraseMode: argsRecord.eraseMode as boolean | undefined
      }) as Record<string, unknown>);
    case 'create_procedural_terrain':
      return cleanObject(await executeAutomationRequest(tools, 'create_procedural_terrain', {
        name: argsTyped.name || '',
        location: vec3ToArray(argsTyped.location),
        subdivisions: argsRecord.subdivisions as number | undefined,
        settings: argsRecord.settings as Record<string, unknown> | undefined
      }) as Record<string, unknown>);
    case 'create_procedural_foliage':
      return cleanObject(await executeAutomationRequest(tools, 'create_procedural_foliage', {
        name: argsTyped.name || '',
        foliageTypes: argsRecord.foliageTypes as { meshPath: string; density: number }[] | undefined,
        volumeName: argsRecord.volumeName as string | undefined,
        bounds: argsTyped.bounds ? { location: argsTyped.bounds.min, size: argsTyped.bounds.max } : undefined,
        seed: argsTyped.seed,
        tileSize: argsRecord.tileSize as number | undefined
      }) as Record<string, unknown>);

    case 'bake_lightmap':
      return cleanObject(await executeAutomationRequest(tools, 'bake_lightmap', {
        quality: (argsRecord.quality as string) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      }) as Record<string, unknown>);
    case 'create_landscape_grass_type':
      return cleanObject(await executeAutomationRequest(tools, 'create_landscape_grass_type', {
        name: argsTyped.name || '',
        meshPath: argsTyped.meshPath || (argsRecord.path as string) || (argsRecord.staticMesh as string),
        path: argsRecord.path as string | undefined,
        staticMesh: argsRecord.staticMesh as string | undefined
      }) as Record<string, unknown>);
    case 'export_snapshot':
      return cleanObject(await tools.environmentTools.exportSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as Record<string, unknown>;
    case 'import_snapshot':
      return cleanObject(await tools.environmentTools.importSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as Record<string, unknown>;
    case 'set_landscape_material':
      return cleanObject(await executeAutomationRequest(tools, 'set_landscape_material', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        materialPath: argsTyped.materialPath ?? ''
      }) as Record<string, unknown>);
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
