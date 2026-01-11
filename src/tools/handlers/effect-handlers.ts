import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, EffectArgs, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

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

function isNonEmptyString(val: unknown): val is string {
  return typeof val === 'string' && val.trim().length > 0;
}

/** Response from automation request */
interface AutomationResponse {
  success?: boolean;
  error?: string;
  message?: string;
  result?: {
    error?: string;
    message?: string;
    [key: string]: unknown;
  };
  [key: string]: unknown;
}

export async function handleEffectTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as EffectArgs;
  const mutableArgs = { ...args } as Record<string, unknown>;
  
  if (!mutableArgs || typeof mutableArgs !== 'object') {
    // Create empty object
  }

  // Always ensure action/subAction are present before any routing.
  ensureActionAndSubAction(action, mutableArgs);

  // Handle creation actions explicitly to use NiagaraTools helper
  if (action === 'create_niagara_system') {
    const res = await tools.niagaraTools.createSystem({
      name: argsTyped.name,
      savePath: (mutableArgs.savePath as string | undefined),
      template: (mutableArgs.template as string | undefined)
    });
    return cleanObject(res) as Record<string, unknown>;
  }
  if (action === 'create_niagara_emitter') {
    const res = await tools.niagaraTools.createEmitter({
      name: argsTyped.name,
      savePath: (mutableArgs.savePath as string | undefined),
      systemPath: argsTyped.systemPath,
      template: (mutableArgs.template as string | undefined)
    });
    return cleanObject(res) as Record<string, unknown>;
  }

  // Pre-process arguments for particle presets
  if (mutableArgs.action === 'particle') {
    const presets: Record<string, string> = {
      'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
      'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
      'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
      'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
    };
    // Check both preset and effectType fields
    const key = (argsTyped.preset || argsTyped.type) as string | undefined;
    if (key && presets[key]) {
      mutableArgs.preset = presets[key];
    }
  }

  // Handle debug shapes (must happen before any automation request)
  if (action === 'debug_shape' || mutableArgs.action === 'debug_shape') {
    // Map 'shape' to 'shapeType' if provided (schema uses 'shape', C++ uses 'shapeType')
    if (argsTyped.shape && !mutableArgs.shapeType) {
      mutableArgs.shapeType = argsTyped.shape;
    }
    requireNonEmptyString(mutableArgs.shapeType as string | undefined, 'shapeType', 'Missing required parameter: shapeType');
    mutableArgs.action = 'debug_shape';
    mutableArgs.subAction = 'debug_shape';
    return cleanObject(await executeAutomationRequest(tools, 'create_effect', mutableArgs)) as Record<string, unknown>;
  }

  // Validate Niagara-related required parameters (keep errors explicit and early)
  const subAction = String(mutableArgs.subAction || '').trim();
  if (subAction === 'niagara' || subAction === 'spawn_niagara') {
    requireNonEmptyString(argsTyped.systemPath, 'systemPath', 'Missing required parameter: systemPath');
  }

  if (subAction === 'activate_niagara' || subAction === 'deactivate_niagara' || subAction === 'advance_simulation') {
    const systemName = (mutableArgs.systemName as string | undefined) ?? (mutableArgs.actorName as string | undefined);
    requireNonEmptyString(systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    mutableArgs.systemName = systemName;
  }

  if (subAction === 'set_niagara_parameter') {
    const systemName = (mutableArgs.systemName as string | undefined) ?? (mutableArgs.actorName as string | undefined);
    requireNonEmptyString(systemName, 'systemName', 'Missing required parameter: systemName (or actorName)');
    requireNonEmptyString(argsTyped.parameterName, 'parameterName', 'Missing required parameter: parameterName');
    // parameterType is required for unambiguous native conversion; accept common aliases.
    if (!isNonEmptyString(argsTyped.parameterType) && isNonEmptyString(argsTyped.type)) {
      mutableArgs.parameterType = argsTyped.type.charAt(0).toUpperCase() + argsTyped.type.slice(1);
    }
    requireNonEmptyString(mutableArgs.parameterType as string | undefined, 'parameterType', 'Missing required parameter: parameterType');
    mutableArgs.systemName = systemName;
  }

  // Handle debug cleanup actions
  if (action === 'clear_debug_shapes') {
    return executeAutomationRequest(tools, action, mutableArgs) as Promise<HandlerResult>;
  }
  // Discovery action: list available debug shape types
  if (action === 'list_debug_shapes') {
    return executeAutomationRequest(tools, 'list_debug_shapes', mutableArgs) as Promise<HandlerResult>;
  }
  if (action === 'cleanup') {
    mutableArgs.action = 'cleanup';
    mutableArgs.subAction = 'cleanup';
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
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
    mutableArgs.action = action;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }

  // Map simulation control actions
  if (action === 'activate' || action === 'activate_effect') {
    mutableArgs.action = 'activate_niagara';
    mutableArgs.systemName = (mutableArgs.actorName as string | undefined) || (mutableArgs.systemName as string | undefined);
    requireNonEmptyString(mutableArgs.systemName as string | undefined, 'systemName', 'Missing required parameter: systemName (or actorName)');
    mutableArgs.reset = true;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }
  if (action === 'deactivate') {
    mutableArgs.action = 'deactivate_niagara';
    mutableArgs.systemName = (mutableArgs.actorName as string | undefined) || (mutableArgs.systemName as string | undefined);
    requireNonEmptyString(mutableArgs.systemName as string | undefined, 'systemName', 'Missing required parameter: systemName (or actorName)');
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }
  if (action === 'reset') {
    mutableArgs.action = 'activate_niagara';
    mutableArgs.systemName = (mutableArgs.actorName as string | undefined) || (mutableArgs.systemName as string | undefined);
    requireNonEmptyString(mutableArgs.systemName as string | undefined, 'systemName', 'Missing required parameter: systemName (or actorName)');
    mutableArgs.reset = true;
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }
  if (action === 'advance_simulation') {
    mutableArgs.action = 'advance_simulation';
    mutableArgs.systemName = (mutableArgs.actorName as string | undefined) || (mutableArgs.systemName as string | undefined);
    requireNonEmptyString(mutableArgs.systemName as string | undefined, 'systemName', 'Missing required parameter: systemName (or actorName)');
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }

  // Map parameter setting
  if (action === 'set_niagara_parameter') {
    mutableArgs.action = 'set_niagara_parameter';
    // If actorName is provided, use it as systemName (which C++ expects for actor label)
    if ((mutableArgs.actorName as string | undefined) && !(mutableArgs.systemName as string | undefined)) {
      mutableArgs.systemName = mutableArgs.actorName;
    }
    // Map 'type' to 'parameterType' if provided and parameterType is missing
    const typeVal = mutableArgs.type as string | undefined;
    if (typeVal && !(mutableArgs.parameterType as string | undefined)) {
      mutableArgs.parameterType = typeVal.charAt(0).toUpperCase() + typeVal.slice(1);
    }
    requireNonEmptyString(mutableArgs.systemName as string | undefined, 'systemName', 'Missing required parameter: systemName (or actorName)');
    requireNonEmptyString(mutableArgs.parameterName as string | undefined, 'parameterName', 'Missing required parameter: parameterName');
    requireNonEmptyString(mutableArgs.parameterType as string | undefined, 'parameterType', 'Missing required parameter: parameterType');
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<HandlerResult>;
  }

  const res = await executeAutomationRequest(
    tools,
    'create_effect',
    mutableArgs,
    'Automation bridge not available for effect creation operations'
  ) as AutomationResponse;

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
      systemPath: argsTyped.systemPath,
      handled: true
    });
  }

  // If we got here and it was a spawn_niagara failure, maybe try to be helpful about paths
  if (action === 'spawn_niagara' && errorCode === 'SYSTEM_NOT_FOUND' && argsTyped.systemPath) {
    // Check if path ends in .Name
    const path = argsTyped.systemPath;
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

  return cleanObject(res) as Record<string, unknown>;
}
