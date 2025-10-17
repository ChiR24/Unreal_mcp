// Foliage tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { coerceBoolean, coerceNumber, coerceString } from '../utils/result-helpers.js';

export class FoliageTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  // NOTE: We intentionally avoid issuing Unreal console commands here because
  // they have proven unreliable and generate engine warnings (failed FindConsoleObject).
  // Instead, we validate inputs and return structured results. Actual foliage
  // authoring should be implemented via Python APIs in future iterations.

  // Add foliage type via Python (creates FoliageType asset properly)
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
    // Basic validation to prevent bad inputs like 'undefined' and empty strings
    const errors: string[] = [];
    const name = String(params?.name ?? '').trim();
    const meshPath = String(params?.meshPath ?? '').trim();

    if (!name || name.toLowerCase() === 'undefined' || name.toLowerCase() === 'any') {
      errors.push(`Invalid foliage type name: '${params?.name}'`);
    }
    if (!meshPath || meshPath.toLowerCase() === 'undefined') {
      errors.push(`Invalid meshPath: '${params?.meshPath}'`);
    }
    if (params?.density !== undefined) {
      if (typeof params.density !== 'number' || !isFinite(params.density) || params.density < 0) {
        errors.push(`Invalid density: '${params.density}' (must be non-negative finite number)`);
      }
    }
    if (params?.minScale !== undefined || params?.maxScale !== undefined) {
      const minS = params?.minScale ?? 1;
      const maxS = params?.maxScale ?? 1;
      if (typeof minS !== 'number' || typeof maxS !== 'number' || minS <= 0 || maxS <= 0 || maxS < minS) {
        errors.push(`Invalid scale range: min=${params?.minScale}, max=${params?.maxScale}`);
      }
    }
    if (errors.length > 0) {
      return { success: false, error: errors.join('; ') };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Foliage operations require plugin support.');
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('add_foliage_type', {
        name,
        meshPath,
        density: params.density,
        radius: params.radius,
        minScale: params.minScale,
        maxScale: params.maxScale,
        alignToNormal: params.alignToNormal,
        randomYaw: params.randomYaw,
        groundSlope: params.groundSlope
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Add foliage type failed',
          note: coerceString((response.result as any)?.note)
        };
      }

      const payload = response.result as Record<string, unknown>;
      const created = coerceBoolean(payload.created, false) ?? false;
      const exists = coerceBoolean(payload.exists_after, false) ?? created;
      const method = coerceString(payload.method) ?? 'Unknown';
      const assetPath = coerceString(payload.asset_path);
      const usedMesh = coerceString(payload.used_mesh);
      const note = coerceString(payload.note);

      return {
        success: true,
        created,
        exists,
        method,
        assetPath,
        usedMesh,
        note,
        message: exists
          ? `Foliage type '${name}' ready (${method})`
          : `Created foliage '${name}' but verification did not find it yet`
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to add foliage type: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Paint foliage by placing HISM instances (editor-only)
  async paintFoliage(params: {
    foliageType: string;
    position: [number, number, number];
    brushSize?: number;
    paintDensity?: number;
    eraseMode?: boolean;
  }) {
    const errors: string[] = [];
    const foliageType = String(params?.foliageType ?? '').trim();
    const pos = Array.isArray(params?.position) ? params.position : [0,0,0];

    if (!foliageType || foliageType.toLowerCase() === 'undefined' || foliageType.toLowerCase() === 'any') {
      errors.push(`Invalid foliageType: '${params?.foliageType}'`);
    }
    if (!Array.isArray(pos) || pos.length !== 3 || pos.some(v => typeof v !== 'number' || !isFinite(v))) {
      errors.push(`Invalid position: '${JSON.stringify(params?.position)}'`);
    }
    if (params?.brushSize !== undefined) {
      if (typeof params.brushSize !== 'number' || !isFinite(params.brushSize) || params.brushSize < 0) {
        errors.push(`Invalid brushSize: '${params.brushSize}' (must be non-negative finite number)`);
      }
    }
    if (params?.paintDensity !== undefined) {
      if (typeof params.paintDensity !== 'number' || !isFinite(params.paintDensity) || params.paintDensity < 0) {
        errors.push(`Invalid paintDensity: '${params.paintDensity}' (must be non-negative finite number)`);
      }
    }

    if (errors.length > 0) {
      return { success: false, error: errors.join('; ') };
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Foliage operations require plugin support.');
    }

    const brush = Number.isFinite(params.brushSize as number) ? (params.brushSize as number) : 300;

    try {
      const response = await this.automationBridge.sendAutomationRequest('paint_foliage', {
        foliageType,
        position: pos,
        brushSize: brush,
        paintDensity: params.paintDensity,
        eraseMode: params.eraseMode
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Paint foliage failed',
          note: coerceString((response.result as any)?.note)
        };
      }

      const payload = response.result as Record<string, unknown>;
      const added = coerceNumber(payload.added) ?? 0;
      const actor = coerceString(payload.actor);
      const component = coerceString(payload.component);
      const usedMesh = coerceString(payload.used_mesh);
      const note = coerceString(payload.note);

      return {
        success: true,
        added,
        actor,
        component,
        usedMesh,
        note,
        message: `Painted ${added} instances for '${foliageType}' around (${pos[0]}, ${pos[1]}, ${pos[2]})`
      };
    } catch (error) {
      return {
        success: false,
        error: `Failed to paint foliage: ${error instanceof Error ? error.message : String(error)}`
      };
    }
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Instanced mesh ${params.name} created with ${params.instances.length} instances` };
  }

  // Set foliage LOD
  async setFoliageLOD(params: {
    foliageType: string;
    lodDistances?: number[];
    screenSize?: number[];
  }) {
  const commands: string[] = [];
    
    if (params.lodDistances) {
      commands.push(`SetFoliageLODDistances ${params.foliageType} ${params.lodDistances.join(' ')}`);
    }
    
    if (params.screenSize) {
      commands.push(`SetFoliageLODScreenSize ${params.foliageType} ${params.screenSize.join(' ')}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Foliage LOD settings updated' };
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Procedural foliage volume ${params.volumeName} created` };
  }

  // Set foliage collision
  async setFoliageCollision(params: {
    foliageType: string;
    collisionEnabled?: boolean;
    collisionProfile?: string;
    generateOverlapEvents?: boolean;
  }) {
  const commands: string[] = [];
    
    if (params.collisionEnabled !== undefined) {
      commands.push(`SetFoliageCollision ${params.foliageType} ${params.collisionEnabled}`);
    }
    
    if (params.collisionProfile) {
      commands.push(`SetFoliageCollisionProfile ${params.foliageType} ${params.collisionProfile}`);
    }
    
    if (params.generateOverlapEvents !== undefined) {
      commands.push(`SetFoliageOverlapEvents ${params.foliageType} ${params.generateOverlapEvents}`);
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Foliage collision settings updated' };
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
  const commands: string[] = [];
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: `Grass system ${params.name} created` };
  }

  // Remove foliage instances
  async removeFoliageInstances(params: {
    foliageType: string;
    position: [number, number, number];
    radius: number;
  }) {
    const command = `RemoveFoliageInRadius ${params.foliageType} ${params.position.join(' ')} ${params.radius}`;
    return this.bridge.executeConsoleCommand(command);
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
    
    return this.bridge.executeConsoleCommand(command);
  }

  // Update foliage instances
  async updateFoliageInstances(params: {
    foliageType: string;
    updateTransforms?: boolean;
    updateMesh?: boolean;
    newMeshPath?: string;
  }) {
  const commands: string[] = [];
    
    if (params.updateTransforms) {
      commands.push(`UpdateFoliageTransforms ${params.foliageType}`);
    }
    
    if (params.updateMesh && params.newMeshPath) {
      commands.push(`UpdateFoliageMesh ${params.foliageType} ${params.newMeshPath}`);
    }
    
    commands.push(`RefreshFoliage ${params.foliageType}`);
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Foliage instances updated' };
  }

  // Create foliage spawner
  async createFoliageSpawner(params: {
    name: string;
    spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All';
    excludeAreas?: Array<[number, number, number, number]>; // [x, y, z, radius]
  }) {
  const commands: string[] = [];
    
    commands.push(`CreateFoliageSpawner ${params.name} ${params.spawnArea}`);
    
    if (params.excludeAreas) {
      for (const area of params.excludeAreas) {
        commands.push(`AddFoliageExclusionArea ${params.name} ${area.join(' ')}`);
      }
    }
    
    await this.bridge.executeConsoleCommands(commands);
    
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
    
    await this.bridge.executeConsoleCommands(commands);
    
    return { success: true, message: 'Foliage optimized' };
  }
}
