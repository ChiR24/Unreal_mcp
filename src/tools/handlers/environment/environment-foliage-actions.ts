import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { vec3ToObject, type EnvironmentArgs, type LocationItem, type Vector3 } from './environment-handler-utils.js';

// Everything the plugin's paint brush reads, under the names it reads them by: it
// expands a disc (location + radius) or a box area itself, drops each point onto
// the ground and varies scale/yaw. This path used to send radius/density as
// brushSize/paintDensity, which the plugin never read, and drop the rest.
function paintPayload(argsRecord: Record<string, unknown>, argsTyped: EnvironmentArgs, foliageType: string): Record<string, unknown> {
  const locations = argsTyped.locations as Vector3[] | undefined;
  return {
    foliageType,
    locations: locations?.map((l) => ({ x: l.x ?? 0, y: l.y ?? 0, z: l.z ?? 0 })),
    position: vec3ToObject(argsRecord.position as Vector3 | undefined) ?? vec3ToObject(argsTyped.location),
    radius: argsTyped.radius,
    density: argsTyped.density ?? argsRecord.strength,
    count: argsRecord.count,
    area: argsRecord.area,
    snapToSurface: argsRecord.snapToSurface,
    minScale: argsTyped.minScale,
    maxScale: argsTyped.maxScale,
    randomYaw: argsTyped.randomYaw,
    alignToNormal: argsTyped.alignToNormal
  };
}

export async function handleEnvironmentFoliageAction(
  action: string,
  argsRecord: Record<string, unknown>,
  argsTyped: EnvironmentArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
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
        const foliageType = argsTyped.foliageType || argsTyped.foliageTypePath;
        if (!foliageType) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: 'add_foliage requires either: (1) meshPath to create a new foliage type, or (2) foliageType/foliageTypePath to place instances of an existing type. Example foliage assets: /Game/StarterContent/Props/SM_Bush, /Engine/BasicShapes/Sphere'
          });
        }

        // The disc was generated here with Math.random: unsnapped, unscaled, and
        // with density misread as a count. The plugin's brush does all of it now.
        return cleanObject(await executeAutomationRequest(tools, 'paint_foliage',
          paintPayload(argsRecord, argsTyped, foliageType)) as Record<string, unknown>);
      }
    }

    case 'create_foliage_type': {
      const meshPath = argsTyped.meshPath || (argsRecord.staticMesh as string) || '';
      const defaultName = meshPath ? `${meshPath.split('/').pop()?.split('.')[0]}_Foliage_Type` : undefined;
      const forwarded = { ...argsRecord };
      delete forwarded.action;
      return cleanObject(await executeAutomationRequest(tools, 'add_foliage_type', {
        ...forwarded,
        name: argsTyped.foliageType || argsTyped.name || defaultName || 'NewFoliageType',
        meshPath
      }) as Record<string, unknown>);
    }

    case 'add_foliage_instances': {
      // Bare locations go through as they are, with minScale/maxScale/randomYaw for the
      // plugin to vary each instance; turning them into transforms here pinned every
      // instance at scale 1 facing +X and dropped all three.
      const locationsRaw = argsTyped.locations as LocationItem[] | undefined;
      return cleanObject(await executeAutomationRequest(tools, 'add_foliage_instances', {
        foliageType: argsTyped.foliageType || argsTyped.foliageTypePath || argsTyped.meshPath || '',
        transforms: argsTyped.transforms,
        locations: argsTyped.transforms ? undefined : locationsRaw?.map((l: LocationItem) => ({ x: l.x ?? 0, y: l.y ?? 0, z: l.z ?? 0 })),
        minScale: argsTyped.minScale,
        maxScale: argsTyped.maxScale,
        randomYaw: argsTyped.randomYaw
      }) as Record<string, unknown>);
    }
    case 'paint_foliage':
      return cleanObject(await executeAutomationRequest(tools, 'paint_foliage',
        paintPayload(argsRecord, argsTyped, argsTyped.foliageType || argsTyped.foliageTypePath || '')) as Record<string, unknown>);
    case 'paint_foliage_instances':
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        ...argsRecord,
        action: 'paint_foliage_instances'
      }, 'Automation bridge not available for environment building operations')) as Record<string, unknown>;
    case 'remove_foliage_instances':
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        ...argsRecord,
        action: 'remove_foliage_instances'
      }, 'Automation bridge not available for environment building operations')) as Record<string, unknown>;

    default:
      return undefined;
  }
}
