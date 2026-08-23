import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import {
  blueprintCandidates,
  blueprintTarget,
  commonTimingPayload,
  executeBlueprintRequest,
  firstString,
  optionalBoolean,
  optionalNumber,
  optionalString
} from './blueprint-action-context.js';
import type { BlueprintActionContext, BlueprintActionHandler } from './blueprint-action-context.js';
import { handleBlueprintGetAction } from './blueprint-query-actions.js';

export const blueprintCoreHandlers: Readonly<Record<string, BlueprintActionHandler>> = {
  create: async (context) => await handleCreate(context),
  ensure_exists: async (context) => await executeBlueprintRequest(context, 'blueprint_exists', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs),
    shouldExist: context.argsTyped.shouldExist !== false
  }),
  set_metadata: async (context) => await handleSetMetadata(context),
  set_default: async (context) => await executeBlueprintRequest(context, 'blueprint_set_default', {
    blueprintCandidates: blueprintCandidates(context),
    requestedPath: blueprintTarget(context),
    // set_default targets a Blueprint VARIABLE's default, so callers naturally say variableName —
    // alias it instead of failing the C++ "propertyName required" check on a naming difference.
    propertyName: context.argsTyped.propertyName
      ?? optionalString(context.argsRecord.variableName)
      ?? '',
    value: context.argsTyped.value !== undefined ? context.argsTyped.value : context.argsRecord.propertyValue
  }),
  compile: async (context) => await executeBlueprintRequest(context, 'blueprint_compile', {
    requestedPath: blueprintTarget(context),
    saveAfterCompile: optionalBoolean(context.argsRecord.saveAfterCompile)
  }),
  probe_handle: async (context) => {
    const result = await executeBlueprintRequest(context, 'blueprint_probe_handle', {
      requestedPath: blueprintTarget(context),
      componentClass: optionalString(context.argsRecord.componentClass) ?? '',
      operations: context.argsRecord.operations,
      timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
    });
    if (result.reachable === undefined && result.exists !== undefined) {
      result.reachable = result.exists;
    }
    return result;
  },
  get: async (context) => await handleBlueprintGetAction(context)
};

export async function handleBlueprintDefault(
  context: BlueprintActionContext,
  originalArgs: HandlerArgs
): Promise<Record<string, unknown>> {
  const processedArgs: HandlerArgs = { ...originalArgs };
  if (context.argsRecord.action === 'modify_scs' && context.argsRecord.applyAndSave === true) {
    processedArgs.compile = true;
    processedArgs.save = true;
  }

  return await executeBlueprintRequest(
    context,
    'manage_blueprint',
    processedArgs,
    'Automation bridge not available for blueprint operations'
  );
}

async function handleCreate(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  const pathArg = firstString(context.argsRecord.path, context.argsTyped.blueprintPath);
  let name = context.argsTyped.name;
  let savePath = context.argsTyped.savePath;

  if (pathArg) {
    if (name) {
      savePath = pathArg;
    } else {
      const parts = pathArg.split('/');
      name = parts.pop();
      savePath = parts.join('/');
    }
  }

  const resolvedSavePath = savePath || '/Game';
  if (!name || typeof name !== 'string' || name.trim() === '') {
    throw new Error('Missing or invalid required parameter: name (must be a non-empty string for create action)');
  }

  return await executeBlueprintRequest(context, 'blueprint_create', {
    name,
    blueprintType: context.argsTyped.blueprintType,
    savePath: resolvedSavePath,
    parentClass: optionalString(context.argsRecord.parentClass),
    properties: context.argsTyped.properties,
    ...commonTimingPayload(context)
  });
}

async function handleSetMetadata(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  const assetPath = resolveMetadataAssetPath(context);
  if (!assetPath) {
    throw new Error('Invalid parameters: assetPath or blueprintPath or name+savePath required for set_metadata');
  }

  const metadata = context.argsTyped.metadata && typeof context.argsTyped.metadata === 'object'
    ? context.argsTyped.metadata
    : {};
  return await executeBlueprintRequest(context, 'set_metadata', {
    ...context.argsRecord,
    assetPath,
    metadata
  });
}

function resolveMetadataAssetPath(context: BlueprintActionContext): string {
  const assetPath = trimmedString(context.argsRecord.assetPath);
  if (assetPath) {
    return assetPath;
  }

  const blueprintPath = trimmedString(context.argsTyped.blueprintPath);
  if (blueprintPath) {
    return blueprintPath;
  }

  const name = trimmedString(context.argsTyped.name);
  const savePath = trimmedString(context.argsTyped.savePath);
  if (!name || !savePath) {
    return '';
  }

  return `${savePath.replace(/\/$/, '')}/${name}`;
}

function trimmedString(value: unknown): string {
  return typeof value === 'string' ? value.trim() : '';
}
