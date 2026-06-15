import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { extractString, normalizeArgs } from '../foundation/arguments/argument-helper.js';
import type { InspectHandlerContext, InspectResponse } from './inspect-actions.js';

const GLOBAL_INSPECT_ACTIONS = new Set([
  'get_project_settings',
  'get_editor_settings',
  'get_performance_stats',
  'get_memory_stats',
  'get_scene_stats',
  'get_viewport_info',
  'get_content_browser_state',
  'get_selected_actors'
]);

function resolveClassName(className: string): string {
  if (className && !className.includes('/') && !className.includes('.')) {
    if (className === 'Landscape') return '/Script/Landscape.Landscape';
    if (['Actor', 'Pawn', 'Character', 'StaticMeshActor'].includes(className)) {
      return `/Script/Engine.${className}`;
    }
  }
  return className;
}

export async function handleRuntimeReport(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  return cleanObject(await executeAutomationRequest(context.tools, 'inspect', {
    action: context.originalAction === 'pie_report' ? 'pie_report' : 'runtime_report',
    filter: context.inspectArgs.filter,
    actorName: context.inspectArgs.actorName || context.inspectArgs.name,
    componentName: context.inspectArgs.componentName,
    componentNames: context.inspectArgs.componentNames,
    propertyName: context.inspectArgs.propertyName || context.inspectArgs.propertyPath,
    propertyNames: context.inspectArgs.propertyNames
  }) as Record<string, unknown>);
}

export async function handleInspectClass(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'className', aliases: ['classPath'], required: true }
  ]);
  const className = resolveClassName(extractString(params, 'className'));
  const res = await executeAutomationRequest(context.tools, 'inspect', {
    action: 'inspect_class',
    className
  }) as InspectResponse;

  if (!res || res.success === false) {
    const originalClassName = typeof context.inspectArgs.className === 'string' ? context.inspectArgs.className : '';
    if (originalClassName && !originalClassName.includes('/') && !className.startsWith('/Script/')) {
      const retryName = `/Script/Engine.${originalClassName}`;
      const resRetry = await executeAutomationRequest(context.tools, 'inspect', {
        action: 'inspect_class',
        className: retryName
      }) as InspectResponse;
      if (resRetry?.success) return cleanObject(resRetry);
    }

    return cleanObject({
      success: false,
      error: res?.error || 'OPERATION_FAILED',
      message: res?.message || `inspect_class failed for '${className}'`,
      className,
      cdo: res?.cdo ?? null
    });
  }

  return cleanObject(res);
}

export async function handleInspectCdo(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  return cleanObject(await executeAutomationRequest(context.tools, 'inspect', {
    ...context.normalizedArgs,
    action: 'inspect_cdo',
    blueprintPath: context.inspectArgs.blueprintPath || context.normalizedArgs.objectPath as string
  })) as Record<string, unknown>;
}

export async function handleGlobalInspectAction(
  action: string,
  context: InspectHandlerContext
): Promise<Record<string, unknown> | undefined> {
  if (!GLOBAL_INSPECT_ACTIONS.has(action)) {
    return undefined;
  }

  return cleanObject(await executeAutomationRequest(context.tools, 'inspect', {
    action,
    ...context.normalizedArgs
  })) as Record<string, unknown>;
}
