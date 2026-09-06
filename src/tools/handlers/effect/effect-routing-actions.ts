import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { EffectArgs } from '../../../types/handlers/handler-types.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import {
  DEFAULT_NIAGARA_ACTOR_NAME,
  ensureDefaultNiagaraActor,
  ensureDefaultNiagaraAssets
} from './effect-handler-state.js';

const effectCreateActions = [
  'create_volumetric_fog',
  'create_particle_trail',
  'create_environment_effect',
  'create_impact_effect',
  'create_niagara_ribbon'
];

function resolveSystemName(mutableArgs: Record<string, unknown>): string {
  return (mutableArgs.effect as string | undefined) ||
    (mutableArgs.effectHandle as string | undefined) ||
    (mutableArgs.niagaraHandle as string | undefined) ||
    (mutableArgs.actorName as string | undefined) ||
    (mutableArgs.systemName as string | undefined) ||
    DEFAULT_NIAGARA_ACTOR_NAME;
}

export function applyParticlePreset(argsTyped: EffectArgs, mutableArgs: Record<string, unknown>): void {
  if (mutableArgs.action !== 'particle' && mutableArgs.subAction !== 'particle') return;

  const presets: Record<string, string> = {
    'Default': '/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit',
    'Smoke': '/StarterContent/Particles/P_Smoke.P_Smoke',
    'Fire': '/StarterContent/Particles/P_Fire.P_Fire',
    'Explosion': '/StarterContent/Particles/P_Explosion.P_Explosion',
  };
  const rawPreset = (mutableArgs.preset || argsTyped.type) as string | undefined;
  if (rawPreset) {
    if (presets[rawPreset]) {
      mutableArgs.preset = presets[rawPreset];
    } else if (rawPreset.startsWith('/')) {
      mutableArgs.preset = rawPreset;
    }
  }
  if (!mutableArgs.preset) {
    mutableArgs.preset = presets.Default;
  }
}

export function prepareDynamicLightAction(action: string, mutableArgs: Record<string, unknown>): void {
  if (action !== 'create_dynamic_light' && mutableArgs.action !== 'create_dynamic_light') return;

  if (!mutableArgs.location) {
    mutableArgs.location = { x: 0, y: 0, z: 250 };
  }
  if (!mutableArgs.lightName && mutableArgs.name) {
    mutableArgs.lightName = mutableArgs.name;
  }
}

export async function handleEffectDebugShapeAction(
  action: string,
  argsTyped: EffectArgs,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (action === 'debug_shape' || mutableArgs.action === 'debug_shape') {
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

  return undefined;
}

export async function handleEffectCleanupAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
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

  return undefined;
}

export async function handleEffectCreateAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (!effectCreateActions.includes(action)) return undefined;

  if (action !== 'create_volumetric_fog' && !mutableArgs.systemPath) {
    const defaultAssets = await ensureDefaultNiagaraAssets(tools);
    mutableArgs.systemPath = defaultAssets.systemPath;
  }
  mutableArgs.action = action;
  mutableArgs.subAction = action;
  return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
}

export async function handleEffectSimulationAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (action === 'activate' || action === 'activate_effect' || action === 'reset') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'activate_niagara';
    mutableArgs.subAction = 'activate_niagara';
    mutableArgs.systemName = resolveSystemName(mutableArgs);
    if (mutableArgs.reset === undefined) {
      mutableArgs.reset = true;
    }
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'deactivate') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'deactivate_niagara';
    mutableArgs.subAction = 'deactivate_niagara';
    mutableArgs.systemName = resolveSystemName(mutableArgs);
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }
  if (action === 'advance_simulation') {
    await ensureDefaultNiagaraActor(tools);
    mutableArgs.action = 'advance_simulation';
    mutableArgs.subAction = 'advance_simulation';
    mutableArgs.systemName = resolveSystemName(mutableArgs);
    return executeAutomationRequest(tools, 'create_effect', mutableArgs) as Promise<Record<string, unknown>>;
  }

  return undefined;
}

export async function handleEffectParameterAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (action !== 'set_niagara_parameter') return undefined;

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

export async function handleEffectNiagaraSpawnAction(
  action: string,
  mutableArgs: Record<string, unknown>,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (action !== 'niagara' && action !== 'spawn_niagara') return undefined;

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
