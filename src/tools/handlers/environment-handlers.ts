import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleEnvironmentTools(action: string, args: any, tools: ITools) {
  const envAction = String(action || '').toLowerCase();
  switch (envAction) {
    case 'create_landscape':
      return cleanObject(await tools.landscapeTools.createLandscape({
        name: args.name,
        location: args.location,
        sizeX: args.sizeX,
        sizeY: args.sizeY,
        quadsPerSection: args.quadsPerSection,
        sectionsPerComponent: args.sectionsPerComponent,
        componentCount: args.componentCount,
        materialPath: args.materialPath,
        enableWorldPartition: args.enableWorldPartition,
        runtimeGrid: args.runtimeGrid,
        isSpatiallyLoaded: args.isSpatiallyLoaded,
        dataLayers: args.dataLayers
      }));
    case 'sculpt':
      return cleanObject(await tools.landscapeTools.sculptLandscape({
        landscapeName: args.landscapeName || args.name,
        tool: args.tool,
        location: args.location,
        radius: args.radius,
        strength: args.strength
      }));
    case 'add_foliage': {
      // Check if this is adding a foliage TYPE (has meshPath) or INSTANCES (has locations/position)
      if (args.meshPath) {
        return cleanObject(await tools.foliageTools.addFoliageType({
          name: args.foliageType || args.name || 'TC_Tree',
          meshPath: args.meshPath,
          density: args.density
        }));
      } else {
        // Validate foliageType is provided
        const foliageType = args.foliageType || args.foliageTypePath;
        if (!foliageType) {
          return cleanObject({
            success: false,
            error: 'INVALID_ARGUMENT',
            message: 'add_foliage requires either: (1) meshPath to create a new foliage type, or (2) foliageType/foliageTypePath to place instances of an existing type. Example foliage assets: /Game/StarterContent/Props/SM_Bush, /Engine/BasicShapes/Sphere'
          });
        }

        // Support location+radius to generate locations if explicit array not provided
        let locations = args.locations;
        if (!locations && args.location && args.radius) {
          // Generate locations around the center point within radius
          const center = args.location;
          const radius = args.radius || 500;
          const count = args.density || args.count || 10;
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
        } else if (!locations && args.position) {
          locations = [args.position];
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
        }));
      }
    }
    case 'paint_foliage':
      return cleanObject(await tools.foliageTools.paintFoliage({
        foliageType: args.foliageType || args.foliageTypePath,
        position: args.position || args.location, // Handle both
        brushSize: args.brushSize || args.radius,
        paintDensity: args.density || args.strength, // Map strength/density
        eraseMode: args.eraseMode
      }));
    case 'create_procedural_terrain':
      return cleanObject(await tools.landscapeTools.createProceduralTerrain({
        name: args.name,
        location: args.location,
        subdivisions: args.subdivisions,
        settings: args.settings
      }));
    case 'create_procedural_foliage':
      return cleanObject(await tools.foliageTools.createProceduralFoliage({
        name: args.name,
        foliageTypes: args.foliageTypes,
        volumeName: args.volumeName,
        bounds: args.bounds,
        seed: args.seed,
        tileSize: args.tileSize
      }));

    case 'bake_lightmap':
      return cleanObject(await tools.lightingTools.buildLighting({
        quality: (args.quality as any) || 'Preview',
        buildOnlySelected: false,
        buildReflectionCaptures: false
      }));
    case 'create_landscape_grass_type':
      return cleanObject(await tools.landscapeTools.createLandscapeGrassType({
        name: args.name,
        // Prefer explicit meshPath used by tests, fall back to path/staticMesh for
        // compatibility with older callers.
        meshPath: args.meshPath || args.path || args.staticMesh,
        path: args.path,
        staticMesh: args.staticMesh
      }));
    case 'export_snapshot':
      return cleanObject(await tools.environmentTools.exportSnapshot({
        path: args.path,
        filename: args.filename
      }));
    case 'import_snapshot':
      return cleanObject(await tools.environmentTools.importSnapshot({
        path: args.path,
        filename: args.filename
      }));
    case 'set_landscape_material':
      return cleanObject(await tools.landscapeTools.setLandscapeMaterial({
        landscapeName: args.landscapeName || args.name,
        materialPath: args.materialPath
      }));
    case 'generate_lods':
      return cleanObject(await executeAutomationRequest(tools, 'build_environment', {
        action: 'generate_lods',
        assetPaths: args.assetPaths || args.assets || (args.path ? [args.path] : []),
        numLODs: args.numLODs
      }, 'Bridge unavailable'));
    case 'delete': {
      const names = Array.isArray(args.names)
        ? args.names
        : (Array.isArray(args.actors) ? args.actors : []);
      const res = await tools.environmentTools.cleanup({ names });
      return cleanObject(res);
    }
    default:
      const res = await executeAutomationRequest(tools, 'build_environment', args, 'Automation bridge not available for environment building operations');
      return cleanObject(res);
  }
}
