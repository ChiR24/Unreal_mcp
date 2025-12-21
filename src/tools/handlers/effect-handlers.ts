import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

function ensureActionAndSubAction(action: string, args: any) {
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

function isNonEmptyString(val: unknown): val is string {
  return typeof val === 'string' && val.trim().length > 0;
}

export async function handleEffectTools(action: string, args: any, tools: ITools) {
  if (!args || typeof args !== 'object') {
    args = {};
  }

  // Always ensure action/subAction are present before any routing.
  ensureActionAndSubAction(action, args);

  // Handle creation actions explicitly to use NiagaraTools helper
  if (action === 'create_niagara_system') {
    const res = await tools.niagaraTools.createSystem({
      name: args.name,
      savePath: args.savePath,
      template: args.template
    });
    return cleanObject(res);
  }
  if (action === 'create_niagara_emitter') {
    const res = await tools.niagaraTools.createEmitter({
      name: args.name,
      savePath: args.savePath,
      systemPath: args.systemPath,
      template: args.template
    });
    return cleanObject(res);
  }

  // Pre-process arguments for particle presets
  if (args.action === 'particle') {
    const presets: Record<string, string> = {
      'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
      'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
      'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
      'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
    };
    // Check both preset and effectType fields
    const key = args.preset || args.effectType;
    if (key && presets[key]) {
      args.preset = presets[key];
    }
  }

  // Handle debug shapes (must happen before any automation request)
  if (action === 'debug_shape' || args.action === 'debug_shape') {
    // Map 'shape' to 'shapeType' if provided (schema uses 'shape', C++ uses 'shapeType')
    if (args.shape && !args.shapeType) {
      args.shapeType = args.shape;
    }
    requireNonEmptyString(args.shapeType, 'shapeType', 'Missing required parameter: shapeType');
    args.action = 'debug_shape';
    args.subAction = 'debug_shape';
    return cleanObject(await executeAutomationRequest(tools, 'create_effect', args));
  }

  // Validate Niagara-related required parameters (keep errors explicit and early)
  const subAction = String(args.subAction || '').trim();
  if (subAction === 'niagara' || subAction === 'spawn_niagara') {
    requireNonEmptyString(args.systemPath, 'systemPath', 'Missing required parameter: systemPath');
  }

  if (subAction === 'activate_niagara' || subAction === 'deactivate_niagara' || subAction === 'advance_simulation') {
    const systemName = args.systemName ?? args.actorName;
    requireNonEmptyString(systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    args.systemName = systemName;
  }

  if (subAction === 'set_niagara_parameter') {
    const systemName = args.systemName ?? args.actorName;
    requireNonEmptyString(systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
    // parameterType is required for unambiguous native conversion; accept common aliases.
    if (!isNonEmptyString(args.parameterType) && isNonEmptyString(args.type)) {
      args.parameterType = args.type.charAt(0).toUpperCase() + args.type.slice(1);
    }
    requireNonEmptyString(args.parameterType, 'parameterType', 'Missing required parameter: parameterType');
    args.systemName = systemName;
  }

  // Handle debug cleanup actions
  if (action === 'clear_debug_shapes') {
    return executeAutomationRequest(tools, action, args);
  }
  // Discovery action: list available debug shape types
  if (action === 'list_debug_shapes') {
    return executeAutomationRequest(tools, 'list_debug_shapes', args);
  }
  if (action === 'cleanup') {
    args.action = 'cleanup';
    args.subAction = 'cleanup';
    return executeAutomationRequest(tools, 'create_effect', args);
  }

  // Map high-level actions to create_effect with subAction
  const createActions = [
    'create_volumetric_fog',
    'create_particle_trail',
    'create_environment_effect',
    'create_impact_effect',
    'create_niagara_ribbon'
  ];
  if (createActions.includes(action)) {
    args.action = action;
    return executeAutomationRequest(tools, 'create_effect', args);
  }

  // Map simulation control actions
  if (action === 'activate' || action === 'activate_effect') {
    args.action = 'activate_niagara';
    args.systemName = args.actorName || args.systemName;
    requireNonEmptyString(args.systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    args.reset = true;
    return executeAutomationRequest(tools, 'create_effect', args);
  }
  if (action === 'deactivate') {
    args.action = 'deactivate_niagara';
    args.systemName = args.actorName || args.systemName;
    requireNonEmptyString(args.systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    return executeAutomationRequest(tools, 'create_effect', args);
  }
  if (action === 'reset') {
    args.action = 'activate_niagara';
    args.systemName = args.actorName || args.systemName;
    requireNonEmptyString(args.systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    args.reset = true;
    return executeAutomationRequest(tools, 'create_effect', args);
  }
  if (action === 'advance_simulation') {
    args.action = 'advance_simulation';
    args.systemName = args.actorName || args.systemName;
    requireNonEmptyString(args.systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    return executeAutomationRequest(tools, 'create_effect', args);
  }

  // Map parameter setting
  if (action === 'set_niagara_parameter') {
    args.action = 'set_niagara_parameter';
    // If actorName is provided, use it as systemName (which C++ expects for actor label)
    if (args.actorName && !args.systemName) {
      args.systemName = args.actorName;
    }
    // Map 'type' to 'parameterType' if provided and parameterType is missing
    if (args.type && !args.parameterType) {
      args.parameterType = args.type.charAt(0).toUpperCase() + args.type.slice(1);
    }
    requireNonEmptyString(args.systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
    requireNonEmptyString(args.parameterType, 'parameterType', 'Missing required parameter: parameterType');
    return executeAutomationRequest(tools, 'create_effect', args);
  }

  const res: any = await executeAutomationRequest(
    tools,
    'create_effect',
    args,
    'Automation bridge not available for effect creation operations'
  );

  const result = res?.result ?? res ?? {};
  const topError = typeof res?.error === 'string' ? res.error : '';
  const nestedError = typeof result.error === 'string' ? result.error : '';
  const errorCode = (topError || nestedError).toUpperCase();

  const topMessage = typeof res?.message === 'string' ? res.message : '';
  const nestedMessage = typeof result.message === 'string' ? result.message : '';
  const message = topMessage || nestedMessage || '';

  const combined = `${topError} ${nestedError} ${message}`.toLowerCase();

  if (
    action === 'niagara' &&
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
      systemPath: args.systemPath,
      handled: true
    });
  }

  // If we got here and it was a spawn_niagara failure, maybe try to be helpful about paths
  if (action === 'spawn_niagara' && errorCode === 'SYSTEM_NOT_FOUND' && args.systemPath) {
    // Check if path ends in .Name
    const path = args.systemPath;
    const name = path.split('/').pop();
    if (name && !path.endsWith(`.${name}`)) {
      // Retry with corrected path?
      // We can't easily retry here without recursion, but we can hint in the message.
      return cleanObject({
        success: false,
        error: 'SYSTEM_NOT_FOUND',
        message: `Niagara System not found at ${path}. Did you mean ${path}.${name}?`,
        systemPath: path
      });
    }
  }

  return cleanObject(res);
}
