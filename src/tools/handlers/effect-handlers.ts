import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EffectArgs, AutomationResponse } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

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

  // paramType → parameterType
  const rawParamType = (mutableArgs.paramType as string | undefined);
  if (rawParamType && !(mutableArgs.parameterType as string | undefined)) {
    mutableArgs.parameterType = rawParamType;
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
    // Extract name and savePath from assetPath if provided
    const resolvedName = (argsTyped.name as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.split('/').pop()?.replace(/\.[^.]+$/, '') : undefined)
      || 'NS_Custom';
    const resolvedSavePath = (mutableArgs.savePath as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.replace(/\/[^/]+$/, '') : '/Game/FX');
    const res = await executeAutomationRequest(tools, 'create_niagara_system', {
      name: resolvedName,
      savePath: resolvedSavePath,
    }) as Record<string, unknown>;
    return cleanObject(res);
  }
  if (action === 'create_niagara_emitter') {
    const resolvedName = (argsTyped.name as string | undefined)
      || (mutableArgs.emitterName as string | undefined) || 'DefaultEmitter';
    const resolvedSavePath = (mutableArgs.savePath as string | undefined) || '/Game/FX';
    const res = await executeAutomationRequest(tools, 'create_niagara_emitter', {
      name: resolvedName,
      savePath: resolvedSavePath,
    }) as Record<string, unknown>;
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
  }

  // =========================================================================
  // Debug shapes
  // =========================================================================
  if (action === 'debug_shape' || mutableArgs.action === 'debug_shape') {
    // Map 'shape' to 'shapeType' if provided (schema uses 'shape', C++ uses 'shapeType')
    if (argsTyped.shape && !mutableArgs.shapeType) {
      mutableArgs.shapeType = argsTyped.shape;
    }
    mutableArgs.action = 'debug_shape';
    mutableArgs.subAction = 'debug_shape';
    return cleanObject(await executeAutomationRequest(tools, 'create_effect', mutableArgs)) as Record<string, unknown>;
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
    mutableArgs.action = 'create_effect';
    mutableArgs.subAction = action;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Simulation control actions
  // =========================================================================
  if (action === 'activate' || action === 'activate_effect') {
    mutableArgs.action = 'activate_niagara';
    mutableArgs.subAction = 'activate_niagara';
    // Accept effect, effectHandle, niagaraHandle, actorName, or systemName as the identifier
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) || 
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) || 
                             (mutableArgs.systemName as string | undefined);
    // Use user's reset value if provided, default to true for activate
    if (mutableArgs.reset === undefined) {
      mutableArgs.reset = true;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'deactivate') {
    mutableArgs.action = 'deactivate_niagara';
    mutableArgs.subAction = 'deactivate_niagara';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) || 
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) || 
                             (mutableArgs.systemName as string | undefined);
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'reset') {
    mutableArgs.action = 'activate_niagara';
    mutableArgs.subAction = 'activate_niagara';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) || 
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) || 
                             (mutableArgs.systemName as string | undefined);
    if (mutableArgs.reset === undefined) {
      mutableArgs.reset = true;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'advance_simulation') {
    mutableArgs.action = 'advance_simulation';
    mutableArgs.subAction = 'advance_simulation';
    mutableArgs.systemName = (mutableArgs.effect as string | undefined) ||
                             (mutableArgs.effectHandle as string | undefined) || 
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) || 
                             (mutableArgs.systemName as string | undefined);
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara parameter setting
  // =========================================================================
  if (action === 'set_niagara_parameter') {
    mutableArgs.action = 'set_niagara_parameter';
    mutableArgs.subAction = 'set_niagara_parameter';
    mutableArgs.systemName = (mutableArgs.effectHandle as string | undefined) || 
                             (mutableArgs.niagaraHandle as string | undefined) ||
                             (mutableArgs.actorName as string | undefined) || 
                             (mutableArgs.systemName as string | undefined);
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara graph manipulation → manage_niagara_graph
  // =========================================================================
  const graphActions = ['add_niagara_module', 'connect_niagara_pins', 'remove_niagara_node'];
  if (graphActions.includes(action)) {
    const subActionMap: Record<string, string> = {
      'add_niagara_module': 'add_module',
      'connect_niagara_pins': 'connect_pins',
      'remove_niagara_node': 'remove_node',
    };
    mutableArgs.subAction = subActionMap[action] || action;
    return executeAutomationRequest(tools, 'manage_niagara_graph', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara authoring → manage_niagara_authoring
  // =========================================================================
  const authoringActions = [
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
  ];
  if (authoringActions.includes(action)) {
    return executeAutomationRequest(tools, 'manage_niagara_authoring', mutableArgs) as Promise<Record<string, unknown>>;
  }

  // =========================================================================
  // Niagara/spawn_niagara via create_effect
  // =========================================================================
  if (action === 'niagara' || action === 'spawn_niagara') {
    mutableArgs.action = 'create_effect';
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
