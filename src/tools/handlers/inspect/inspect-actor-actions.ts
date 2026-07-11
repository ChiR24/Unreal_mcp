import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { extractOptionalString, extractString, normalizeArgs, resolveObjectPath } from '../foundation/arguments/argument-helper.js';
import type { InspectHandlerContext, InspectResponse } from './inspect-actions.js';

async function cleanControlActorAction(
  context: InspectHandlerContext,
  payload: Record<string, unknown>
): Promise<Record<string, unknown>> {
  return cleanObject(await executeAutomationRequest(context.tools, 'control_actor', payload) as Record<string, unknown>);
}

function assetPathGuidance(actorName: string | undefined): string | undefined {
  // delete_object deletes WORLD actors by name; a content path (/Game/..., /Engine/...) is an ASSET
  // reference and will never match an actor. Point the caller at the right tool instead of leaving an
  // ambiguous NOT_FOUND (no silent cross-routing: deletion is destructive, the caller must re-target).
  // The ':' subobject delimiter marks a world OBJECT path (/Game/Map.Map:PersistentLevel.Actor_1) —
  // a legitimate actor reference, not an asset — so it must not get the asset hint.
  if (actorName && /^\/(Game|Engine)\//i.test(actorName) && !actorName.includes(':')) {
    return 'delete_object deletes WORLD actors by name; this looks like an ASSET path. To delete an asset, use manage_asset with action:delete.';
  }
  return undefined;
}

function handledNotFoundResponse(res: InspectResponse, actorName: string): Record<string, unknown> | undefined {
  if (res.success !== false) return undefined;
  const message = String(res.message || res.error || '');
  const lower = message.toLowerCase();
  if (lower.includes('actor not found') || lower.includes('actors not found') || lower.includes('not found')) {
    return cleanObject({
      success: false,
      error: res.error || 'NOT_FOUND',
      handled: true,
      message,
      deleted: actorName,
      notFound: true,
      hint: assetPathGuidance(actorName)
    });
  }
  return cleanObject({ ...res, handled: true, notFound: lower.includes('not found') });
}

async function handleDeleteObject(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools);
  try {
    if (!actorName) throw new Error('actorName is required for delete_object');
    const res = await executeAutomationRequest(context.tools, 'control_actor', {
      action: 'delete',
      actorName
    }) as InspectResponse;
    return handledNotFoundResponse(res, actorName) ?? cleanObject(res);
  } catch (error: unknown) {
    const message = String(error instanceof Error ? error.message : error);
    const lower = message.toLowerCase();
    if (lower.includes('actor not found') || lower.includes('actors not found') || lower.includes('not found')) {
      return cleanObject({
        success: false,
        error: 'NOT_FOUND',
        handled: true,
        message,
        deleted: actorName,
        notFound: true,
        hint: assetPathGuidance(actorName)
      });
    }
    throw error;
  }
}

async function handleBoundingBox(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools);
  try {
    if (!actorName) throw new Error('actorName is required for get_bounding_box');
    return cleanObject(await executeAutomationRequest(context.tools, 'control_actor', {
      action: 'get_bounding_box',
      actorName
    }) as Record<string, unknown>);
  } catch (error: unknown) {
    const message = String(error instanceof Error ? error.message : error);
    if (message.toLowerCase().includes('actor not found')) {
      return cleanObject({
        success: false,
        error: 'NOT_FOUND',
        handled: true,
        message,
        actorName,
        notFound: true
      });
    }
    throw error;
  }
}

export async function handleActorInspectAction(
  action: string,
  context: InspectHandlerContext
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
    case 'get_metadata': {
      const actorName = await resolveObjectPath(context.args, context.tools);
      if (!actorName) throw new Error('Invalid actorName');
      return cleanControlActorAction(context, { action: 'get_metadata', actorName });
    }
    case 'add_tag': {
      const actorName = await resolveObjectPath(context.args, context.tools);
      const tag = extractString(normalizeArgs(context.args, [{ key: 'tag', required: true }]), 'tag');
      if (!actorName) throw new Error('Invalid actorName');
      return cleanControlActorAction(context, { action: 'add_tag', actorName, tag });
    }
    case 'find_by_tag': {
      const tag = extractOptionalString(normalizeArgs(context.args, [{ key: 'tag' }]), 'tag') ?? '';
      return cleanControlActorAction(context, { action: 'find_by_tag', tag });
    }
    case 'create_snapshot': {
      const actorName = await resolveObjectPath(context.args, context.tools);
      if (!actorName) throw new Error('actorName is required for create_snapshot');
      const snapshotName = typeof context.inspectArgs.snapshotName === 'string' ? context.inspectArgs.snapshotName : '';
      return cleanControlActorAction(context, { action: 'create_snapshot', actorName, snapshotName });
    }
    case 'restore_snapshot': {
      const actorName = await resolveObjectPath(context.args, context.tools);
      if (!actorName) throw new Error('actorName is required for restore_snapshot');
      const snapshotName = typeof context.inspectArgs.snapshotName === 'string' ? context.inspectArgs.snapshotName : '';
      return cleanControlActorAction(context, { action: 'restore_snapshot', actorName, snapshotName });
    }
    case 'export': {
      const actorName = await resolveObjectPath(context.args, context.tools);
      if (!actorName) throw new Error('actorName may be required for export depending on context (exporting actor requires it)');
      return cleanControlActorAction(context, { action: 'export', actorName: actorName || '' });
    }
    case 'delete_object':
      return handleDeleteObject(context);
    case 'list_objects':
      return cleanControlActorAction(context, { action: 'list_actors', ...context.args });
    case 'find_by_class': {
      const className = extractString(normalizeArgs(context.args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]), 'className');
      const res = await executeAutomationRequest(context.tools, 'inspect', {
        action: 'find_by_class',
        className
      }) as InspectResponse;
      if (!res || res.success === false) {
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || 'find_by_class failed',
          className,
          objects: [],
          count: 0
        });
      }
      return cleanObject(res);
    }
    case 'get_bounding_box':
      return handleBoundingBox(context);
    default:
      return undefined;
  }
}
