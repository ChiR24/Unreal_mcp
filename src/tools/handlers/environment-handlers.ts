import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EnvironmentArgs, Vector3, HandlerResult } from '../../types/handler-types.js';
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

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Normalize path fields to ensure they start with /Game/ and use forward slashes.
 * Returns a copy of the args with normalized paths.
 */
function normalizePathFields(args: Record<string, unknown>): Record<string, unknown> {
  const result = { ...args };
  const pathFields = [
    // Water fields
    'actorPath', 'waterMaterial', 'underwaterPostProcessMaterial',
    'waterInfoMaterial', 'waterMeshPath', 'materialPath',
    'lakeTransitionMaterialPath', 'oceanTransitionMaterialPath', 'waterZonePath',
    // Weather fields
    'niagaraSystemPath', 'particleSystemPath'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      // Replace backslashes with forward slashes
      let normalized = value.split('\\').join('/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Allow /Script/ paths for built-in UE classes
      // Allow plugin paths like /MyPlugin/Assets to pass through unchanged
      if (!normalized.startsWith('/')) {
        normalized = '/Game/' + normalized;
      }
      result[field] = normalized;
    }
  }

  return result;
}

export async function handleEnvironmentTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as EnvironmentArgs;
  const argsRecord = args as Record<string, unknown>;
  const envAction = String(action || '').toLowerCase();
  const timeoutMs = getTimeoutMs();

  // Helper for Water/Weather requests
  const sendAutomationRequest = async (targetTool: string, actionName: string, requestArgs: Record<string, unknown>): Promise<HandlerResult> => {
    const normalizedArgs = normalizePathFields(requestArgs);
    const payload = { ...normalizedArgs, action: actionName };
    const result = await executeAutomationRequest(
      tools,
      targetTool,
      payload as HandlerArgs,
      `Automation bridge not available for ${targetTool} action: ${actionName}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };
  
  switch (envAction) {
    // ========================================================================
    // Water Actions (Phase 54 Consolidation)
    // ========================================================================
    case 'create_water_body_ocean':
    case 'create_water_body_lake':
    case 'create_water_body_river':
    case 'configure_water_body':
    case 'configure_water_waves':
    case 'get_water_body_info':
    case 'list_water_bodies':
    case 'set_river_depth':
    case 'set_ocean_extent':
    case 'set_water_static_mesh':
    case 'set_river_transitions':
    case 'set_water_zone':
    case 'get_water_surface_info':
    case 'get_wave_info':
      return sendAutomationRequest('manage_water', envAction, argsRecord);

    // ========================================================================
    // Weather Actions (Phase 54 Consolidation)
    // ========================================================================
    case 'configure_wind':
    case 'create_weather_system':
    case 'configure_rain_particles':
    case 'configure_snow_particles':
    case 'configure_lightning':
      return sendAutomationRequest('manage_weather', envAction, argsRecord);

    // ========================================================================
    // Landscape & Foliage Actions
    // ========================================================================
    case 'create_landscape':
      return cleanObject(await tools.landscapeTools.createLandscape({
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
      })) as HandlerResult;
    case 'modify_heightmap':
      return cleanObject(await tools.landscapeTools.modifyHeightmap({
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        heightData: argsTyped.heightData ?? [],
        minX: (argsRecord.minX as number) ?? 0,
        minY: (argsRecord.minY as number) ?? 0,
        maxX: (argsRecord.maxX as number) ?? 0,
        maxY: (argsRecord.maxY as number) ?? 0,
        updateNormals: argsRecord.updateNormals as boolean | undefined
      })) as HandlerResult;
    case 'sculpt':
    case 'sculpt_landscape': {
      // Default to 'Raise' tool if not specified
      const tool = (argsRecord.tool as string) || 'Raise';
      return cleanObject(await tools.landscapeTools.sculptLandscape({
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        tool,
        location: vec3ToArray(argsTyped.location),
        radius: argsTyped.radius || 500,
        strength: (argsRecord.strength as number) || 0.5
      })) as HandlerResult;
    }
    case 'add_foliage': {
      // Check if this is adding a foliage TYPE (has meshPath) or INSTANCES (has locations/position)
      if (argsTyped.meshPath) {
        // Derive a better default name from mesh path if not provided
        const defaultName = argsTyped.meshPath.split('/').pop()?.split('.')[0] + '_Foliage_Type';
        return cleanObject(await tools.foliageTools.addFoliageType({
          name: argsTyped.foliageType || argsTyped.name || defaultName || 'NewFoliageType',
          meshPath: argsTyped.meshPath,
          density: argsTyped.density
        })) as HandlerResult;
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

        return cleanObject(await tools.foliageTools.addFoliage({
          foliageType,
          locations
        })) as HandlerResult;
      }
    }

    case 'add_foliage_instances': {
      const locationsRaw = argsTyped.locations as LocationItem[] | undefined;
      const transformsRaw = argsTyped.transforms || 
        (locationsRaw ? locationsRaw.map((l: LocationItem) => ({ location: [l.x ?? 0, l.y ?? 0, l.z ?? 0] as [number, number, number] })) : []);
      return cleanObject(await tools.foliageTools.addFoliageInstances({
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || argsTyped.meshPath || '',
        transforms: transformsRaw as { location: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }[]
      })) as HandlerResult;
    }
    case 'paint_foliage':
      return cleanObject(await tools.foliageTools.paintFoliage({
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || '',
        position: vec3ToArray(argsRecord.position as Vector3 | undefined) ?? vec3ToArray(argsTyped.location) ?? [0, 0, 0],
        brushSize: (argsRecord.brushSize as number) || argsTyped.radius,
        paintDensity: argsTyped.density || (argsRecord.strength as number),
        eraseMode: argsRecord.eraseMode as boolean | undefined
      })) as HandlerResult;
    case 'create_procedural_terrain':
      return cleanObject(await tools.landscapeTools.createProceduralTerrain({
        name: argsTyped.name || '',
        location: vec3ToArray(argsTyped.location),
        subdivisions: argsRecord.subdivisions as number | undefined,
        settings: argsRecord.settings as Record<string, unknown> | undefined
      })) as HandlerResult;
    case 'create_procedural_foliage':
      return cleanObject(await tools.foliageTools.createProceduralFoliage({
        name: argsTyped.name || '',
        foliageTypes: argsRecord.foliageTypes as { meshPath: string; density: number }[] | undefined,
        volumeName: argsRecord.volumeName as string | undefined,
        bounds: argsTyped.bounds ? { location: argsTyped.bounds.min, size: argsTyped.bounds.max } : undefined,
        seed: argsTyped.seed,
        tileSize: argsRecord.tileSize as number | undefined
      })) as HandlerResult;

    case 'bake_lightmap':
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: (argsRecord.quality as string) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      })) as HandlerResult;
    case 'create_landscape_grass_type':
      return cleanObject(await tools.landscapeTools.createLandscapeGrassType({
        name: argsTyped.name || '',
        meshPath: argsTyped.meshPath || (argsRecord.path as string) || (argsRecord.staticMesh as string),
        path: argsRecord.path as string | undefined,
        staticMesh: argsRecord.staticMesh as string | undefined
      })) as HandlerResult;
    case 'export_snapshot':
      return cleanObject(await tools.environmentTools.exportSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as HandlerResult;
    case 'import_snapshot':
      return cleanObject(await tools.environmentTools.importSnapshot({
        path: argsRecord.path as string | undefined,
        filename: argsRecord.filename as string | undefined
      })) as HandlerResult;
    case 'set_landscape_material':
      return cleanObject(await tools.landscapeTools.setLandscapeMaterial({
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        materialPath: argsTyped.materialPath ?? ''
      })) as HandlerResult;
    case 'generate_lods':
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        action: 'generate_lods',
        assetPaths: (argsRecord.assetPaths as string[]) || (argsRecord.assets as string[]) || (argsRecord.path ? [argsRecord.path as string] : []),
        numLODs: argsRecord.numLODs as number | undefined
      }, 'Bridge unavailable')) as HandlerResult;
    case 'delete': {
      const names: string[] = Array.isArray(argsRecord.names)
        ? argsRecord.names as string[]
        : (Array.isArray(argsRecord.actors) ? argsRecord.actors as string[] : []);
      if (argsTyped.name) {
        names.push(argsTyped.name);
      }
      const res = await tools.environmentTools.cleanup({ names });
      return cleanObject(res) as HandlerResult;
    }
    default: {
      const res = await executeAutomationRequest(tools, 'build_environment', args, 'Automation bridge not available for environment building operations');
      return cleanObject(res) as HandlerResult;
    }
  }
}
