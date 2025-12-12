import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { sanitizeAssetName, validateAssetParams } from '../utils/validation.js';

type Vector3 = [number, number, number];


export class NiagaraTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  /**
   * Create Niagara System
   */
  async createSystem(params: {
    name: string;
    savePath?: string;
    template?: 'Empty' | 'Fountain' | 'Ambient' | 'Projectile' | 'Custom';
    emitters?: Array<{
      name: string;
      spawnRate?: number;
      lifetime?: number;
      shape?: 'Point' | 'Sphere' | 'Box' | 'Cylinder' | 'Cone';
      shapeSize?: [number, number, number];
    }>;
  }) {
    try {
      if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
        throw new Error('Automation Bridge not available. Niagara system creation requires plugin support.');
      }

      const path = params.savePath || '/Game/Effects/Niagara';
      const response: any = await this.automationBridge.sendAutomationRequest(
        'create_niagara_system',
        { name: params.name, savePath: path, template: params.template },
        { timeoutMs: 60000 }
      );

      if (response && response.success !== false) {
        const result = response.result ?? {};
        const systemName: string = result.systemName ?? params.name;
        const systemPath: string = response.path ?? result.systemPath ?? result.path ?? `${path}/${params.name}`;
        return {
          success: true,
          systemName,
          path: systemPath,
          message: response.message || result.message || `Niagara system ${systemName} created`
        } as const;
      }

      return {
        success: false,
        error: response?.error ?? 'CREATE_NIAGARA_SYSTEM_FAILED',
        message: response?.message ?? 'Niagara system creation failed'
      } as const;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return { success: false, error: `Failed to create Niagara system: ${message}` } as const;
    }
  }

  async createEmitter(params: {
    name: string;
    savePath?: string;
    systemPath?: string;
    template?: string;
  }) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'createEmitter requires automation bridge' } as const;
    }

    const requestPayload: Record<string, unknown> = {
      name: params.name,
      savePath: params.savePath ?? '/Game/Effects/Niagara'
    };
    if (params.systemPath) requestPayload.systemPath = params.systemPath;
    if (params.template) requestPayload.template = params.template;

    try {
      const response: any = await this.automationBridge.sendAutomationRequest('create_niagara_emitter', requestPayload, { timeoutMs: 60000 });
      if (response && response.success !== false) {
        const result = response.result ?? {};
        return {
          success: true,
          emitterPath: response.emitterPath ?? result.emitterPath ?? result.path,
          emitterName: result.emitterName ?? params.name,
          message: response.message || result.message || `Niagara emitter ${params.name} created`
        } as const;
      }

      return {
        success: false,
        error: response?.error ?? 'CREATE_NIAGARA_EMITTER_FAILED',
        message: response?.message ?? 'Niagara emitter creation failed'
      } as const;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return { success: false, error: `Failed to create Niagara emitter: ${message}` } as const;
    }
  }

  async createRibbon(params: {
    systemPath: string;
    start?: { x: number; y: number; z: number } | [number, number, number];
    end?: { x: number; y: number; z: number } | [number, number, number];
    color?: [number, number, number, number];
    width?: number;
  }) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'createRibbon requires automation bridge' } as const;
    }

    const toVector = (value?: { x: number; y: number; z: number } | Vector3): Vector3 | undefined => {
      if (!value) return undefined;
      if (Array.isArray(value)) return [value[0] ?? 0, value[1] ?? 0, value[2] ?? 0];
      return [value.x ?? 0, value.y ?? 0, value.z ?? 0];
    };

    const requestPayload: Record<string, unknown> = { systemPath: params.systemPath };
    const start = toVector(params.start);
    const end = toVector(params.end);
    if (start) requestPayload.start = start;
    if (end) requestPayload.end = end;
    if (params.color) requestPayload.color = params.color;
    if (typeof params.width === 'number') requestPayload.width = params.width;

    try {
      const response: any = await this.automationBridge.sendAutomationRequest('create_niagara_ribbon', requestPayload, { timeoutMs: 60000 });
      if (response && response.success !== false) {
        const result = response.result ?? {};
        return {
          success: true,
          ribbonPath: response.ribbonPath ?? result.ribbonPath ?? result.path,
          message: response.message || result.message || 'Niagara ribbon created'
        } as const;
      }

      return {
        success: false,
        error: response?.error ?? 'CREATE_NIAGARA_RIBBON_FAILED',
        message: response?.message ?? 'Niagara ribbon creation failed'
      } as const;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return { success: false, error: `Failed to create Niagara ribbon: ${message}` } as const;
    }
  }

  async cleanupEffects(params: { filter: string }) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'cleanupEffects requires automation bridge' } as const;
    }
    if (!params.filter || typeof params.filter !== 'string') {
      return { success: false, error: 'INVALID_ARGUMENT', message: 'filter is required' } as const;
    }

    try {
      const response: any = await this.automationBridge.sendAutomationRequest('cleanup', { filter: params.filter }, { timeoutMs: 60000 });
      if (response && response.success !== false) {
        const result = response.result ?? {};
        const removedActors: string[] = result.removedActors ?? response.removedActors ?? [];
        const removedCount = result.removed ?? removedActors.length;
        return {
          success: true,
          removed: removedCount,
          removedActors,
          message: response.message || result.message || `Cleanup completed (removed=${removedCount})`
        } as const;
      }

      return {
        success: false,
        error: response?.error ?? 'CLEANUP_FAILED',
        message: response?.message ?? 'Niagara cleanup failed'
      } as const;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      return { success: false, error: `Failed to cleanup Niagara effects: ${message}` } as const;
    }
  }

  /**
   * Add Emitter to System
   */
  async addEmitter(params: {
    systemName: string;
    emitterName: string;
    emitterType: 'Sprite' | 'Mesh' | 'Ribbon' | 'Beam' | 'GPU';
    properties?: {
      spawnRate?: number;
      lifetime?: number;
      velocityMin?: [number, number, number];
      velocityMax?: [number, number, number];
      size?: number;
      color?: [number, number, number, number];
      material?: string;
      mesh?: string;
    };
  }) {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'addEmitter requires automation bridge' } as const;
    }

    try {
      const resp: any = await this.automationBridge.sendAutomationRequest('manage_niagara_graph', {
        subAction: 'add_emitter',
        systemName: params.systemName,
        emitterName: params.emitterName,
        emitterType: params.emitterType,
        properties: params.properties
      });

      if (resp && resp.success !== false) {
        return {
          success: true,
          message: resp.message || `Emitter ${params.emitterName} added to ${params.systemName}`,
          emitterId: resp.result?.emitterId
        };
      }

      return {
        success: false,
        error: resp?.error || 'ADD_EMITTER_FAILED',
        message: resp?.message || 'Failed to add emitter'
      };
    } catch (e) {
      const message = e instanceof Error ? e.message : String(e);
      return { success: false, error: 'ADD_EMITTER_FAILED', message };
    }
  }

  async addModule(params: {
    systemPath: string;
    modulePath: string;
    emitterName?: string;
    scriptType?: 'Spawn' | 'Update';
    timeoutMs?: number;
  }) {
    if (!params.systemPath) return { success: false, error: 'INVALID_SYSTEM_PATH', message: 'System path is required' } as const;
    if (!params.modulePath) return { success: false, error: 'INVALID_MODULE_PATH', message: 'Module path is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_niagara_graph', {
      subAction: 'add_module',
      assetPath: params.systemPath,
      modulePath: params.modulePath,
      emitterName: params.emitterName,
      scriptType: params.scriptType
    });
    return res;
  }

  async connectPins(params: {
    systemPath: string;
    fromNodeId: string;
    fromPinName: string;
    toNodeId: string;
    toPinName: string;
    emitterName?: string;
    scriptType?: 'Spawn' | 'Update';
    timeoutMs?: number;
  }) {
    if (!params.systemPath) return { success: false, error: 'INVALID_SYSTEM_PATH', message: 'System path is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_niagara_graph', {
      subAction: 'connect_pins',
      assetPath: params.systemPath,
      fromNode: params.fromNodeId,
      fromPin: params.fromPinName,
      toNode: params.toNodeId,
      toPin: params.toPinName,
      emitterName: params.emitterName,
      scriptType: params.scriptType
    });
    return res;
  }

  async removeNode(params: {
    systemPath: string;
    nodeId: string;
    emitterName?: string;
    scriptType?: 'Spawn' | 'Update';
    timeoutMs?: number;
  }) {
    if (!params.systemPath) return { success: false, error: 'INVALID_SYSTEM_PATH', message: 'System path is required' } as const;
    if (!params.nodeId) return { success: false, error: 'INVALID_NODE_ID', message: 'Node ID is required' } as const;

    const res = await this.automationBridge?.sendAutomationRequest('manage_niagara_graph', {
      subAction: 'remove_node',
      assetPath: params.systemPath,
      nodeId: params.nodeId,
      emitterName: params.emitterName,
      scriptType: params.scriptType
    });
    return res;
  }

  async setParameter(params: {
    systemName: string;
    parameterName: string;
    parameterType: 'Float' | 'Vector' | 'Color' | 'Bool' | 'Int';
    value: any;
    isUserParameter?: boolean;
  }) {
    // Note: This uses 'set_niagara_parameter' top-level action, OR 'manage_niagara_graph' with subAction 'set_parameter'.
    // The previous implementation used 'set_niagara_parameter'. 
    // The C++ 'manage_niagara_graph' also has 'set_parameter'.
    // I will keep existing logic if it works, or switch to manage_niagara_graph if preferred.
    // Given the audit, 'manage_niagara_graph' is the graph-based one.
    // The existing setParameter uses 'set_niagara_parameter' which might be instance-based?
    // Let's stick to existing unless broken, but I'll add the graph-based one as setGraphParameter?
    // User requested "implement all missing". I'll stick to adding missing graph methods I just verified.

    try {
      const automationBridge = (this.bridge as any).automationBridge;
      if (!automationBridge) {
        return { success: false, error: 'Automation bridge not available' };
      }
      const resp: any = await automationBridge.sendAutomationRequest('set_niagara_parameter', {
        systemName: params.systemName,
        parameterName: params.parameterName,
        parameterType: params.parameterType,
        value: params.value,
        isUserParameter: params.isUserParameter === true
      });
      if (resp && resp.success !== false) return { success: true, message: resp.message || `Parameter ${params.parameterName} set on ${params.systemName}`, applied: resp.applied ?? resp.result?.applied } as any;
      return { success: false, message: resp?.message ?? 'Set parameter failed', error: resp?.error ?? 'SET_PARAMETER_FAILED' } as any;
    } catch (err) {
      return { success: false, error: `Failed to set parameter: ${err}` };
    }
  }

  /**
   * Create Preset Effect (now creates a real Niagara system asset)
   */
  async createEffect(params: {
    effectType: 'Fire' | 'Smoke' | 'Explosion' | 'Water' | 'Rain' | 'Snow' | 'Magic' | 'Lightning' | 'Dust' | 'Steam';
    name: string;
    location: [number, number, number] | { x: number, y: number, z: number };
    scale?: number;
    intensity?: number;
    customParameters?: { [key: string]: any };
  }) {
    try {
      // Validate effect type at runtime (inputs can come from JSON)
      const allowedTypes = ['Fire', 'Smoke', 'Explosion', 'Water', 'Rain', 'Snow', 'Magic', 'Lightning', 'Dust', 'Steam', 'Default'];
      if (!params || !allowedTypes.includes(String(params.effectType))) {
        return { success: false, error: `Invalid effectType: ${String(params?.effectType)}` };
      }

      // Sanitize and validate name and path
      const defaultPath = '/Game/Effects/Niagara';
      const nameToUse = sanitizeAssetName(params.name);
      const validation = validateAssetParams({ name: nameToUse, savePath: defaultPath });
      if (!validation.valid) {
        return { success: false, error: validation.error || 'Invalid asset parameters' };
      }
      const safeName = validation.sanitized.name;
      const savePath = validation.sanitized.savePath || defaultPath;
      const fullPath = `${savePath}/${safeName}`;

      // Create or ensure the Niagara system asset exists
      const createRes = await this.createSystem({ name: safeName, savePath, template: 'Empty' });
      if (!createRes.success) {
        return { success: false, error: createRes.error || 'Failed creating Niagara system' };
      }

      // Asset created successfully
      return { success: true, message: `${params.effectType} effect ${safeName} created`, path: fullPath };
    } catch (err) {
      return { success: false, error: `Failed to create effect: ${err}` };
    }
  }

  async createGPUSimulation(params: {
    name: string;
    simulationType: 'Fluid' | 'Hair' | 'Cloth' | 'Debris' | 'Crowd';
    particleCount: number;
    savePath?: string;
    gpuSettings?: {
      computeShader?: string;
      textureFormat?: 'RGBA8' | 'RGBA16F' | 'RGBA32F';
      gridResolution?: [number, number, number];
      iterations?: number;
    };
  }) {
    try {
      const path = params.savePath || '/Game/Effects/GPUSimulations';
      const commands = [`CreateGPUSimulation ${params.name} ${params.simulationType} ${params.particleCount} ${path}`];
      if (params.gpuSettings) {
        const s = params.gpuSettings;
        if (s.computeShader) commands.push(`SetGPUComputeShader ${params.name} ${s.computeShader}`);
        if (s.textureFormat) commands.push(`SetGPUTextureFormat ${params.name} ${s.textureFormat}`);
        if (s.gridResolution) { const r = s.gridResolution; commands.push(`SetGPUGridResolution ${params.name} ${r[0]} ${r[1]} ${r[2]}`); }
        if (s.iterations !== undefined) commands.push(`SetGPUIterations ${params.name} ${s.iterations}`);
      }
      await this.bridge.executeConsoleCommands(commands);
      return { success: true, message: `GPU simulation ${params.name} created`, path: `${path}/${params.name}` };
    } catch (err) {
      return { success: false, error: `Failed to create GPU simulation: ${err}` };
    }
  }

  /**
   * Spawn Niagara Effect in Level (NiagaraActor)
   */
  async spawnEffect(params: {
    systemPath: string;
    location: [number, number, number] | { x: number, y: number, z: number };
    rotation?: [number, number, number];
    scale?: [number, number, number] | number;
    autoDestroy?: boolean;
    attachToActor?: string;
  }) {
    try {
      const loc = Array.isArray(params.location) ? { x: params.location[0], y: params.location[1], z: params.location[2] } : params.location;
      // Prefer plugin transport when available
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('spawn_niagara', {
            systemPath: params.systemPath,
            location: [loc.x ?? 0, loc.y ?? 0, loc.z ?? 0],
            rotation: params.rotation,
            scale: params.scale,
            autoDestroy: params.autoDestroy,
            attachToActor: params.attachToActor
          });
          if (resp && resp.success !== false) {
            return { success: true, message: resp.message || 'Niagara effect spawned', actor: resp.actor || resp.result?.actor || resp.result?.actorName } as any;
          }
          return { success: false, message: resp?.message ?? 'Spawn failed', error: resp?.error ?? 'SPAWN_FAILED' } as any;
        } catch (error) {
          return { success: false, error: `Failed to spawn effect: ${error instanceof Error ? error.message : String(error)}` };
        }
      }

      throw new Error('Automation Bridge not available. Niagara effect spawning requires plugin support.');
    } catch (err) {
      return { success: false, error: `Failed to spawn effect: ${err}` };
    }
  }

}
