// Landscape tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge';

export class LandscapeTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Create landscape
  async createLandscape(params: {
    name: string;
    location?: [number, number, number];
    sizeX?: number;
    sizeY?: number;
    quadsPerSection?: number;
    sectionsPerComponent?: number;
    componentCount?: number;
    materialPath?: string;
  }) {
    const commands = [];
    const loc = params.location || [0, 0, 0];
    const sizeX = params.sizeX || 127;
    const sizeY = params.sizeY || 127;
    
    commands.push(`CreateLandscape ${params.name} ${loc.join(' ')} ${sizeX} ${sizeY}`);
    
    if (params.quadsPerSection) {
      commands.push(`SetLandscapeQuadsPerSection ${params.name} ${params.quadsPerSection}`);
    }
    
    if (params.sectionsPerComponent) {
      commands.push(`SetLandscapeSectionsPerComponent ${params.name} ${params.sectionsPerComponent}`);
    }
    
    if (params.componentCount) {
      commands.push(`SetLandscapeComponentCount ${params.name} ${params.componentCount}`);
    }
    
    if (params.materialPath) {
      commands.push(`SetLandscapeMaterial ${params.name} ${params.materialPath}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Landscape ${params.name} created` };
  }

  // Sculpt landscape
  async sculptLandscape(params: {
    landscapeName: string;
    tool: 'Sculpt' | 'Smooth' | 'Flatten' | 'Ramp' | 'Erosion' | 'Hydro' | 'Noise' | 'Retopologize';
    brushSize?: number;
    brushFalloff?: number;
    strength?: number;
    position?: [number, number, number];
  }) {
    const commands = [];
    
    commands.push(`SelectLandscapeTool ${params.tool}`);
    
    if (params.brushSize !== undefined) {
      commands.push(`SetLandscapeBrushSize ${params.brushSize}`);
    }
    
    if (params.brushFalloff !== undefined) {
      commands.push(`SetLandscapeBrushFalloff ${params.brushFalloff}`);
    }
    
    if (params.strength !== undefined) {
      commands.push(`SetLandscapeToolStrength ${params.strength}`);
    }
    
    if (params.position) {
      commands.push(`ApplyLandscapeTool ${params.landscapeName} ${params.position.join(' ')}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Landscape sculpting applied` };
  }

  // Paint landscape
  async paintLandscape(params: {
    landscapeName: string;
    layerName: string;
    position: [number, number, number];
    brushSize?: number;
    strength?: number;
    targetValue?: number;
  }) {
    const commands = [];
    
    commands.push(`SelectLandscapeLayer ${params.layerName}`);
    
    if (params.brushSize !== undefined) {
      commands.push(`SetLandscapeBrushSize ${params.brushSize}`);
    }
    
    if (params.strength !== undefined) {
      commands.push(`SetPaintStrength ${params.strength}`);
    }
    
    if (params.targetValue !== undefined) {
      commands.push(`SetPaintTargetValue ${params.targetValue}`);
    }
    
    commands.push(`PaintLandscape ${params.landscapeName} ${params.position.join(' ')}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Landscape painted with ${params.layerName}` };
  }

  // Add landscape layer
  async addLandscapeLayer(params: {
    landscapeName: string;
    layerName: string;
    weightMapPath?: string;
    blendMode?: 'Weight' | 'Alpha';
  }) {
    const commands = [];
    
    commands.push(`AddLandscapeLayer ${params.landscapeName} ${params.layerName}`);
    
    if (params.weightMapPath) {
      commands.push(`SetLayerWeightMap ${params.layerName} ${params.weightMapPath}`);
    }
    
    if (params.blendMode) {
      commands.push(`SetLayerBlendMode ${params.layerName} ${params.blendMode}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
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
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
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
    
    return this.executeCommand(command);
  }

  // Export heightmap
  async exportHeightmap(params: {
    landscapeName: string;
    exportPath: string;
    format?: 'PNG' | 'RAW';
  }) {
    const format = params.format || 'PNG';
    const command = `ExportLandscapeHeightmap ${params.landscapeName} ${params.exportPath} ${format}`;
    
    return this.executeCommand(command);
  }

  // Set landscape LOD
  async setLandscapeLOD(params: {
    landscapeName: string;
    lodBias?: number;
    forcedLOD?: number;
    lodDistribution?: number;
  }) {
    const commands = [];
    
    if (params.lodBias !== undefined) {
      commands.push(`SetLandscapeLODBias ${params.landscapeName} ${params.lodBias}`);
    }
    
    if (params.forcedLOD !== undefined) {
      commands.push(`SetLandscapeForcedLOD ${params.landscapeName} ${params.forcedLOD}`);
    }
    
    if (params.lodDistribution !== undefined) {
      commands.push(`SetLandscapeLODDistribution ${params.landscapeName} ${params.lodDistribution}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
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
    const commands = [];
    
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
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Grass type ${params.grassType} created on landscape` };
  }

  // Landscape collision
  async updateLandscapeCollision(params: {
    landscapeName: string;
    collisionMipLevel?: number;
    simpleCollision?: boolean;
  }) {
    const commands = [];
    
    if (params.collisionMipLevel !== undefined) {
      commands.push(`SetLandscapeCollisionMipLevel ${params.landscapeName} ${params.collisionMipLevel}`);
    }
    
    if (params.simpleCollision !== undefined) {
      commands.push(`SetLandscapeSimpleCollision ${params.landscapeName} ${params.simpleCollision}`);
    }
    
    commands.push(`UpdateLandscapeCollision ${params.landscapeName}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Landscape collision updated' };
  }

  // Retopologize landscape
  async retopologizeLandscape(params: {
    landscapeName: string;
    targetTriangleCount?: number;
    preserveDetails?: boolean;
  }) {
    const commands = [];
    
    if (params.targetTriangleCount !== undefined) {
      commands.push(`SetRetopologizeTarget ${params.targetTriangleCount}`);
    }
    
    if (params.preserveDetails !== undefined) {
      commands.push(`SetRetopologizePreserveDetails ${params.preserveDetails}`);
    }
    
    commands.push(`RetopologizeLandscape ${params.landscapeName}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
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
    
    return this.executeCommand(command);
  }
}
