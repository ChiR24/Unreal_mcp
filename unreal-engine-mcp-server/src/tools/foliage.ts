// Foliage tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';

export class FoliageTools {
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

  // Add foliage type
  async addFoliageType(params: {
    name: string;
    meshPath: string;
    density?: number;
    radius?: number;
    minScale?: number;
    maxScale?: number;
    alignToNormal?: boolean;
    randomYaw?: boolean;
    groundSlope?: number;
  }) {
    const commands = [];
    
    commands.push(`AddFoliageType ${params.name} ${params.meshPath}`);
    
    if (params.density !== undefined) {
      commands.push(`SetFoliageDensity ${params.name} ${params.density}`);
    }
    
    if (params.radius !== undefined) {
      commands.push(`SetFoliageRadius ${params.name} ${params.radius}`);
    }
    
    if (params.minScale !== undefined && params.maxScale !== undefined) {
      commands.push(`SetFoliageScale ${params.name} ${params.minScale} ${params.maxScale}`);
    }
    
    if (params.alignToNormal !== undefined) {
      commands.push(`SetFoliageAlignToNormal ${params.name} ${params.alignToNormal}`);
    }
    
    if (params.randomYaw !== undefined) {
      commands.push(`SetFoliageRandomYaw ${params.name} ${params.randomYaw}`);
    }
    
    if (params.groundSlope !== undefined) {
      commands.push(`SetFoliageGroundSlope ${params.name} ${params.groundSlope}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage type ${params.name} added` };
  }

  // Paint foliage
  async paintFoliage(params: {
    foliageType: string;
    position: [number, number, number];
    brushSize?: number;
    paintDensity?: number;
    eraseMode?: boolean;
  }) {
    const commands = [];
    
    commands.push(`SelectFoliageType ${params.foliageType}`);
    
    if (params.brushSize !== undefined) {
      commands.push(`SetFoliageBrushSize ${params.brushSize}`);
    }
    
    if (params.paintDensity !== undefined) {
      commands.push(`SetFoliagePaintDensity ${params.paintDensity}`);
    }
    
    if (params.eraseMode !== undefined) {
      commands.push(`SetFoliageEraseMode ${params.eraseMode}`);
    }
    
    commands.push(`PaintFoliage ${params.position.join(' ')}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage painted at position` };
  }

  // Create instanced mesh
  async createInstancedMesh(params: {
    name: string;
    meshPath: string;
    instances: Array<{
      position: [number, number, number];
      rotation?: [number, number, number];
      scale?: [number, number, number];
    }>;
    enableCulling?: boolean;
    cullDistance?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateInstancedStaticMesh ${params.name} ${params.meshPath}`);
    
    for (const instance of params.instances) {
      const rot = instance.rotation || [0, 0, 0];
      const scale = instance.scale || [1, 1, 1];
      commands.push(`AddInstance ${params.name} ${instance.position.join(' ')} ${rot.join(' ')} ${scale.join(' ')}`);
    }
    
    if (params.enableCulling !== undefined) {
      commands.push(`SetInstanceCulling ${params.name} ${params.enableCulling}`);
    }
    
    if (params.cullDistance !== undefined) {
      commands.push(`SetInstanceCullDistance ${params.name} ${params.cullDistance}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Instanced mesh ${params.name} created with ${params.instances.length} instances` };
  }

  // Set foliage LOD
  async setFoliageLOD(params: {
    foliageType: string;
    lodDistances?: number[];
    screenSize?: number[];
  }) {
    const commands = [];
    
    if (params.lodDistances) {
      commands.push(`SetFoliageLODDistances ${params.foliageType} ${params.lodDistances.join(' ')}`);
    }
    
    if (params.screenSize) {
      commands.push(`SetFoliageLODScreenSize ${params.foliageType} ${params.screenSize.join(' ')}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage LOD settings updated` };
  }

  // Create procedural foliage
  async createProceduralFoliage(params: {
    volumeName: string;
    position: [number, number, number];
    size: [number, number, number];
    foliageTypes: string[];
    seed?: number;
    tileSize?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateProceduralFoliageVolume ${params.volumeName} ${params.position.join(' ')} ${params.size.join(' ')}`);
    
    for (const type of params.foliageTypes) {
      commands.push(`AddProceduralFoliageType ${params.volumeName} ${type}`);
    }
    
    if (params.seed !== undefined) {
      commands.push(`SetProceduralSeed ${params.volumeName} ${params.seed}`);
    }
    
    if (params.tileSize !== undefined) {
      commands.push(`SetProceduralTileSize ${params.volumeName} ${params.tileSize}`);
    }
    
    commands.push(`GenerateProceduralFoliage ${params.volumeName}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Procedural foliage volume ${params.volumeName} created` };
  }

  // Set foliage collision
  async setFoliageCollision(params: {
    foliageType: string;
    collisionEnabled?: boolean;
    collisionProfile?: string;
    generateOverlapEvents?: boolean;
  }) {
    const commands = [];
    
    if (params.collisionEnabled !== undefined) {
      commands.push(`SetFoliageCollision ${params.foliageType} ${params.collisionEnabled}`);
    }
    
    if (params.collisionProfile) {
      commands.push(`SetFoliageCollisionProfile ${params.foliageType} ${params.collisionProfile}`);
    }
    
    if (params.generateOverlapEvents !== undefined) {
      commands.push(`SetFoliageOverlapEvents ${params.foliageType} ${params.generateOverlapEvents}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage collision settings updated` };
  }

  // Create grass system
  async createGrassSystem(params: {
    name: string;
    grassTypes: Array<{
      meshPath: string;
      density: number;
      minScale?: number;
      maxScale?: number;
    }>;
    windStrength?: number;
    windSpeed?: number;
  }) {
    const commands = [];
    
    commands.push(`CreateGrassSystem ${params.name}`);
    
    for (const grassType of params.grassTypes) {
      const minScale = grassType.minScale || 0.8;
      const maxScale = grassType.maxScale || 1.2;
      commands.push(`AddGrassType ${params.name} ${grassType.meshPath} ${grassType.density} ${minScale} ${maxScale}`);
    }
    
    if (params.windStrength !== undefined) {
      commands.push(`SetGrassWindStrength ${params.name} ${params.windStrength}`);
    }
    
    if (params.windSpeed !== undefined) {
      commands.push(`SetGrassWindSpeed ${params.name} ${params.windSpeed}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Grass system ${params.name} created` };
  }

  // Remove foliage instances
  async removeFoliageInstances(params: {
    foliageType: string;
    position: [number, number, number];
    radius: number;
  }) {
    const command = `RemoveFoliageInRadius ${params.foliageType} ${params.position.join(' ')} ${params.radius}`;
    return this.executeCommand(command);
  }

  // Select foliage instances
  async selectFoliageInstances(params: {
    foliageType: string;
    position?: [number, number, number];
    radius?: number;
    selectAll?: boolean;
  }) {
    let command: string;
    
    if (params.selectAll) {
      command = `SelectAllFoliage ${params.foliageType}`;
    } else if (params.position && params.radius) {
      command = `SelectFoliageInRadius ${params.foliageType} ${params.position.join(' ')} ${params.radius}`;
    } else {
      command = `SelectFoliageType ${params.foliageType}`;
    }
    
    return this.executeCommand(command);
  }

  // Update foliage instances
  async updateFoliageInstances(params: {
    foliageType: string;
    updateTransforms?: boolean;
    updateMesh?: boolean;
    newMeshPath?: string;
  }) {
    const commands = [];
    
    if (params.updateTransforms) {
      commands.push(`UpdateFoliageTransforms ${params.foliageType}`);
    }
    
    if (params.updateMesh && params.newMeshPath) {
      commands.push(`UpdateFoliageMesh ${params.foliageType} ${params.newMeshPath}`);
    }
    
    commands.push(`RefreshFoliage ${params.foliageType}`);
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage instances updated` };
  }

  // Create foliage spawner
  async createFoliageSpawner(params: {
    name: string;
    spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All';
    excludeAreas?: Array<[number, number, number, number]>; // [x, y, z, radius]
  }) {
    const commands = [];
    
    commands.push(`CreateFoliageSpawner ${params.name} ${params.spawnArea}`);
    
    if (params.excludeAreas) {
      for (const area of params.excludeAreas) {
        commands.push(`AddFoliageExclusionArea ${params.name} ${area.join(' ')}`);
      }
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Foliage spawner ${params.name} created` };
  }

  // Optimize foliage
  async optimizeFoliage(params: {
    mergeInstances?: boolean;
    generateClusters?: boolean;
    clusterSize?: number;
    reduceDrawCalls?: boolean;
  }) {
    const commands = [];
    
    if (params.mergeInstances) {
      commands.push('MergeFoliageInstances');
    }
    
    if (params.generateClusters) {
      const size = params.clusterSize || 100;
      commands.push(`GenerateFoliageClusters ${size}`);
    }
    
    if (params.reduceDrawCalls) {
      commands.push('OptimizeFoliageDrawCalls');
    }
    
    commands.push('RebuildFoliageTree');
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: 'Foliage optimized' };
  }
}
