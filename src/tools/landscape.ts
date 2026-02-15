// Landscape tools for Unreal Engine with UE 5.6 World Partition support
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { ensureVector3 } from '../utils/validation.js';
import { wasmIntegration } from '../wasm/index.js';
import { ILandscapeTools, StandardActionResponse } from '../types/tool-interfaces.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('LandscapeTools');

export class LandscapeTools implements ILandscapeTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  // Create landscape with World Partition support (UE 5.6)
  async createLandscape(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    quadsPerSection?: number;
    sectionsPerComponent?: number;
    componentCount?: number;
    materialPath?: string;
    // World Partition specific (UE 5.6)
    enableWorldPartition?: boolean;
    runtimeGrid?: string;
    isSpatiallyLoaded?: boolean;
    dataLayers?: string[];
  }): Promise<StandardActionResponse> {
    const name = params.name?.trim();
    if (!name) {
      return { success: false, error: 'Landscape name is required' };
    }
    if (typeof params.sizeX === 'number' && params.sizeX <= 0) {
      return {
        success: false,
        error: 'Landscape sizeX must be a positive number'
      };
    }
    if (typeof params.sizeY === 'number' && params.sizeY <= 0) {
      return {
        success: false,
        error: 'Landscape sizeY must be a positive number'
      };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape operations require plugin support.');
    }

    const [locX, locY, locZ] = ensureVector3(params.location ?? [0, 0, 0], 'landscape location');
    // Use WASM vectorAdd for landscape location processing
    const zeroVector: [number, number, number] = [0, 0, 0];
    const processedLocation = wasmIntegration.vectorAdd(zeroVector, [locX, locY, locZ]);
    log.debug('[WASM] Using vectorAdd for landscape positioning');
    const sectionsPerComponent = Math.max(1, Math.floor(params.sectionsPerComponent ?? 1));
    const quadsPerSection = Math.max(1, Math.floor(params.quadsPerSection ?? 63));

    try {
      // Map to plugin-native payload shape
      const componentsX = Math.max(1, Math.floor((params.componentCount ?? Math.max(1, Math.floor((params.sizeX ?? 1000) / 1000)))));
      const componentsY = Math.max(1, Math.floor((params.componentCount ?? Math.max(1, Math.floor((params.sizeY ?? 1000) / 1000)))));
      const quadsPerComponent = quadsPerSection; // Plugin uses quadsPerComponent

      const payload: Record<string, unknown> = {
        name,
        x: processedLocation[0],
        y: processedLocation[1],
        z: processedLocation[2],
        componentsX,
        componentsY,
        quadsPerComponent,
        sectionsPerComponent,
        materialPath: params.materialPath || ''
      };

      const response = await this.automationBridge.sendAutomationRequest('create_landscape', payload, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create landscape actor'
        };
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || 'Landscape actor created',
        landscapeName: response.landscapeName || name,
        worldPartition: response.worldPartition ?? params.enableWorldPartition ?? false
      };

      if (response.landscapeActor) {
        result.landscapeActor = response.landscapeActor;
      }
      if (response.warnings) {
        result.warnings = response.warnings;
      }
      if (response.details) {
        result.details = response.details;
      }
      if (params.runtimeGrid) {
        result.runtimeGrid = params.runtimeGrid;
      }
      if (typeof params.isSpatiallyLoaded === 'boolean') {
        result.spatiallyLoaded = params.isSpatiallyLoaded;
      }

      return result as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create landscape actor: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }


  // Sculpt landscape
  async sculptLandscape(params: {
    landscapeName: string;
    tool: string;
    brushSize?: number;
    brushFalloff?: number;
    strength?: number;
    location?: [number, number, number];
    radius?: number;
  }): Promise<StandardActionResponse> {
    const [x, y, z] = ensureVector3(params.location ?? [0, 0, 0], 'sculpt location');

    const tool = (params.tool || '').trim();
    const lowerTool = tool.toLowerCase();
    const validTools = new Set(['sculpt', 'smooth', 'flatten', 'ramp', 'erosion', 'hydro', 'noise', 'raise', 'lower']);
    const isValidTool = lowerTool.length > 0 && validTools.has(lowerTool);

    if (!isValidTool) {
      return {
        success: false,
        error: `Invalid sculpt tool: ${params.tool}`
      };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape operations require plugin support.');
    }

    const payload = {
      landscapeName: params.landscapeName?.trim(),
      toolMode: tool, // Map 'tool' to 'toolMode'
      brushRadius: params.brushSize ?? params.radius ?? 1000,
      brushFalloff: params.brushFalloff ?? 0.5,
      strength: params.strength ?? 0.1,
      location: { x, y, z }
    };

    const response = await this.automationBridge.sendAutomationRequest('sculpt_landscape', payload);

    if (!response.success) {
      return {
        success: false,
        error: response.error || 'Failed to sculpt landscape'
      };
    }

    return {
      success: true,
      message: `Sculpting applied to ${params.landscapeName}`,
      details: response
    } as StandardActionResponse;
  }

  // Paint landscape
  async paintLandscape(params: {
    landscapeName: string;
    layerName: string;
    position: [number, number, number];
    brushSize?: number;
    strength?: number;
    targetValue?: number;
    radius?: number;
    density?: number;
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available.');
    }

    const [x, y] = ensureVector3(params.position, 'paint position');
    const radius = params.brushSize ?? params.radius ?? 1000;

    // Map brush to a square region for now as C++ only supports region fill
    const minX = Math.floor(x - radius);
    const maxX = Math.floor(x + radius);
    const minY = Math.floor(y - radius);
    const maxY = Math.floor(y + radius);

    const payload = {
      landscapeName: params.landscapeName?.trim(),
      layerName: params.layerName?.trim(),
      region: { minX, minY, maxX, maxY },
      strength: params.strength ?? 1.0
    };

    const response = await this.automationBridge.sendAutomationRequest('paint_landscape_layer', payload);

    if (!response.success) {
      return {
        success: false,
        error: response.error || 'Failed to paint landscape layer'
      };
    }

    return {
      success: true,
      message: `Painted layer ${params.layerName}`,
      details: response
    } as StandardActionResponse;
  }

  // Create procedural terrain using ProceduralMeshComponent
  async createProceduralTerrain(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    subdivisions?: number;
    heightFunction?: string; // Expression for height calculation
    material?: string;
    settings?: Record<string, unknown>;
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Procedural terrain creation requires plugin support.');
    }

    try {
      // Combine specific params with generic settings
      const payload = {
        name: params.name,
        location: params.location || [0, 0, 0],
        sizeX: params.sizeX || 2000,
        sizeY: params.sizeY || 2000,
        subdivisions: params.subdivisions || 50,
        heightFunction: params.heightFunction || 'math.sin(x/100) * 50 + math.cos(y/100) * 30',
        material: params.material,
        ...params.settings
      };

      const response = await this.automationBridge.sendAutomationRequest('create_procedural_terrain', payload, {
        timeoutMs: 120000 // 2 minutes for mesh generation
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to create procedural terrain',
          message: response.message || 'Failed to create procedural terrain'
        };
      }

      const result = (response.result ?? {}) as Record<string, unknown>;
      return {
        success: true,
        message: response.message || `Created procedural terrain '${params.name}'`,
        actorName: result.actor_name,
        vertices: result.vertices,
        triangles: result.triangles,
        size: result.size,
        subdivisions: result.subdivisions,
        details: result
      } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create procedural terrain: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Create a LandscapeGrassType asset via AutomationBridge
  async createLandscapeGrassType(params: {
    name: string;
    meshPath: string; // Normalized parameter name (was path/staticMesh/meshPath)
    density?: number;
    minScale?: number;
    maxScale?: number;
    path?: string; // Legacy support
    staticMesh?: string; // Legacy support
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape operations require plugin support.');
    }

    const name = typeof params.name === 'string' ? params.name.trim() : '';
    if (!name) {
      return { success: false, error: 'Grass type name is required' };
    }

    // Accept mesh path from multiple fields for compatibility
    const meshPathRaw = typeof params.meshPath === 'string' && params.meshPath.trim().length > 0
      ? params.meshPath.trim()
      : (typeof params.path === 'string' && params.path.trim().length > 0
        ? params.path.trim()
        : (typeof params.staticMesh === 'string' && params.staticMesh.trim().length > 0
          ? params.staticMesh.trim()
          : ''));

    if (!meshPathRaw) {
      return { success: false, error: 'meshPath is required to create a landscape grass type' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_landscape_grass_type', {
        name,
        meshPath: meshPathRaw,
        density: params.density || 1.0,
        minScale: params.minScale || 0.8,
        maxScale: params.maxScale || 1.2
      }, { timeoutMs: 90000 }) as Record<string, unknown>;

      if (response && response.success === false) {
        return {
          success: false,
          error: (response.error as string) || (response.message as string) || 'Failed to create landscape grass type'
        };
      }

      const result = (response.result ?? {}) as Record<string, unknown>;
      return {
        success: true,
        message: (response.message as string) || `Landscape grass type '${name}' created`,
        assetPath: (result.asset_path as string) || (response.assetPath as string) || (response.asset_path as string)
      } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create landscape grass type: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Set the material used by an existing landscape actor
  async setLandscapeMaterial(params: { landscapeName: string; materialPath: string }): Promise<StandardActionResponse> {
    const landscapeName = typeof params.landscapeName === 'string' ? params.landscapeName.trim() : '';
    const materialPath = typeof params.materialPath === 'string' ? params.materialPath.trim() : '';

    if (!landscapeName) {
      return { success: false, error: 'Landscape name is required' };
    }
    if (!materialPath) {
      return { success: false, error: 'materialPath is required' };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape operations require plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('set_landscape_material', {
        landscapeName,
        materialPath
      }, { timeoutMs: 60000 }) as Record<string, unknown>;

      if (response && response.success === false) {
        return {
          success: false,
          error: (response.error as string) || (response.message as string) || 'Failed to set landscape material'
        };
      }

      return {
        success: true,
        message: (response.message as string) || `Landscape material set on '${landscapeName}'`,
        landscapeName: (response.landscapeName as string) || landscapeName,
        materialPath: (response.materialPath as string) || materialPath
      } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to set landscape material: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Create landscape grass
  async createLandscapeGrass(params: {
    landscapeName: string;
    grassType: string;
    density?: number;
    minScale?: number;
    maxScale?: number;
    randomRotation?: boolean;
  }): Promise<StandardActionResponse> {
    const commands: string[] = [];

    commands.push(`CreateLandscapeGrass ${params.landscapeName} ${params.grassType}`);

    if (params.density !== undefined) {
      commands.push(`SetGrassDensity ${params.grassType} ${params.density}`);
    }

    if (params.minScale !== undefined && params.maxScale !== undefined) {
      commands.push(`SetGrassScale ${params.grassType} ${params.minScale} ${params.maxScale}`);
    }

    if (params.randomRotation !== undefined) {
      commands.push(`SetGrassRandomRotation ${params.grassType} ${params.randomRotation}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: `Grass type ${params.grassType} created on landscape` };
  }

  // Landscape collision
  async updateLandscapeCollision(params: {
    landscapeName: string;
    collisionMipLevel?: number;
    simpleCollision?: boolean;
  }): Promise<StandardActionResponse> {
    const commands: string[] = [];

    if (params.collisionMipLevel !== undefined) {
      commands.push(`SetLandscapeCollisionMipLevel ${params.landscapeName} ${params.collisionMipLevel}`);
    }

    if (params.simpleCollision !== undefined) {
      commands.push(`SetLandscapeSimpleCollision ${params.landscapeName} ${params.simpleCollision}`);
    }

    commands.push(`UpdateLandscapeCollision ${params.landscapeName}`);

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Landscape collision updated' };
  }

  // Retopologize landscape
  async retopologizeLandscape(params: {
    landscapeName: string;
    targetTriangleCount?: number;
    preserveDetails?: boolean;
  }): Promise<StandardActionResponse> {
    const commands: string[] = [];

    if (params.targetTriangleCount !== undefined) {
      commands.push(`SetRetopologizeTarget ${params.targetTriangleCount}`);
    }

    if (params.preserveDetails !== undefined) {
      commands.push(`SetRetopologizePreserveDetails ${params.preserveDetails}`);
    }

    commands.push(`RetopologizeLandscape ${params.landscapeName}`);

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Landscape retopologized' };
  }

  // Create water body
  async createWaterBody(params: {
    type: 'Ocean' | 'Lake' | 'River' | 'Stream';
    name: string;
    location?: [number, number, number];
    size?: [number, number];
    depth?: number;
  }): Promise<StandardActionResponse> {
    const loc = params.location || [0, 0, 0];
    const size = params.size || [1000, 1000];
    const depth = params.depth || 100;

    const command = `CreateWaterBody ${params.type} ${params.name} ${loc.join(' ')} ${size.join(' ')} ${depth}`;

    return this.bridge.executeConsoleCommand(command);
  }

  // World Partition support for landscapes (UE 5.6)
  async configureWorldPartition(params: {
    landscapeName: string;
    enableSpatialLoading?: boolean;
    runtimeGrid?: string;
    dataLayers?: string[];
    streamingDistance?: number;
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. World Partition operations require plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('configure_landscape_world_partition', {
        landscapeName: params.landscapeName,
        enableSpatialLoading: params.enableSpatialLoading,
        runtimeGrid: params.runtimeGrid || '',
        dataLayers: params.dataLayers || [],
        streamingDistance: params.streamingDistance
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'World Partition configuration failed'
        };
      }

      return {
        success: true,
        message: response.message || 'World Partition configured',
        changes: response.changes
      } as StandardActionResponse;
    } catch (err) {
      return { success: false, error: `Failed to configure World Partition: ${err instanceof Error ? err.message : String(err)}` };
    }
  }

  // Set landscape data layers (UE 5.6)
  async setDataLayers(params: {
    landscapeName: string;
    dataLayerNames: string[];
    operation: 'add' | 'remove' | 'set';
  }): Promise<StandardActionResponse> {
    try {
      const commands = [];

      // Use console commands for data layer management
      if (params.operation === 'set' || params.operation === 'add') {
        for (const layerName of params.dataLayerNames) {
          commands.push(`wp.Runtime.SetDataLayerRuntimeState Loaded ${layerName}`);
        }
      } else if (params.operation === 'remove') {
        for (const layerName of params.dataLayerNames) {
          commands.push(`wp.Runtime.SetDataLayerRuntimeState Unloaded ${layerName}`);
        }
      }

      // Execute commands
      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Data layers ${params.operation === 'add' ? 'added' : params.operation === 'remove' ? 'removed' : 'set'} for landscape`,
        layers: params.dataLayerNames
      } as StandardActionResponse;
    } catch (err) {
      return { success: false, error: `Failed to manage data layers: ${err}` };
    }
  }

  // Configure landscape streaming cells (UE 5.6 World Partition)
  async configureStreamingCells(params: {
    landscapeName: string;
    cellSize?: number;
    loadingRange?: number;
    enableHLOD?: boolean;
  }): Promise<StandardActionResponse> {
    const commands = [];

    // World Partition runtime commands
    if (params.loadingRange !== undefined) {
      commands.push(`wp.Runtime.OverrideRuntimeSpatialHashLoadingRange -grid=0 -range=${params.loadingRange}`);
    }

    if (params.enableHLOD !== undefined) {
      commands.push(`wp.Runtime.HLOD ${params.enableHLOD ? '1' : '0'}`);
    }

    // Debug visualization commands
    commands.push('wp.Runtime.ToggleDrawRuntimeHash2D'); // Show 2D grid

    try {
      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: 'Streaming cells configured for World Partition',
        settings: {
          cellSize: params.cellSize,
          loadingRange: params.loadingRange,
          hlod: params.enableHLOD
        }
      } as StandardActionResponse;
    } catch (err) {
      return { success: false, error: `Failed to configure streaming cells: ${err}` };
    }
  }

  // Modify landscape heightmap
  async modifyHeightmap(params: {
    landscapeName: string;
    heightData: number[];
    minX: number;
    minY: number;
    maxX: number;
    maxY: number;
    updateNormals?: boolean;
    timeoutMs?: number;
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Landscape operations require plugin support.');
    }

    const { landscapeName, heightData, minX, minY, maxX, maxY } = params;

    if (!landscapeName) {
      return { success: false, error: 'Landscape name is required' };
    }
    if (!heightData || !Array.isArray(heightData) || heightData.length === 0) {
      return { success: false, error: 'heightData array is required' };
    }

    const width = maxX - minX + 1;
    const height = maxY - minY + 1;
    if (heightData.length !== width * height) {
      return {
        success: false,
        error: `Height data length (${heightData.length}) does not match region dimensions (${width}x${height} = ${width * height})`
      };
    }

    // Use provided timeout or default to 90s for heightmap operations
    // Heightmap modification can be slow due to GPU sync and collision rebuild
    const timeoutMs = params.timeoutMs ?? 90000;

    try {
      const response = await this.automationBridge.sendAutomationRequest('modify_heightmap', {
        landscapeName,
        heightData,
        minX,
        minY,
        maxX,
        maxY,
        updateNormals: params.updateNormals ?? true
      }, {
        timeoutMs
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Failed to modify heightmap'
        };
      }

      return {
        success: true,
        message: response.message || 'Heightmap modified successfully'
      } as StandardActionResponse;
    } catch (err) {
      return { success: false, error: `Failed to modify heightmap: ${err instanceof Error ? err.message : String(err)}` };
    }
  }
}

