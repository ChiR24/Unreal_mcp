import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import {
  blueprintCandidates,
  blueprintTarget,
  cleanRecord,
  executeBlueprintRequest,
  firstString,
  optionalNumber
} from './blueprint-action-context.js';
import type { BlueprintActionContext } from './blueprint-action-context.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export async function handleBlueprintGetAction(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  return await executeBlueprintRequest(context, 'blueprint_get', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  });
}

export async function handleBlueprintGet(args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const result = await executeAutomationRequest(
    tools,
    'blueprint_get',
    args,
    'Automation bridge not available for blueprint operations'
  );

  if (isRecord(result) && result.success) {
    const { success, message, error, blueprintPath: _blueprintPath, ...blueprintData } = result;
    const requestedPath = firstString(args.blueprintPath, args.path, args.name);
    return cleanRecord({
      success,
      message: typeof message === 'string' ? message : 'Blueprint fetched',
      error,
      blueprintPath: requestedPath,
      blueprint: Object.keys(blueprintData).length > 0 ? blueprintData : { path: requestedPath }
    });
  }

  return cleanRecord(result);
}
