import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { createUnknownActionResponse } from '../foundation/dispatch/handler-error-context.js';
import { createGASActionContext, sendGASRequest, validateGASRequiredFields } from './gas-action-context.js';
import { getGASActionRoute } from './gas-action-routes.js';
import { handleAddTagToAsset, handleCreateGameplayEffect } from './gas-special-actions.js';

export async function handleGASTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const context = createGASActionContext(args, tools);
  const route = getGASActionRoute(action);

  if (!route) {
    return createUnknownActionResponse(`Unknown GAS action: ${action}`);
  }

  const existingAssetPath = context.argsRecord.assetPath;
  const assetPathMissing = typeof existingAssetPath !== 'string' || existingAssetPath.trim().length === 0;
  if (action === 'get_gas_info' && assetPathMissing) {
    // get_gas_info inspects ANY GAS-relevant asset, so it takes the generic assetPath — but callers
    // coming from the blueprint/inspect tools naturally pass blueprintPath (or path). Alias them
    // instead of failing validation on a naming difference. Treat an empty/whitespace assetPath as
    // missing too, so a supplied blueprintPath still gets used.
    const alias = [context.argsRecord.blueprintPath, context.argsRecord.path]
      .find((v): v is string => typeof v === 'string' && v.trim().length > 0);
    if (alias) context.argsRecord.assetPath = alias.trim();
  }

  validateGASRequiredFields(context.argsRecord, route.requiredFields);

  if (route.kind === 'create_gameplay_effect') {
    return await handleCreateGameplayEffect(context);
  }
  if (route.kind === 'add_tag_to_asset') {
    return await handleAddTagToAsset(context);
  }

  const response = await sendGASRequest(context, action, route.blueprintPathParam);

  if (action === 'get_gas_info' && response && response.success === true) {
    // Make "no GAS here" explicit: a successful inspection of a non-GAS asset returns no gasType,
    // which reads as an incomplete answer. State it instead of leaving the field absent.
    const resultData = response.result as Record<string, unknown> | undefined;
    if (resultData && typeof resultData === 'object' && typeof resultData.gasType !== 'string') {
      try {
        resultData.gasType = 'None';
        resultData.gasNote = 'No GAS parent class detected (not a GameplayAbility/GameplayEffect/AttributeSet/GameplayCue derivative).';
      } catch {
        // Best-effort clarity only — a frozen/sealed result must not turn a successful inspection into a failure.
      }
    }
  }

  return response;
}
