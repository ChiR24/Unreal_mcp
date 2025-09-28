import { UnrealBridge } from '../unreal-bridge.js';
import { sanitizeAssetName, validateAssetParams } from '../utils/validation.js';
import { interpretStandardResult, coerceString } from '../utils/result-helpers.js';

export class NiagaraTools {
  constructor(private bridge: UnrealBridge) {}

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
    const python = `
import unreal
import json

path = r"${path}"
name = r"${params.name}"
full_path = f"{path}/{name}"

if unreal.EditorAssetLibrary.does_asset_exist(full_path):
    print('RESULT:' + json.dumps({'success': True, 'path': full_path, 'existing': True}))
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = None
    try:
        factory = unreal.NiagaraSystemFactoryNew()
    except Exception:
        factory = None

    if factory is None:
        print('RESULT:' + json.dumps({'success': False, 'error': 'NiagaraSystemFactoryNew unavailable'}))
    else:
        asset = asset_tools.create_asset(asset_name=name, package_path=path, asset_class=unreal.NiagaraSystem, factory=factory)
        if asset:
            unreal.EditorAssetLibrary.save_asset(full_path)
            print('RESULT:' + json.dumps({'success': True, 'path': full_path}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'AssetTools create_asset failed'}))
`.trim();
      const resp = await this.bridge.executePython(python);
      const interpreted = interpretStandardResult(resp, {
        successMessage: `Niagara system ${params.name} created`,
        failureMessage: `Failed to create Niagara system ${params.name}`
      });

      if (!interpreted.success) {
        return { success: false, error: interpreted.error ?? interpreted.message };
      }

      const pathFromPayload = coerceString(interpreted.payload.path) ?? `${path}/${params.name}`;
      return {
        success: true,
        path: pathFromPayload,
        message: interpreted.message
      };
    } catch (err) {
      return { success: false, error: `Failed to create Niagara system: ${err}` };
    }
  }

  /**
   * Add Emitter to System (left as-is; console commands may be placeholders)
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

      // Verify existence via Python to avoid RC EditorAssetLibrary issues
      const verifyPy = `
import unreal
import json

p = r"${fullPath}"
exists = bool(unreal.EditorAssetLibrary.does_asset_exist(p))
print('RESULT:' + json.dumps({'success': True, 'exists': exists}))
`.trim();
      const verifyResp = await this.bridge.executePython(verifyPy);
      const verifyResult = interpretStandardResult(verifyResp, {
        successMessage: 'Niagara asset verification complete',
        failureMessage: `Failed to verify Niagara asset at ${fullPath}`
      });

      if (!verifyResult.success) {
        return { success: false, error: verifyResult.error ?? verifyResult.message };
      }

      if (verifyResult.payload.exists === false) {
        return { success: false, error: `Asset not found after creation: ${fullPath}` };
      }

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
      const rot = params.rotation || [0, 0, 0];
      const scl = Array.isArray(params.scale) ? params.scale : (typeof params.scale === 'number' ? [params.scale, params.scale, params.scale] : [1, 1, 1]);
    const py = `
import unreal
import json

loc = unreal.Vector(${loc.x || 0}, ${loc.y || 0}, ${loc.z || 0})
rot = unreal.Rotator(${rot[0]}, ${rot[1]}, ${rot[2]})
scale = unreal.Vector(${scl[0]}, ${scl[1]}, ${scl[2]})
sys_path = r"${params.systemPath}"

if unreal.EditorAssetLibrary.does_asset_exist(sys_path):
  sys = unreal.EditorAssetLibrary.load_asset(sys_path)
  actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  actor = actor_subsystem.spawn_actor_from_class(unreal.NiagaraActor, loc, rot)
  if actor:
    comp = actor.get_niagara_component()
    try:
      comp.set_asset(sys)
    except Exception:
      try:
        comp.set_editor_property('asset', sys)
      except Exception:
        pass
    comp.set_world_scale3d(scale)
    comp.activate(True)
    actor.set_actor_label(f"Niagara_{unreal.SystemLibrary.get_game_time_in_seconds(actor.get_world()):.0f}")
    print('RESULT:' + json.dumps({'success': True, 'actor': actor.get_actor_label()}))
  else:
    print('RESULT:' + json.dumps({'success': False, 'error': 'Failed to spawn NiagaraActor'}))
else:
  print('RESULT:' + json.dumps({'success': False, 'error': 'System asset not found'}))
`.trim();
    const resp = await this.bridge.executePython(py);
    const interpreted = interpretStandardResult(resp, {
      successMessage: 'Niagara effect spawned',
      failureMessage: 'Failed to spawn Niagara effect'
    });

    const actorLabel = coerceString(interpreted.payload.actor);

    if (!interpreted.success) {
      return { success: false, error: interpreted.error ?? interpreted.message };
    }

    const outcome: { success: true; message: string; actor?: string } = {
      success: true,
      message: interpreted.message
    };

    if (actorLabel) {
      outcome.actor = actorLabel;
    }

    return outcome;
    } catch (err) {
      return { success: false, error: `Failed to spawn effect: ${err}` };
    }
  }

}
