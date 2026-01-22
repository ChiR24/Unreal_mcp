// Foliage tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { coerceBoolean, coerceNumber, coerceString } from '../utils/result-helpers.js';
import { IFoliageTools, StandardActionResponse } from '../types/tool-interfaces.js';
import { sanitizeAssetName, sanitizePath, sanitizeConsoleString } from '../utils/validation.js';

export class FoliageTools implements IFoliageTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

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
  }): Promise<StandardActionResponse> {
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
      const base = meshPath.includes('.') ? meshPath : `${meshPath}.${meshPath.split('/').filter(Boolean).pop()}`;
      const response = await this.automationBridge.sendAutomationRequest('add_foliage_type', {
        name,
        meshPath: base,
        density: params.density ?? 100,
        radius: params.radius ?? 0,
        minScale: params.minScale ?? 1.0,
        maxScale: params.maxScale ?? 1.0,
        alignToNormal: params.alignToNormal ?? true,
        randomYaw: params.randomYaw ?? true,
        groundSlope: params.groundSlope ?? 45
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Add foliage type failed',
          note: coerceString((response.result as Record<string, unknown>)?.note)
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
      } as StandardActionResponse;
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
  }): Promise<StandardActionResponse> {
    const errors: string[] = [];
    const foliageType = String(params?.foliageType ?? '').trim();
    const pos = Array.isArray(params?.position) ? params.position : [0, 0, 0];

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

    try {
      const typePath = foliageType.includes('/') ? foliageType : `/Game/Foliage/${foliageType}.${foliageType}`;
      const response = await this.automationBridge.sendAutomationRequest('paint_foliage', {
        foliageTypePath: typePath,
        locations: [{ x: pos[0], y: pos[1], z: pos[2] }],
        brushSize: Number.isFinite(params.brushSize as number) ? (params.brushSize as number) : 300,
        paintDensity: params.paintDensity,
        eraseMode: params.eraseMode
      }, {
        timeoutMs: 60000
      });

      if (response.success === false) {
        return {
          success: false,
          error: response.error || response.message || 'Paint foliage failed',
          note: coerceString((response.result as Record<string, unknown>)?.note)
        };
      }

      const payload = response.result as Record<string, unknown>;
      const added = coerceNumber(payload.instancesPlaced) ?? coerceNumber(payload.count as number | undefined) ?? 0;
      const note = coerceString(payload.note);

      return {
        success: true,
        added,
        note,
        message: `Painted ${added} instances for '${foliageType}' around (${pos[0]}, ${pos[1]}, ${pos[2]})`
      } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to paint foliage: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Query foliage instances (plugin-native)
  async getFoliageInstances(params: { foliageType?: string }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Foliage operations require plugin support.');
    }
    try {
      const typePath = params.foliageType ? (params.foliageType.includes('/') ? params.foliageType : `/Game/Foliage/${params.foliageType}.${params.foliageType}`) : undefined;
      const response = await this.automationBridge.sendAutomationRequest('get_foliage_instances', {
        foliageTypePath: typePath
      }, { timeoutMs: 60000 });
      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Get foliage instances failed' };
      }
      const payload = response.result as Record<string, unknown>;
      return {
        success: true,
        count: coerceNumber(payload.count) ?? 0,
        instances: (payload.instances as Array<Record<string, unknown>>) ?? [],
        message: 'Foliage instances retrieved'
      } as StandardActionResponse;
    } catch (error) {
      return { success: false, error: `Failed to get foliage instances: ${error instanceof Error ? error.message : String(error)}` };
    }
  }

  // Remove foliage (plugin-native)
  async removeFoliage(params: { foliageType?: string; removeAll?: boolean }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Foliage operations require plugin support.');
    }
    try {
      const typePath = params.foliageType ? (params.foliageType.includes('/') ? params.foliageType : `/Game/Foliage/${params.foliageType}.${params.foliageType}`) : undefined;
      const response = await this.automationBridge.sendAutomationRequest('remove_foliage', {
        foliageTypePath: typePath,
        removeAll: !!params.removeAll
      }, { timeoutMs: 60000 });
      if (response.success === false) {
        return { success: false, error: response.error || response.message || 'Remove foliage failed' };
      }
      const payload = response.result as Record<string, unknown>;
      return {
        success: true,
        instancesRemoved: coerceNumber(payload.instancesRemoved) ?? 0,
        message: 'Foliage removed'
      } as StandardActionResponse;
    } catch (error) {
      return { success: false, error: `Failed to remove foliage: ${error instanceof Error ? error.message : String(error)}` };
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
  }): Promise<StandardActionResponse> {
    // Sanitize inputs
    const sanitizedName = sanitizeAssetName(params.name);
    const sanitizedPath = sanitizePath(params.meshPath);

    const commands: string[] = [];

    commands.push(`CreateInstancedStaticMesh ${sanitizedName} ${sanitizedPath}`);

    for (const instance of params.instances) {
      const rot = instance.rotation || [0, 0, 0];
      const scale = instance.scale || [1, 1, 1];
      commands.push(`AddInstance ${sanitizedName} ${instance.position.join(' ')} ${rot.join(' ')} ${scale.join(' ')}`);
    }

    if (params.enableCulling !== undefined) {
      commands.push(`SetInstanceCulling ${sanitizedName} ${params.enableCulling}`);
    }

    if (params.cullDistance !== undefined) {
      commands.push(`SetInstanceCullDistance ${sanitizedName} ${params.cullDistance}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: `Instanced mesh ${sanitizedName} created with ${params.instances.length} instances` };
  }

  // Set foliage LOD
  async setFoliageLOD(params: {
    foliageType: string;
    lodDistances?: number[];
    screenSize?: number[];
  }): Promise<StandardActionResponse> {
    // Sanitize foliageType - it's likely a path or name, but console string sanitization is safer for unknown usage
    const sanitizedType = sanitizeConsoleString(params.foliageType);

    const commands: string[] = [];

    if (params.lodDistances) {
      commands.push(`SetFoliageLODDistances ${sanitizedType} ${params.lodDistances.join(' ')}`);
    }

    if (params.screenSize) {
      commands.push(`SetFoliageLODScreenSize ${sanitizedType} ${params.screenSize.join(' ')}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Foliage LOD settings updated' };
  }

  // Alias for addFoliageType to match interface/handler usage
  async addFoliage(params: { foliageType: string; locations: Array<{ x: number; y: number; z: number }> }): Promise<StandardActionResponse> {
    // Delegate to paintFoliage which handles placing instances at locations
    if (params.locations && params.locations.length > 0) {
      if (!this.automationBridge) {
        throw new Error('Automation Bridge not available.');
      }

      const response = await this.automationBridge.sendAutomationRequest('paint_foliage', {
        foliageTypePath: params.foliageType.includes('/') ? params.foliageType : `/Game/Foliage/${params.foliageType}.${params.foliageType}`,
        locations: params.locations,
        brushSize: 0, // Exact placement
        paintDensity: 1,
        eraseMode: false
      });

      if (!response.success) {
        return { success: false, error: response.error || 'Failed to add foliage instances' };
      }

      return { success: true, message: `Added ${params.locations.length} foliage instances` };
    }

    return { success: true, message: 'No locations provided for addFoliage' };
  }

  // Create procedural foliage
  async createProceduralFoliage(params: {
    name: string;
    bounds?: { location: { x: number; y: number; z: number }; size: { x: number; y: number; z: number } };
    foliageTypes?: Array<{
      meshPath: string;
      density: number;
      minScale?: number;
      maxScale?: number;
      alignToNormal?: boolean;
      randomYaw?: boolean;
    }>;
    // Legacy params compatibility
    volumeName?: string;
    position?: [number, number, number];
    size?: [number, number, number];
    seed?: number;
    tileSize?: number;
  }): Promise<StandardActionResponse> {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available.');
    }

    const volName = params.volumeName || params.name || 'ProceduralFoliageVolume';
    const loc = params.bounds?.location ? [params.bounds.location.x, params.bounds.location.y, params.bounds.location.z] : (params.position || [0, 0, 0]);
    const size = params.bounds?.size ? [params.bounds.size.x, params.bounds.size.y, params.bounds.size.z] : (params.size || [1000, 1000, 100]);

    // Normalize foliage types from both formats
    const foliageTypes = Array.isArray(params.foliageTypes)
      ? params.foliageTypes.map(t => {
        if (typeof t === 'string') return { meshPath: t, density: 0.5 };
        return t;
      })
      : [];

    const payload = {
      name: volName,
      bounds: {
        location: { x: loc[0], y: loc[1], z: loc[2] },
        size: { x: size[0], y: size[1], z: size[2] }
      },
      foliageTypes,
      seed: params.seed ?? 42,
      tileSize: params.tileSize ?? 1000
    };

    const response = await this.automationBridge.sendAutomationRequest('create_procedural_foliage', payload);

    if (!response.success) {
      return {
        success: false,
        error: response.error || 'Failed to create procedural foliage'
      };
    }

    const result = (response.result ?? {}) as Record<string, unknown>;
    return {
      success: true,
      message: `Procedural foliage volume ${volName} created`,
      details: response,
      volumeActor: result.volume_actor,
      spawnerPath: result.spawner_path,
      foliageTypesCount: result.foliage_types_count
    } as StandardActionResponse;
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
  }): Promise<StandardActionResponse> {
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

        const result = (response.result ?? {}) as Record<string, unknown>;
        return {
          success: true,
          message: response.message || `Added ${result.instances_count ?? params.transforms.length} foliage instances`,
          instancesCount: result.instances_count
        } as StandardActionResponse;
    } catch (error) {
      return {
        success: false,
        error: `Failed to add foliage instances: ${error instanceof Error ? error.message : String(error)}`
      };
    }
  }

  // Set foliage collision
  async setFoliageCollision(params: {
    foliageType: string;
    collisionEnabled?: boolean;
    collisionProfile?: string;
    generateOverlapEvents?: boolean;
  }): Promise<StandardActionResponse> {
    const sanitizedType = sanitizeConsoleString(params.foliageType);
    const sanitizedProfile = params.collisionProfile ? sanitizeAssetName(params.collisionProfile) : undefined;

    const commands: string[] = [];

    if (params.collisionEnabled !== undefined) {
      commands.push(`SetFoliageCollision ${sanitizedType} ${params.collisionEnabled}`);
    }

    if (sanitizedProfile) {
      commands.push(`SetFoliageCollisionProfile ${sanitizedType} ${sanitizedProfile}`);
    }

    if (params.generateOverlapEvents !== undefined) {
      commands.push(`SetFoliageOverlapEvents ${sanitizedType} ${params.generateOverlapEvents}`);
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
  }): Promise<StandardActionResponse> {
    const sanitizedName = sanitizeAssetName(params.name);
    const commands: string[] = [];

    commands.push(`CreateGrassSystem ${sanitizedName}`);

    for (const grassType of params.grassTypes) {
      const minScale = grassType.minScale || 0.8;
      const maxScale = grassType.maxScale || 1.2;
      const sanitizedMeshPath = sanitizePath(grassType.meshPath);
      commands.push(`AddGrassType ${sanitizedName} ${sanitizedMeshPath} ${grassType.density} ${minScale} ${maxScale}`);
    }

    if (params.windStrength !== undefined) {
      commands.push(`SetGrassWindStrength ${sanitizedName} ${params.windStrength}`);
    }

    if (params.windSpeed !== undefined) {
      commands.push(`SetGrassWindSpeed ${sanitizedName} ${params.windSpeed}`);
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: `Grass system ${sanitizedName} created` };
  }

  // Remove foliage instances
  async removeFoliageInstances(params: {
    foliageType: string;
    position: [number, number, number];
    radius: number;
  }): Promise<StandardActionResponse> {
    const sanitizedType = sanitizeConsoleString(params.foliageType);
    const command = `RemoveFoliageInRadius ${sanitizedType} ${params.position.join(' ')} ${params.radius}`;
    return this.bridge.executeConsoleCommand(command);
  }

  // Select foliage instances
  async selectFoliageInstances(params: {
    foliageType: string;
    position?: [number, number, number];
    radius?: number;
    selectAll?: boolean;
  }): Promise<StandardActionResponse> {
    const sanitizedType = sanitizeConsoleString(params.foliageType);
    let command: string;

    if (params.selectAll) {
      command = `SelectAllFoliage ${sanitizedType}`;
    } else if (params.position && params.radius) {
      command = `SelectFoliageInRadius ${sanitizedType} ${params.position.join(' ')} ${params.radius}`;
    } else {
      command = `SelectFoliageType ${sanitizedType}`;
    }

    return this.bridge.executeConsoleCommand(command);
  }

  // Update foliage instances
  async updateFoliageInstances(params: {
    foliageType: string;
    updateTransforms?: boolean;
    updateMesh?: boolean;
    newMeshPath?: string;
  }): Promise<StandardActionResponse> {
    const sanitizedType = sanitizeConsoleString(params.foliageType);
    const sanitizedNewMeshPath = params.newMeshPath ? sanitizePath(params.newMeshPath) : undefined;

    const commands: string[] = [];

    if (params.updateTransforms) {
      commands.push(`UpdateFoliageTransforms ${sanitizedType}`);
    }

    if (params.updateMesh && sanitizedNewMeshPath) {
      commands.push(`UpdateFoliageMesh ${sanitizedType} ${sanitizedNewMeshPath}`);
    }

    commands.push(`RefreshFoliage ${sanitizedType}`);

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: 'Foliage instances updated' };
  }

  // Create foliage spawner
  async createFoliageSpawner(params: {
    name: string;
    spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All';
    excludeAreas?: Array<[number, number, number, number]>; // [x, y, z, radius]
  }): Promise<StandardActionResponse> {
    const sanitizedName = sanitizeAssetName(params.name);
    const commands: string[] = [];

    commands.push(`CreateFoliageSpawner ${sanitizedName} ${params.spawnArea}`);

    if (params.excludeAreas) {
      for (const area of params.excludeAreas) {
        commands.push(`AddFoliageExclusionArea ${sanitizedName} ${area.join(' ')}`);
      }
    }

    await this.bridge.executeConsoleCommands(commands);

    return { success: true, message: `Foliage spawner ${sanitizedName} created` };
  }

  // Optimize foliage
  async optimizeFoliage(params: {
    mergeInstances?: boolean;
    generateClusters?: boolean;
    clusterSize?: number;
    reduceDrawCalls?: boolean;
  }): Promise<StandardActionResponse> {
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
