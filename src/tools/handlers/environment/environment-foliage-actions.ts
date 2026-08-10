import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { vec3ToObject, type EnvironmentArgs, type LocationItem, type Vector3 } from './environment-handler-utils.js';

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

        // Support location+radius to generate locations if explicit array not provided
        let locations = argsTyped.locations as Vector3[] | undefined;
        if (!locations && argsTyped.location && argsTyped.radius) {
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
      const locations = argsTyped.locations as Vector3[] | undefined;
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
