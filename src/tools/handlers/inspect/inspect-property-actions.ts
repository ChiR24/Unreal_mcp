import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { extractOptionalString, extractString, normalizeArgs, resolveObjectPath } from '../foundation/arguments/argument-helper.js';
import type { InspectHandlerContext, InspectResponse } from './inspect-actions.js';
import { toComponentList } from './inspect-components.js';
import type { ComponentInfo } from '../../../types/handlers/handler-types.js';

function normalizedBlueprintPath(params: Record<string, unknown>): string | undefined {
  return extractOptionalString(params, 'blueprintPath')?.trim().replace(/\/+$/, '') || undefined;
}

async function tryRootComponentProperty(
  context: InspectHandlerContext,
  actorName: string,
  propertyName: string
): Promise<Record<string, unknown> | undefined> {
  try {
    const rootRes = await executeAutomationRequest(context.tools, 'inspect', {
      action: 'get_property',
      objectPath: actorName,
      propertyName: 'RootComponent'
    }) as InspectResponse;
    const rootValue = rootRes.value as Record<string, unknown> | string | undefined;
    const rootPath = typeof rootValue === 'string'
      ? rootValue
      : (typeof rootValue === 'object' && rootValue ? (rootValue.path || rootValue.objectPath) as string : undefined);

    if (rootRes.success && rootPath && typeof rootPath === 'string' && rootPath.length > 0 && rootPath !== 'None') {
      const propRes = await executeAutomationRequest(context.tools, 'inspect', {
        action: 'get_property',
        objectPath: rootPath,
        propertyName
      }) as InspectResponse;
      if (propRes.success) {
        return cleanObject({
          ...propRes,
          message: `Resolved property '${propertyName}' on RootComponent (Smart Lookup)`,
          foundOnComponent: 'RootComponent'
        });
      }
    }
  } catch (error) {
    if (!(error instanceof Error)) {
      throw error;
    }
  }
  return undefined;
}

async function tryComponentProperties(
  context: InspectHandlerContext,
  res: InspectResponse,
  actorName: string,
  propertyName: string
): Promise<Record<string, unknown>> {
  try {
    const shortName = String(context.inspectArgs.objectPath || '').trim();
    const compsRes = await executeAutomationRequest(context.tools, 'inspect', {
      action: 'get_components',
      actorName: shortName,
      objectPath: shortName
    }) as InspectResponse;

    if (compsRes.success && (Array.isArray(compsRes.components) || Array.isArray(compsRes))) {
      const list: ComponentInfo[] = Array.isArray(compsRes.components)
        ? toComponentList(compsRes.components)
        : toComponentList(compsRes);
      const triedPaths: string[] = [];
      for (const comp of list) {
        const compPath = comp.objectPath || (comp.name ? `${actorName}.${comp.name}` : undefined);
        if (!compPath) continue;
        triedPaths.push(compPath);

        const compRes = await executeAutomationRequest(context.tools, 'inspect', {
          action: 'get_property',
          objectPath: compPath,
          propertyName
        }) as InspectResponse;
        if (compRes.success) {
          return cleanObject({
            ...compRes,
            message: `Resolved property '${propertyName}' on component '${comp.name}' (Smart Lookup)`,
            foundOnComponent: comp.name
          });
        }
      }

      return cleanObject({
        ...res,
        message: `${res.message as string} (Smart Lookup failed. Tried: ${triedPaths.length} paths. First: ${triedPaths[0]}. Components: ${list.map(component => component.name).join(',')})`,
        smartLookupTriedPaths: triedPaths
      });
    }

    // Surface *why* the component sweep could not run, but never the raw
    // automation envelope: it carries transport internals (type,
    // requestId) that mean nothing to the caller and cannot be acted on.
    const lookupReason = compsRes.success
      ? 'returned success but no component list'
      : `failed: ${typeof compsRes.error === 'string' ? compsRes.error : JSON.stringify(compsRes.error) || 'unknown error'}${
        compsRes.message ? ` (${String(compsRes.message)})` : ''}`;

    return cleanObject({
      ...res,
      message: `${res.message as string} (Smart Lookup could not enumerate components: get_components ${lookupReason}. Looked for '${shortName}' under '${actorName}'. Verify the actor/Blueprint path, then retry with an explicit component path.)`
    });
  } catch (error: unknown) {
    const errorMsg = error instanceof Error ? error.message : String(error);
    return cleanObject({
      ...res,
      message: `${res.message as string} (Smart Lookup exception: ${errorMsg})`,
      error: res.error
    });
  }
}

async function smartLookupProperty(
  context: InspectHandlerContext,
  res: InspectResponse,
  propertyName: string
): Promise<Record<string, unknown> | undefined> {
  if (res.success || (res.error !== 'PROPERTY_NOT_FOUND' && !String(res.error).includes('not found'))) {
    return undefined;
  }

  const actorName = await resolveObjectPath(context.args, context.tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  if (!actorName) {
    return undefined;
  }

  return await tryRootComponentProperty(context, actorName, propertyName)
    ?? tryComponentProperties(context, res, actorName, propertyName);
}

export async function handleGetProperty(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const objectPath = await resolveObjectPath(context.args, context.tools);
  const params = normalizeArgs(context.args, [
    { key: 'blueprintPath', aliases: ['blueprint_path'] },
    { key: 'propertyName', aliases: ['propertyPath'], required: true }
  ]);
  const blueprintPath = normalizedBlueprintPath(params);
  const propertyName = extractString(params, 'propertyName');

  if (!objectPath && !blueprintPath) {
    throw new Error('inspect:get_property: Either objectPath or blueprintPath is required');
  }

  const payload: Record<string, unknown> = { ...context.args, action: 'get_property', propertyName };
  if (blueprintPath) payload.blueprintPath = blueprintPath;
  if (objectPath) payload.objectPath = objectPath;

  const res = await executeAutomationRequest(context.tools, 'inspect', payload) as InspectResponse;
  return await smartLookupProperty(context, res, propertyName) ?? cleanObject(res);
}

export async function handleSetProperty(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const objectPath = await resolveObjectPath(context.args, context.tools);
  const params = normalizeArgs(context.args, [
    { key: 'blueprintPath', aliases: ['blueprint_path'] },
    { key: 'propertyName', aliases: ['propertyPath'], required: true },
    { key: 'value' }
  ]);
  const blueprintPath = normalizedBlueprintPath(params);
  const propertyName = extractString(params, 'propertyName');

  if (!objectPath && !blueprintPath) {
    throw new Error('inspect:set_property: Either objectPath or blueprintPath is required');
  }

  const payload: Record<string, unknown> = { action: 'set_property', propertyName, value: params.value };
  if (blueprintPath) payload.blueprintPath = blueprintPath;
  if (objectPath) payload.objectPath = objectPath;

  const res = await executeAutomationRequest(context.tools, 'inspect', payload) as InspectResponse;
  if (res.success === false && String(res.error || '').toUpperCase() === 'PROPERTY_NOT_FOUND') {
    return cleanObject({ ...res, error: 'UNKNOWN_PROPERTY' });
  }
  return cleanObject(res);
}
