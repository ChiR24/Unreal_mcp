// Landscape tools for Unreal Engine with UE 5.6 World Partition support
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { ensureVector3 } from '../utils/validation.js';

export class LandscapeTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

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
  }) {
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
    const sectionsPerComponent = Math.max(1, Math.floor(params.sectionsPerComponent ?? 1));
    const quadsPerSection = Math.max(1, Math.floor(params.quadsPerSection ?? 63));
    const _componentCount = Math.max(1, Math.floor(params.componentCount ?? 1));

    const defaultSize = 1000;
    const _scaleX = params.sizeX ? Math.max(0.1, params.sizeX / defaultSize) : 1;
    const _scaleY = params.sizeY ? Math.max(0.1, params.sizeY / defaultSize) : 1;

    try {
      // Map to plugin-native payload shape
      const componentsX = Math.max(1, Math.floor((params.componentCount ?? Math.max(1, Math.floor((params.sizeX ?? 1000) / 1000)))));
      const componentsY = Math.max(1, Math.floor((params.componentCount ?? Math.max(1, Math.floor((params.sizeY ?? 1000) / 1000)))));
      const quadsPerComponent = quadsPerSection; // Plugin uses quadsPerComponent

      const payload: Record<string, unknown> = {
        x: locX,
        y: locY,
        z: locZ,
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

      return result;
    } catch (error) {
      return {
        success: false,
        error: `Failed to create landscape actor: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }


  // Sculpt landscape
  async sculptLandscape(_params: {
    landscapeName: string;
    tool: 'Sculpt' | 'Smooth' | 'Flatten' | 'Ramp' | 'Erosion' | 'Hydro' | 'Noise' | 'Retopologize';
    brushSize?: number;
    brushFalloff?: number;
    strength?: number;
    position?: [number, number, number];
  }) {
    return { success: false, error: 'sculptLandscape not implemented via Remote Control. Requires Landscape editor tools.' };
  }

  // Paint landscape
  async paintLandscape(_params: {
    landscapeName: string;
    layerName: string;
    position: [number, number, number];
    brushSize?: number;
    strength?: number;
    targetValue?: number;
  }) {
    return { success: false, error: 'paintLandscape not implemented via Remote Control. Requires Landscape editor tools.' };
  }

  // Add landscape layer
  async addLandscapeLayer(params: {
    landscapeName: string;
    layerName: string;
    weightMapPath?: string;
    blendMode?: 'Weight' | 'Alpha';
  }) {
  const commands: string[] = [];
    
    commands.push(`AddLandscapeLayer ${params.landscapeName} ${params.layerName}`);
    
    if (params.weightMapPath) {
      commands.push(`SetLayerWeightMap ${params.layerName} ${params.weightMapPath}`);
    }
    
    if (params.blendMode) {
      commands.push(`SetLayerBlendMode ${params.layerName} ${params.blendMode}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Layer ${params.layerName} added to landscape` };
  }

  // Create landscape spline
  async createLandscapeSpline(params: {
    landscapeName: string;
    splineName: string;
    points: Array<[number, number, number]>;
    width?: number;
    falloffWidth?: number;
    meshPath?: string;
  }) {
  const commands: string[] = [];
    
    commands.push(`CreateLandscapeSpline ${params.landscapeName} ${params.splineName}`);
    
    for (const point of params.points) {
      commands.push(`AddSplinePoint ${params.splineName} ${point.join(' ')}`);
    }
    
    if (params.width !== undefined) {
      commands.push(`SetSplineWidth ${params.splineName} ${params.width}`);
    }
    
    if (params.falloffWidth !== undefined) {
      commands.push(`SetSplineFalloffWidth ${params.splineName} ${params.falloffWidth}`);
    }
    
    if (params.meshPath) {
      commands.push(`SetSplineMesh ${params.splineName} ${params.meshPath}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Landscape spline ${params.splineName} created` };
  }

  // Import heightmap
  async importHeightmap(params: {
    landscapeName: string;
    heightmapPath: string;
    scale?: [number, number, number];
  }) {
    const scale = params.scale || [100, 100, 100];
const command = `ImportLandscapeHeightmap ${params.landscapeName} ${params.heightmapPath} ${scale.join(' ')}`;
    const raw = await this.bridge.executeConsoleCommand(command);
    const summary = this.bridge.summarizeConsoleCommand(command, raw);
    const ok = summary.returnValue !== false && !(summary.output && /unknown|invalid/i.test(summary.output));
    return { success: ok, message: summary.output || (ok ? 'Heightmap import executed' : 'Heightmap import failed'), raw };
  }

  // Export heightmap
  async exportHeightmap(params: {
    landscapeName: string;
    exportPath: string;
    format?: 'PNG' | 'RAW';
  }) {
    const format = params.format || 'PNG';
const command = `ExportLandscapeHeightmap ${params.landscapeName} ${params.exportPath} ${format}`;
    const raw = await this.bridge.executeConsoleCommand(command);
    const summary = this.bridge.summarizeConsoleCommand(command, raw);
    const ok = summary.returnValue !== false && !(summary.output && /unknown|invalid/i.test(summary.output));
    return { success: ok, message: summary.output || (ok ? 'Heightmap export executed' : 'Heightmap export failed'), raw };
  }

  // Set landscape LOD
  async setLandscapeLOD(params: {
    landscapeName: string;
    lodBias?: number;
    forcedLOD?: number;
    lodDistribution?: number;
  }) {
  const commands: string[] = [];
    
    if (params.lodBias !== undefined) {
      commands.push(`SetLandscapeLODBias ${params.landscapeName} ${params.lodBias}`);
    }
    
    if (params.forcedLOD !== undefined) {
      commands.push(`SetLandscapeForcedLOD ${params.landscapeName} ${params.forcedLOD}`);
    }
    
    if (params.lodDistribution !== undefined) {
      commands.push(`SetLandscapeLODDistribution ${params.landscapeName} ${params.lodDistribution}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Landscape LOD settings updated' };
  }

  // Create landscape grass
  async createLandscapeGrass(params: {
    landscapeName: string;
    grassType: string;
    density?: number;
    minScale?: number;
    maxScale?: number;
    randomRotation?: boolean;
  }) {
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
  }) {
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
  }) {
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
  }) {
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
  }) {
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
      };
    } catch (err) {
      return { success: false, error: `Failed to configure World Partition: ${err instanceof Error ? err.message : String(err)}` };
    }
  }

  // Set landscape data layers (UE 5.6)
  async setDataLayers(params: {
    landscapeName: string;
    dataLayerNames: string[];
    operation: 'add' | 'remove' | 'set';
  }) {
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
      };
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
  }) {
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
      };
    } catch (err) {
      return { success: false, error: `Failed to configure streaming cells: ${err}` };
    }
  }
}
