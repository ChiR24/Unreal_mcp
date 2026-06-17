import {
  blueprintCandidates,
  blueprintTarget,
  cleanRecord,
  commonTimingPayload,
  executeBlueprintRequest,
  firstString,
  optionalNumber,
  optionalString
} from './blueprint-action-context.js';
import type { BlueprintActionContext, BlueprintActionHandler } from './blueprint-action-context.js';

export const blueprintEventHandlers: Readonly<Record<string, BlueprintActionHandler>> = {
  add_event: async (context) => await handleAddEvent(context),
  remove_event: async (context) => await executeBlueprintRequest(context, 'blueprint_remove_event', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    eventName: optionalString(context.argsRecord.eventName) ?? '',
    customEventName: optionalString(context.argsRecord.customEventName),
    ...commonTimingPayload(context)
  }),
  add_function: async (context) => await handleAddFunction(context),
  remove_function: async (context) => {
    // Mirror add_function's path/name disambiguation. blueprintTarget() is name-first,
    // so with both { blueprintPath, name } it would treat the function name as the
    // Blueprint path. Resolve the path blueprintPath-first, and only fall back to `name`
    // for the function name when it was NOT consumed as the Blueprint path.
    const blueprintName = firstString(context.argsTyped.blueprintPath, context.argsRecord.path, context.argsTyped.name) ?? '';
    const usedNameForBlueprint = !context.argsTyped.blueprintPath && !context.argsRecord.path && Boolean(context.argsTyped.name);
    return await executeBlueprintRequest(context, 'blueprint_remove_function', {
      blueprintCandidates: [blueprintName],
      requestedPath: blueprintName,
      functionName: optionalString(context.argsRecord.functionName) || optionalString(context.argsTyped.memberName) || (!usedNameForBlueprint ? optionalString(context.argsTyped.name) : undefined) || '',
      ...commonTimingPayload(context)
    });
  },
  add_construction_script: async (context) => await executeBlueprintRequest(context, 'blueprint_add_construction_script', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    scriptName: optionalString(context.argsRecord.scriptName) ?? '',
    ...commonTimingPayload(context)
  })
};

async function handleAddEvent(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  const blueprintName = firstString(context.argsTyped.blueprintPath, context.argsRecord.path, context.argsTyped.name) ?? '';
  const usedNameForBlueprint = !context.argsTyped.blueprintPath && !context.argsRecord.path && Boolean(context.argsTyped.name);
  const result = await executeBlueprintRequest(context, 'blueprint_add_event', {
    blueprintCandidates: [blueprintName],
    requestedPath: blueprintName,
    eventType: context.argsTyped.eventType ?? 'Custom',
    customEventName: optionalString(context.argsRecord.customEventName) || (!usedNameForBlueprint ? context.argsTyped.name : undefined),
    parameters: context.argsRecord.parameters,
    posX: optionalNumber(context.argsRecord.posX),
    posY: optionalNumber(context.argsRecord.posY),
    x: optionalNumber(context.argsRecord.x),
    y: optionalNumber(context.argsRecord.y),
    ...commonTimingPayload(context)
  });

  if (result.success === false) {
    const message = typeof result.message === 'string' ? result.message.toLowerCase() : '';
    if (message.includes('already exists') || message.includes('duplicate')) {
      return cleanRecord({
        success: false,
        error: 'EVENT_ALREADY_EXISTS',
        message: result.message || 'Event already exists',
        blueprintName
      });
    }
  }

  return result;
}

async function handleAddFunction(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  const blueprintName = firstString(context.argsTyped.blueprintPath, context.argsRecord.path, context.argsTyped.name) ?? '';
  const usedNameForBlueprint = !context.argsTyped.blueprintPath && !context.argsRecord.path && Boolean(context.argsTyped.name);
  return await executeBlueprintRequest(context, 'blueprint_add_function', {
    blueprintCandidates: [blueprintName],
    requestedPath: blueprintName,
    functionName: optionalString(context.argsRecord.functionName) || context.argsTyped.memberName || (!usedNameForBlueprint ? context.argsTyped.name : undefined) || 'NewFunction',
    inputs: context.argsRecord.inputs,
    outputs: context.argsRecord.outputs,
    isPublic: context.argsRecord.isPublic,
    category: optionalString(context.argsRecord.category),
    ...commonTimingPayload(context)
  });
}
