import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { EffectArgs } from '../../../types/handlers/handler-types.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import {
  rememberLastCreatedNiagaraEmitterPath,
  rememberLastCreatedNiagaraSystemPath
} from './effect-handler-state.js';

export async function handleEffectAssetAction(
  action: string,
  argsTyped: EffectArgs,
  mutableArgs: Record<string, unknown>,
  effectiveSystemPath: string | undefined,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  if (action === 'create_niagara_system') {
    const resolvedName = (argsTyped.systemName as string | undefined)
      || (argsTyped.name as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.split('/').pop()?.replace(/\.[^.]+$/, '') : undefined)
      || 'NS_Custom';
    const resolvedSavePath = (mutableArgs.savePath as string | undefined)
      || (mutableArgs.path as string | undefined)
      || (effectiveSystemPath ? effectiveSystemPath.replace(/\/[^/]+$/, '') : '/Game/FX');
    const res = await executeAutomationRequest(tools, 'create_niagara_system', {
      name: resolvedName,
      path: resolvedSavePath,
      savePath: resolvedSavePath,
    }) as Record<string, unknown>;
    const result = (res.result ?? res) as Record<string, unknown>;
    rememberLastCreatedNiagaraSystemPath(result.systemPath);
    return cleanObject(res);
  }

  if (action === 'create_niagara_emitter') {
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
    rememberLastCreatedNiagaraEmitterPath(result.emitterPath);
    return cleanObject(res);
  }

  return undefined;
}
