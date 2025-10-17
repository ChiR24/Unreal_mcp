import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { sanitizeAssetName, validateAssetParams } from '../utils/validation.js';

export class NiagaraTools {
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  /**
   * Create Niagara System (real asset via Python)
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
    const path = params.savePath || '/Game/Effects/Niagara';
    // Prefer plugin-first creation
    if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
      try {
        const res: any = await this.automationBridge.sendAutomationRequest('create_niagara_system', { name: params.name, savePath: path, template: params.template }, { timeoutMs: 60000 });
        if (res && res.success !== false) {
          return { success: true, path: res.path || res.result?.path || `${path}/${params.name}`, message: res.message || 'Niagara system created' } as any;
        }
        return { success: false, message: res?.message ?? 'Niagara create failed', error: res?.error ?? 'CREATE_NIAGARA_FAILED' } as any;
      } catch (error) {
        return { success: false, error: `Failed to create Niagara system: ${error instanceof Error ? error.message : String(error)}` };
      }
    }

    throw new Error('Automation Bridge not available. Niagara system creation requires plugin support.');
    } catch (err) {
      return { success: false, error: `Failed to create Niagara system: ${err}` };
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
    try {
      const commands = [
        `AddNiagaraEmitter ${params.systemName} ${params.emitterName} ${params.emitterType}`
      ];
      if (params.properties) {
        const props = params.properties;
        if (props.spawnRate !== undefined) {
          commands.push(`SetEmitterSpawnRate ${params.systemName} ${params.emitterName} ${props.spawnRate}`);
        }
        if (props.lifetime !== undefined) {
          commands.push(`SetEmitterLifetime ${params.systemName} ${params.emitterName} ${props.lifetime}`);
        }
        if (props.velocityMin && props.velocityMax) {
          const min = props.velocityMin; const max = props.velocityMax;
          commands.push(`SetEmitterVelocity ${params.systemName} ${params.emitterName} ${min[0]} ${min[1]} ${min[2]} ${max[0]} ${max[1]} ${max[2]}`);
        }
        if (props.size !== undefined) {
          commands.push(`SetEmitterSize ${params.systemName} ${params.emitterName} ${props.size}`);
        }
        if (props.color) {
          const color = props.color;
          commands.push(`SetEmitterColor ${params.systemName} ${params.emitterName} ${color[0]} ${color[1]} ${color[2]} ${color[3]}`);
        }
        if (props.material) {
          commands.push(`SetEmitterMaterial ${params.systemName} ${params.emitterName} ${props.material}`);
        }
        if (props.mesh && params.emitterType === 'Mesh') {
          commands.push(`SetEmitterMesh ${params.systemName} ${params.emitterName} ${props.mesh}`);
        }
      }
      await this.bridge.executeConsoleCommands(commands);
      return { success: true, message: `Emitter ${params.emitterName} added to ${params.systemName}` };
    } catch (err) {
      return { success: false, error: `Failed to add emitter: ${err}` };
    }
  }

  async setParameter(params: {
    systemName: string;
    parameterName: string;
    parameterType: 'Float' | 'Vector' | 'Color' | 'Bool' | 'Int';
    value: any;
    isUserParameter?: boolean;
  }) {
    try {
      const paramType = params.isUserParameter ? 'User' : 'System';
      let valueStr = '';
      // Prefer plugin transport when available
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('set_niagara_parameter', {
            systemName: params.systemName,
            parameterName: params.parameterName,
            parameterType: params.parameterType,
            value: params.value,
            isUserParameter: params.isUserParameter === true
          });
          if (resp && resp.success !== false) return { success: true, message: resp.message || `Parameter ${params.parameterName} set on ${params.systemName}`, applied: resp.applied ?? resp.result?.applied } as any;
          const errTxt = String(resp?.error ?? resp?.message ?? '');
          if (errTxt.toLowerCase().includes('unknown') || errTxt.includes('UNKNOWN_PLUGIN_ACTION')) {
            // fall through to console fallback
          } else {
            return { success: false, message: resp?.message ?? 'Set parameter failed', error: resp?.error ?? 'SET_PARAMETER_FAILED' } as any;
          }
        } catch (_e) {
          // fall back to console execution below
        }
      }
      switch (params.parameterType) {
        case 'Float': case 'Int': case 'Bool': valueStr = String(params.value); break;
        case 'Vector': { const v = params.value as number[]; valueStr = `${v[0]} ${v[1]} ${v[2]}`; break; }
        case 'Color': { const c = params.value as number[]; valueStr = `${c[0]} ${c[1]} ${c[2]} ${c[3] || 1}`; break; }
      }
      const command = `SetNiagara${paramType}Parameter ${params.systemName} ${params.parameterName} ${params.parameterType} ${valueStr}`;
      await this.bridge.executeConsoleCommand(command);
      return { success: true, message: `Parameter ${params.parameterName} set on ${params.systemName}` };
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
      const allowedTypes = ['Fire','Smoke','Explosion','Water','Rain','Snow','Magic','Lightning','Dust','Steam'];
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
   * Spawn Niagara Effect in Level using Python (NiagaraActor)
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
            return { success: true, message: resp.message || 'Niagara effect spawned', actor: resp.actor || resp.result?.actor } as any;
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
