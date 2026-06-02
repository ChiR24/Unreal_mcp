import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EffectArgs, AutomationResponse } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { sanitizePath } from '../../utils/path-security.js';

const DEFAULT_EFFECT_SAVE_PATH = '/Game/MCPTest/ManageEffectDefaults';
const DEFAULT_NIAGARA_SYSTEM_NAME = `MCP_ManageEffectDefaultSystem_${process.pid}`;
const DEFAULT_NIAGARA_EMITTER_ASSET_NAME = `MCP_ManageEffectDefaultEmitter_${process.pid}`;
const DEFAULT_NIAGARA_AUTHORING_SYSTEM_NAME = `MCP_ManageEffectAuthoringSystem_${process.pid}`;
const DEFAULT_NIAGARA_AUTHORING_EMITTER_ASSET_NAME = `MCP_ManageEffectAuthoringEmitter_${process.pid}`;
const DEFAULT_NIAGARA_EMITTER_NAME = 'DefaultEmitter';
const DEFAULT_NIAGARA_ACTOR_NAME = `MCP_ManageEffectDefaultActor_${process.pid}`;

let defaultNiagaraAssetsPromise: Promise<{ systemPath: string; emitterPath: string }> | undefined;
let defaultNiagaraAuthoringAssetsPromise: Promise<{ systemPath: string; emitterPath: string }> | undefined;
let lastCreatedNiagaraSystemPath: string | undefined;
let lastCreatedNiagaraEmitterPath: string | undefined;
let lastAddedNiagaraUserParameterName: string | undefined;

const EFFECT_GRAPH_ACTIONS = new Set<string>([
  'add_niagara_module',
  'connect_niagara_pins',
  'remove_niagara_node'
]);

const EFFECT_GRAPH_SUB_ACTIONS: Record<string, string> = {
  'add_niagara_module': 'add_module',
  'connect_niagara_pins': 'connect_pins',
  'remove_niagara_node': 'remove_node',
};

const EFFECT_AUTHORING_ACTIONS = new Set<string>([
  'add_emitter_to_system', 'set_emitter_properties',
  'add_spawn_rate_module', 'add_spawn_burst_module', 'add_spawn_per_unit_module',
  'add_initialize_particle_module', 'add_particle_state_module',
  'add_force_module', 'add_velocity_module', 'add_acceleration_module',
  'add_size_module', 'add_color_module',
  'add_sprite_renderer_module', 'add_mesh_renderer_module',
  'add_ribbon_renderer_module', 'add_light_renderer_module',
  'add_collision_module', 'add_kill_particles_module', 'add_camera_offset_module',
  'add_user_parameter', 'set_parameter_value', 'bind_parameter_to_source',
  'add_skeletal_mesh_data_interface', 'add_static_mesh_data_interface',
  'add_spline_data_interface', 'add_audio_spectrum_data_interface',
  'add_collision_query_data_interface',
  'add_event_generator', 'add_event_receiver', 'configure_event_payload',
  'enable_gpu_simulation', 'add_simulation_stage',
  'get_niagara_info', 'validate_niagara_system'
]);

function makeGameAssetPath(savePath: string, assetName: string): string {
  return `${savePath.replace(/\/$/, '')}/${assetName}`;
}

function makeGameObjectPath(savePath: string, assetName: string): string {
  const assetPath = makeGameAssetPath(savePath, assetName);
  return `${assetPath}.${assetName}`;
}

async function ensureDefaultNiagaraAssets(tools: ITools): Promise<{ systemPath: string; emitterPath: string }> {
  if (!defaultNiagaraAssetsPromise) {
    defaultNiagaraAssetsPromise = (async () => {
      const systemResult = await executeAutomationRequest(tools, 'create_niagara_system', {
        name: DEFAULT_NIAGARA_SYSTEM_NAME,
        path: DEFAULT_EFFECT_SAVE_PATH,
        savePath: DEFAULT_EFFECT_SAVE_PATH,
        save: false,
      }) as Record<string, unknown>;
      if (systemResult.success === false) {
        throw new Error(`Failed to create default Niagara system: ${String(systemResult.message || systemResult.error || 'unknown error')}`);
      }

      const emitterResult = await executeAutomationRequest(tools, 'create_niagara_emitter', {
        name: DEFAULT_NIAGARA_EMITTER_ASSET_NAME,
        path: DEFAULT_EFFECT_SAVE_PATH,
        savePath: DEFAULT_EFFECT_SAVE_PATH,
        save: false,
      }) as Record<string, unknown>;
      if (emitterResult.success === false) {
        throw new Error(`Failed to create default Niagara emitter: ${String(emitterResult.message || emitterResult.error || 'unknown error')}`);
      }

      const systemPayload = (systemResult.result ?? systemResult) as Record<string, unknown>;
      const emitterPayload = (emitterResult.result ?? emitterResult) as Record<string, unknown>;
      const systemPath = typeof systemPayload.systemPath === 'string' && systemPayload.systemPath.length > 0
        ? systemPayload.systemPath
        : makeGameObjectPath(DEFAULT_EFFECT_SAVE_PATH, DEFAULT_NIAGARA_SYSTEM_NAME);
      const emitterPath = typeof emitterPayload.emitterPath === 'string' && emitterPayload.emitterPath.length > 0
        ? emitterPayload.emitterPath
        : makeGameObjectPath(DEFAULT_EFFECT_SAVE_PATH, DEFAULT_NIAGARA_EMITTER_ASSET_NAME);

      return {
        systemPath,
        emitterPath,
      };
    })();
  }

  return defaultNiagaraAssetsPromise;
}

async function ensureDefaultNiagaraAuthoringAssets(tools: ITools): Promise<{ systemPath: string; emitterPath: string }> {
  if (lastCreatedNiagaraSystemPath && lastCreatedNiagaraEmitterPath) {
    return {
      systemPath: lastCreatedNiagaraSystemPath,
      emitterPath: lastCreatedNiagaraEmitterPath,
    };
  }

  if (!defaultNiagaraAuthoringAssetsPromise) {
    defaultNiagaraAuthoringAssetsPromise = (async () => {
      const systemResult = await executeAutomationRequest(tools, 'create_niagara_system', {
        name: DEFAULT_NIAGARA_AUTHORING_SYSTEM_NAME,
        path: DEFAULT_EFFECT_SAVE_PATH,
        savePath: DEFAULT_EFFECT_SAVE_PATH,
        save: false,
      }) as Record<string, unknown>;
      if (systemResult.success === false) {
        throw new Error(`Failed to create default Niagara authoring system: ${String(systemResult.message || systemResult.error || 'unknown error')}`);
      }

      const emitterResult = await executeAutomationRequest(tools, 'create_niagara_emitter', {
        name: DEFAULT_NIAGARA_AUTHORING_EMITTER_ASSET_NAME,
        path: DEFAULT_EFFECT_SAVE_PATH,
        savePath: DEFAULT_EFFECT_SAVE_PATH,
        save: false,
      }) as Record<string, unknown>;
      if (emitterResult.success === false) {
        throw new Error(`Failed to create default Niagara authoring emitter: ${String(emitterResult.message || emitterResult.error || 'unknown error')}`);
      }

      const systemPayload = (systemResult.result ?? systemResult) as Record<string, unknown>;
      const emitterPayload = (emitterResult.result ?? emitterResult) as Record<string, unknown>;
      const systemPath = typeof systemPayload.systemPath === 'string' && systemPayload.systemPath.length > 0
        ? systemPayload.systemPath
        : makeGameObjectPath(DEFAULT_EFFECT_SAVE_PATH, DEFAULT_NIAGARA_AUTHORING_SYSTEM_NAME);
      const emitterPath = typeof emitterPayload.emitterPath === 'string' && emitterPayload.emitterPath.length > 0
        ? emitterPayload.emitterPath
        : makeGameObjectPath(DEFAULT_EFFECT_SAVE_PATH, DEFAULT_NIAGARA_AUTHORING_EMITTER_ASSET_NAME);

      return {
        systemPath,
        emitterPath,
      };
    })();
  }

  return defaultNiagaraAuthoringAssetsPromise;
}

async function ensureDefaultNiagaraActor(tools: ITools): Promise<void> {
  const defaultAssets = await ensureDefaultNiagaraAssets(tools);
  await executeAutomationRequest(tools, 'create_effect', {
    action: 'niagara',
    systemPath: defaultAssets.systemPath,
    actorName: DEFAULT_NIAGARA_ACTOR_NAME,
    location: { x: 0, y: 0, z: 100 },
  });
}

function ensureActionAndSubAction(action: string, args: Record<string, unknown>): void {
  if (!args || typeof args !== 'object') return;
  // Many callers pass the tool action as the action name (e.g. "niagara") and
  // omit args.action; the native handler requires subAction.
  if (!args.action) {
    args.action = action;
  }
  if (!args.subAction) {
    args.subAction = args.action;
  }
}

function sanitizeEffectPaths(args: Record<string, unknown>): void {
  const pathFields = [
    'path',
    'savePath',
    'assetPath',
    'systemPath',
    'emitterPath',
    'material',
    'materialPath',
    'mesh',
    'meshPath',
    'skeletalMeshPath',
    'staticMeshPath',
    'preset',
    'system'
  ];

  for (const field of pathFields) {
    const value = args[field];
    if (typeof value === 'string' && value.trim().startsWith('/')) {
      args[field] = sanitizePath(value);
    }
  }
}

/** Result payload structure for effect responses */
interface ResultPayload {
  error?: string;
  message?: string;
}

export async function handleEffectTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = args as EffectArgs;
  const mutableArgs = { ...args } as Record<string, unknown>;

  if (!mutableArgs || typeof mutableArgs !== 'object') {
    // Create empty object
  }

  // Always ensure action/subAction are present before any routing.
  ensureActionAndSubAction(action, mutableArgs);
  sanitizeEffectPaths(mutableArgs);

  // =========================================================================
  // PARAMETER NORMALIZATION — map test-friendly aliases to C++ field names
  // =========================================================================

  // system → systemPath (used by niagara, spawn_niagara, and many authoring actions)
  const rawSystem = (mutableArgs.system as string | undefined);
  if (rawSystem && !(mutableArgs.systemPath as string | undefined)) {
    mutableArgs.systemPath = rawSystem;
  }

  // template → preset (for particle action — tests send `template`, C++ needs `preset`)
  const rawTemplate = (mutableArgs.template as string | undefined);
  if (rawTemplate && !(mutableArgs.preset as string | undefined)) {
    mutableArgs.preset = rawTemplate;
  }

  // path → savePath for asset creation actions.
  const rawPath = (mutableArgs.path as string | undefined);
  if (rawPath && !(mutableArgs.savePath as string | undefined)) {
    mutableArgs.savePath = rawPath;
  }

  // emitter → emitterName (for Niagara module actions)
  const rawEmitter = (mutableArgs.emitter as string | undefined);
  if (rawEmitter && !(mutableArgs.emitterName as string | undefined)) {
    mutableArgs.emitterName = rawEmitter;
  }

  // paramName → parameterName
  const rawParamName = (mutableArgs.paramName as string | undefined);
  if (rawParamName && !(mutableArgs.parameterName as string | undefined)) {
    mutableArgs.parameterName = rawParamName;
  }

  const rawPropertyName = (mutableArgs.propertyName as string | undefined);
  if (rawPropertyName && !(mutableArgs.parameterName as string | undefined)) {
    mutableArgs.parameterName = rawPropertyName;
  }

  if (mutableArgs.propertyValue !== undefined && mutableArgs.value === undefined) {
    mutableArgs.value = mutableArgs.propertyValue;
  }

  if (mutableArgs.propertyValue !== undefined && mutableArgs.parameterValue === undefined) {
    mutableArgs.parameterValue = mutableArgs.propertyValue;
  }

  // paramType → parameterType
  const rawParamType = (mutableArgs.paramType as string | undefined);
  if (rawParamType && !(mutableArgs.parameterType as string | undefined)) {
    mutableArgs.parameterType = rawParamType;
  }

  // Existing public aliases → native Niagara field names.
  const rawMaterial = (mutableArgs.material as string | undefined);
  if (rawMaterial && !(mutableArgs.materialPath as string | undefined)) {
    mutableArgs.materialPath = rawMaterial;
  }

  const rawMesh = (mutableArgs.mesh as string | undefined);
  if (rawMesh && !(mutableArgs.meshPath as string | undefined)) {
    mutableArgs.meshPath = rawMesh;
  }

  if (typeof mutableArgs.count === 'number' && mutableArgs.burstCount === undefined) {
    mutableArgs.burstCount = mutableArgs.count;
  }

  if (typeof mutableArgs.unitsPerSpawn === 'number' && mutableArgs.spawnPerUnit === undefined) {
    mutableArgs.spawnPerUnit = mutableArgs.unitsPerSpawn;
  }

  if (typeof mutableArgs.strength === 'number' && mutableArgs.forceStrength === undefined) {
    mutableArgs.forceStrength = mutableArgs.strength;
  }

  if (typeof mutableArgs.offsetAmount === 'number' && mutableArgs.cameraOffset === undefined) {
    mutableArgs.cameraOffset = mutableArgs.offsetAmount;
  }

  // type → parameterType (for set_niagara_parameter)
  const rawType = (mutableArgs.type as string | undefined);
  if (rawType && !(mutableArgs.parameterType as string | undefined)) {
    mutableArgs.parameterType = rawType.charAt(0).toUpperCase() + rawType.slice(1);
  }

  // Re-read systemPath after alias mapping (used in response normalization)
  const effectiveSystemPath = (mutableArgs.systemPath as string | undefined);

  // =========================================================================
  // Niagara asset creation (uses dedicated C++ handlers)
  // =========================================================================
  if (action === 'create_niagara_system') {
    // The manage_effect tool schema exposes the name as `systemName`, so accept
    // it first. Fall back to `name` (legacy) and a systemPath-derived name
    // before the `NS_Custom` placeholder — otherwise every caller ends up with
    // the same default asset name and the per-call systemName is silently lost.
    const resolvedName = (argsTyped.systemName as string | undefined)
      || (argsTyped.name as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.split('/').pop()?.replace(/\.[^.]+$/, '') : undefined)
      || 'NS_Custom';
    const resolvedSavePath = (mutableArgs.savePath as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.replace(/\/[^/]+$/, '') : '/Game/FX');
      const res = await executeAutomationRequest(tools, 'create_niagara_system', {
        name: resolvedName,
        path: resolvedSavePath,
        savePath: resolvedSavePath,
      }) as Record<string, unknown>;
    const result = (res.result ?? res) as Record<string, unknown>;
    const createdPath = result.systemPath;
    if (typeof createdPath === 'string' && createdPath.length > 0) {
      lastCreatedNiagaraSystemPath = createdPath;
    }
    return cleanObject(res);
  }
  if (action === 'create_niagara_emitter') {
    // Same pattern: accept `emitterName` (top-level schema key) before falling
    // back to the lower-precedence aliases so the caller's name isn't ignored.
    const resolvedName = (argsTyped.emitterName as string | undefined)
      || (argsTyped.name as string | undefined)
      || (mutableArgs.emitterName as string | undefined)
      || 'DefaultEmitter';
    const resolvedSavePath = (mutableArgs.savePath as string | undefined) || '/Game/FX';
      const res = await executeAutomationRequest(tools, 'create_niagara_emitter', {
        name: resolvedName,
        path: resolvedSavePath,
        savePath: resolvedSavePath,
      }) as Record<string, unknown>;
    const result = (res.result ?? res) as Record<string, unknown>;
    const createdPath = result.emitterPath;
    if (typeof createdPath === 'string' && createdPath.length > 0) {
      lastCreatedNiagaraEmitterPath = createdPath;
    }
    return cleanObject(res);
  }

  // =========================================================================
  // Particle preset resolution
  // =========================================================================
  if (mutableArgs.action === 'particle' || mutableArgs.subAction === 'particle') {
    const presets: Record<string, string> = {
      'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
      'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
      'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
      'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
    };
    // Check preset field (already normalized from template above), and aliases
    const rawPreset = (mutableArgs.preset || argsTyped.type) as string | undefined;
    if (rawPreset) {
      // If it's a known preset KEY, resolve to full path
      if (presets[rawPreset]) {
        mutableArgs.preset = presets[rawPreset];
      }
      // If it's already a path (starts with /), use it directly
      else if (rawPreset.startsWith('/')) {
        mutableArgs.preset = rawPreset;
      }
    }
    if (!mutableArgs.preset) {
      mutableArgs.preset = presets.Default;
    }
  }

  // =========================================================================
  // Debug shapes
  // =========================================================================
  if (action === 'debug_shape' || mutableArgs.action === 'debug_shape') {
    // Map 'shape' to 'shapeType' if provided (schema uses 'shape', C++ uses 'shapeType')
    if (argsTyped.shape && !mutableArgs.shapeType) {
      mutableArgs.shapeType = argsTyped.shape;
    }
    if (!mutableArgs.location) {
      mutableArgs.location = { x: 0, y: 0, z: 100 };
    }
    mutableArgs.action = 'debug_shape';
    mutableArgs.subAction = 'debug_shape';
    return cleanObject(await executeAutomationRequest(tools, 'create_effect', mutableArgs)) as Record<string, unknown>;
  }

  if (action === 'create_dynamic_light' || mutableArgs.action === 'create_dynamic_light') {
    if (!mutableArgs.location) {
      mutableArgs.location = { x: 0, y: 0, z: 250 };
    }
    if (!mutableArgs.lightName && mutableArgs.name) {
      mutableArgs.lightName = mutableArgs.name;
    }
  }

  // =========================================================================
  // Debug cleanup / list actions (pass-through to C++)
  // =========================================================================
  if (action === 'clear_debug_shapes') {
    return executeAutomationRequest(tools, action, mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'list_debug_shapes') {
    return executeAutomationRequest(tools, 'list_debug_shapes', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'cleanup') {
    mutableArgs.action = 'cleanup';
    mutableArgs.subAction = 'cleanup';
    if (!mutableArgs.filter) {
      mutableArgs.filter = 'MCP_ManageEffectDefaultActor_';
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // High-level actions → create_effect routing
  // =========================================================================
  const createActions = [
    'create_volumetric_fog',
    'create_particle_trail',
    'create_environment_effect',
    'create_impact_effect',
    'create_niagara_ribbon'
  ];
  if (createActions.includes(action)) {
    if (action !== 'create_volumetric_fog' && !mutableArgs.systemPath) {
      const defaultAssets = await ensureDefaultNiagaraAssets(tools);
      mutableArgs.systemPath = defaultAssets.systemPath;
    }
    mutableArgs.action = action;
    mutableArgs.subAction = action;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Simulation control actions
  // =========================================================================
  if (action === 'activate' || action === 'activate_effect') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'activate_niagara';
    mutableArgs.subAction = 'activate_niagara';
    // Accept effect, effectHandle, niagaraHandle, actorName, or systemName as the identifier
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) ||
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) ||
                             (mutableArgs.systemName as string | undefined) ||
                             DEFAULT_NIAGARA_ACTOR_NAME;
    // Use user's reset value if provided, default to true for activate
    if (mutableArgs.reset === undefined) {
      mutableArgs.reset = true;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'deactivate') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'deactivate_niagara';
    mutableArgs.subAction = 'deactivate_niagara';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) ||
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) ||
                             (mutableArgs.systemName as string | undefined) ||
                             DEFAULT_NIAGARA_ACTOR_NAME;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'reset') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'activate_niagara';
    mutableArgs.subAction = 'activate_niagara';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) ||
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) ||
                             (mutableArgs.systemName as string | undefined) ||
                             DEFAULT_NIAGARA_ACTOR_NAME;
    if (mutableArgs.reset === undefined) {
      mutableArgs.reset = true;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'advance_simulation') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'advance_simulation';
    mutableArgs.subAction = 'advance_simulation';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) ||
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) ||
                             (mutableArgs.systemName as string | undefined) ||
                             DEFAULT_NIAGARA_ACTOR_NAME;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara parameter setting
  // =========================================================================
  if (action === 'set_niagara_parameter') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'set_niagara_parameter';
    mutableArgs.subAction = 'set_niagara_parameter';
    mutableArgs.systemName = (mutableArgs.effectHandle as string | undefined) ||
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) ||
                             (mutableArgs.systemName as string | undefined) ||
                             DEFAULT_NIAGARA_ACTOR_NAME;
    if (!mutableArgs.parameterName) {
      mutableArgs.parameterName = 'MCPParameter';
    }
    if (mutableArgs.value === undefined) {
      mutableArgs.value = 0;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara graph manipulation → manage_niagara_graph
  // =========================================================================
  if (EFFECT_GRAPH_ACTIONS.has(action)) {
    const defaultAssets = await ensureDefaultNiagaraAssets(tools);
    mutableArgs.subAction = EFFECT_GRAPH_SUB_ACTIONS[action] || action;
    if (!mutableArgs.assetPath) {
      mutableArgs.assetPath = defaultAssets.systemPath;
    }
    if (!mutableArgs.systemPath) {
      mutableArgs.systemPath = defaultAssets.systemPath;
    }
    if (action === 'add_niagara_module' && !mutableArgs.modulePath) {
      mutableArgs.modulePath = '/Niagara/Modules/Emitter/EmitterState.EmitterState';
    }
    if (action === 'connect_niagara_pins' && !mutableArgs.fromNode && !mutableArgs.fromPin && !mutableArgs.toNode && !mutableArgs.toPin) {
      mutableArgs.autoConnect = true;
    }
    return executeAutomationRequest(tools, 'manage_niagara_graph', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara authoring → manage_niagara_authoring
  // =========================================================================
  if (EFFECT_AUTHORING_ACTIONS.has(action)) {
    const usesImplicitSystem = !mutableArgs.systemPath;
    const defaultAssets = await ensureDefaultNiagaraAuthoringAssets(tools);
    if (!mutableArgs.systemPath) {
      mutableArgs.systemPath = defaultAssets.systemPath;
    }
    if (!mutableArgs.emitterPath) {
      mutableArgs.emitterPath = defaultAssets.emitterPath;
    }
    if (!mutableArgs.emitterName) {
      mutableArgs.emitterName = DEFAULT_NIAGARA_EMITTER_NAME;
    }
    if (!mutableArgs.assetPath) {
      mutableArgs.assetPath = defaultAssets.systemPath;
    }
    if (!mutableArgs.parameterName) {
      mutableArgs.parameterName = (mutableArgs.name as string | undefined) || 'MCPParameter';
    }
    if (action === 'add_user_parameter' && typeof mutableArgs.parameterName === 'string') {
      lastAddedNiagaraUserParameterName = mutableArgs.parameterName;
    }
    if (
      action === 'set_parameter_value' &&
      (!mutableArgs.parameterName || mutableArgs.parameterName === mutableArgs.propertyName)
    ) {
      mutableArgs.parameterName = lastAddedNiagaraUserParameterName || 'MCPParameter';
    }
    if (!mutableArgs.parameterType) {
      mutableArgs.parameterType = 'Float';
    }
    if (!mutableArgs.forceType) {
      mutableArgs.forceType = 'gravity';
    }
    if (!mutableArgs.sourceBinding) {
      mutableArgs.sourceBinding = 'Emitter.Age';
    }
    if (!mutableArgs.eventName) {
      mutableArgs.eventName = (mutableArgs.name as string | undefined) || 'MCPEvent';
    }
    if (!mutableArgs.stageName) {
      mutableArgs.stageName = (mutableArgs.name as string | undefined) || 'MCPSimulationStage';
    }
    if (mutableArgs.value === undefined) {
      mutableArgs.value = 1;
    }
    if (usesImplicitSystem && mutableArgs.save === undefined) {
      mutableArgs.save = false;
    }
    return executeAutomationRequest(tools, 'manage_niagara_authoring', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara/spawn_niagara via create_effect
  // =========================================================================
  if (action === 'niagara' || action === 'spawn_niagara') {
    if (!mutableArgs.systemPath) {
      const defaultAssets = await ensureDefaultNiagaraAssets(tools);
      mutableArgs.systemPath = defaultAssets.systemPath;
    }
    if (!mutableArgs.actorName && !mutableArgs.name) {
      mutableArgs.actorName = DEFAULT_NIAGARA_ACTOR_NAME;
    }
    mutableArgs.action = 'niagara';
    mutableArgs.subAction = 'niagara';
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Default: pass-through to create_effect
  // =========================================================================
  const res = await executeAutomationRequest(
    tools,
    'create_effect',
    mutableArgs,
    'Automation bridge not available for effect creation operations'
  ) as AutomationResponse;

  const result = (res?.result ?? res ?? {}) as ResultPayload;

  const topError = typeof res?.error === 'string' ? res.error : '';
  const nestedError = typeof result.error === 'string' ? result.error : '';
  const errorCode = (topError || nestedError).toUpperCase();

  const topMessage = typeof res?.message === 'string' ? res.message : '';
  const nestedMessage = typeof result.message === 'string' ? result.message : '';
  const message = topMessage || nestedMessage || '';

  const combined = `${topError} ${nestedError} ${message}`.toLowerCase();

  // Normalize SYSTEM_NOT_FOUND for niagara/spawn_niagara actions
  if (
    (action === 'niagara' || action === 'spawn_niagara') &&
    (
      errorCode === 'SYSTEM_NOT_FOUND' ||
      combined.includes('niagara system not found') ||
      combined.includes('system asset not found')
    )
  ) {
    return cleanObject({
      success: false,
      error: 'SYSTEM_NOT_FOUND',
      message: message || 'Niagara system not found',
      systemPath: effectiveSystemPath,
      handled: true
    });
  }

  return cleanObject(res) as Record<string, unknown>;
}
