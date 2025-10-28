import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';

/**
 * Advanced Build Environment Tools
 * Implements procedural terrain and foliage using AutomationBridge
 */
export class BuildEnvironmentAdvanced {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  /**
   * Create procedural terrain using ProceduralMeshComponent
   * This works around the landscape API limitations
   */
  async createProceduralTerrain(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    subdivisions?: number;
    heightFunction?: string; // Expression for height calculation
    material?: string;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Procedural terrain creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_procedural_terrain', {
        name: params.name,
        location: params.location || [0, 0, 0],
        sizeX: params.sizeX || 2000,
        sizeY: params.sizeY || 2000,
        subdivisions: params.subdivisions || 50,
        heightFunction: params.heightFunction || 'math.sin(x/100) * 50 + math.cos(y/100) * 30',
        material: params.material
      }, {
        timeoutMs: 120000 // 2 minutes for mesh generation
      });
      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create procedural terrain',
          message: response.message || 'Failed to create procedural terrain'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Created procedural terrain '${params.name}'`,
        actor_name: result?.actor_name,
        actorName: result?.actor_name,
        vertices: result?.vertices,
        triangles: result?.triangles,
        size: result?.size,
        subdivisions: result?.subdivisions
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create procedural terrain: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  /**
   * Create procedural foliage using ProceduralFoliageSpawner
   * Uses the documented Unreal Engine API
   */
  async createProceduralFoliage(params: {
    name: string;
    bounds: { location: [number, number, number]; size: [number, number, number] };
    foliageTypes: Array<{
      meshPath: string;
      density: number;
      minScale?: number;
      maxScale?: number;
      alignToNormal?: boolean;
      randomYaw?: boolean;
    }>;
    seed?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Procedural foliage creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_procedural_foliage', {
        name: params.name,
        bounds: params.bounds,
        foliageTypes: params.foliageTypes,
        seed: params.seed || 12345
      }, {
        timeoutMs: 180000 // 3 minutes for complex foliage setup
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create procedural foliage',
          message: response.message || 'Failed to create procedural foliage'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Created procedural foliage volume '${params.name}'`,
        volume_actor: result?.volume_actor,
        volumeActor: result?.volume_actor,
        spawner_path: result?.spawner_path,
        spawnerPath: result?.spawner_path,
        foliage_types_count: result?.foliage_types_count,
        foliageTypesCount: result?.foliage_types_count,
        resimulated: result?.resimulated,
        note: result?.note
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create procedural foliage: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  /**
   * Add foliage instances using InstancedFoliageActor
   * Direct instance placement approach
   */
  async addFoliageInstances(params: {
    foliageType: string; // Path to FoliageType or mesh
    transforms: Array<{
      location: [number, number, number];
      rotation?: [number, number, number];
      scale?: [number, number, number];
    }>;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Foliage instance placement requires plugin support.');
    }

    try {
      const typePath = params.foliageType.includes('/') ? params.foliageType : `/Game/Foliage/${params.foliageType}.${params.foliageType}`;
      const response = await this.automationBridge.sendAutomationRequest('add_foliage_instances', {
        foliageType: typePath,
        transforms: params.transforms
      }, {
        timeoutMs: 120000 // 2 minutes for instance placement
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to add foliage instances',
          message: response.message || 'Failed to add foliage instances'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Added ${result?.instances_count || params.transforms.length} foliage instances`,
        instances_count: result?.instances_count,
        instancesCount: result?.instances_count
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to add foliage instances: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  /**
   * Create landscape grass type for automatic foliage on landscape
   */
  async createLandscapeGrassType(params: {
    name: string;
    meshPath: string;
    density?: number;
    minScale?: number;
    maxScale?: number;
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape grass type creation requires plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_landscape_grass_type', {
        name: params.name,
        meshPath: params.meshPath,
        density: params.density || 1.0,
        minScale: params.minScale || 0.8,
        maxScale: params.maxScale || 1.2
      }, {
        timeoutMs: 90000 // 90 seconds for asset creation
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create landscape grass type',
          message: response.message || 'Failed to create landscape grass type'
        };
      }

      const result = response.result as any;
      return {
        success: true,
        message: response.message || `Created landscape grass type '${params.name}'`,
        asset_path: result?.asset_path,
        assetPath: result?.asset_path
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to create landscape grass type: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }
}