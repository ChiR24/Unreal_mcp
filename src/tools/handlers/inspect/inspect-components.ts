import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { ComponentInfo, HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { extractString, normalizeArgs, resolveObjectPath } from '../foundation/arguments/argument-helper.js';
import type { InspectHandlerContext, InspectResponse } from './inspect-actions.js';

export function toComponentInfo(value: unknown): ComponentInfo | undefined {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return undefined;
  }

  const record = value as Record<string, unknown>;
  const component: ComponentInfo = { name: typeof record.name === 'string' ? record.name : '' };
  Object.assign(component, record);
  component.name = typeof record.name === 'string' ? record.name : '';
  return component;
}

export function toComponentList(value: unknown): ComponentInfo[] {
  if (!Array.isArray(value)) {
    return [];
  }
  return value
    .map(toComponentInfo)
    .filter((component): component is ComponentInfo => component !== undefined);
}

function isComponentPath(value: string): boolean {
  return !value.includes('/') &&
    !value.includes('\\') &&
    value.includes('.') &&
    value.split('.').length === 2;
}

function findComponentMatch(components: ComponentInfo[], needle: string): ComponentInfo | undefined {
  return components.find(component => String(component?.name || '').toLowerCase() === needle)
    ?? components.find(component => String(component?.objectPath || '').toLowerCase() === needle)
    ?? components.find(component => String(component?.path || '').toLowerCase() === needle)
    ?? components.find(component => String(component?.objectPath || '').toLowerCase().endsWith(`:${needle}`))
    ?? components.find(component => String(component?.objectPath || '').toLowerCase().endsWith(`.${needle}`))
    ?? components.find(component => String(component?.path || '').toLowerCase().endsWith(`:${needle}`))
    ?? components.find(component => String(component?.path || '').toLowerCase().endsWith(`.${needle}`))
    ?? components.find(component => String(component?.name || '').toLowerCase().startsWith(needle));
}

export async function resolveComponentObjectPathFromArgs(args: HandlerArgs, tools: ITools): Promise<string> {
  const componentName = typeof args.componentName === 'string' ? args.componentName.trim() : '';
  const componentPath = typeof args.componentPath === 'string' ? args.componentPath.trim() : '';
  const direct = componentPath || (
    (componentName.includes(':') || componentName.includes('.')) &&
      (componentName.startsWith('/Game') || componentName.startsWith('/Script') || componentName.startsWith('/Engine'))
      ? componentName
      : ''
  );
  if (direct) return direct;

  const rawObjectPath = typeof args.objectPath === 'string' ? args.objectPath.trim() : '';
  const objectPathLooksLikeComponent = rawObjectPath && isComponentPath(rawObjectPath);
  let actorName: string | undefined;
  let effectiveComponentName = componentName;

  if (objectPathLooksLikeComponent && !componentName) {
    const parts = rawObjectPath.split('.');
    actorName = parts[0];
    effectiveComponentName = parts[1];
  } else {
    actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  }

  if (!actorName) {
    throw new Error('Invalid actorName: required to resolve componentName');
  }
  if (!effectiveComponentName) {
    return actorName;
  }

  const compsRes = await executeAutomationRequest(
    tools,
    'inspect',
    { action: 'get_components', actorName, objectPath: actorName },
    'Failed to get components'
  ) as InspectResponse;

  const resultData = compsRes.result as Record<string, unknown> | undefined;
  const components = compsRes.success
    ? Array.isArray(compsRes.components)
      ? compsRes.components
      : (resultData && Array.isArray(resultData.components)) ? resultData.components as ComponentInfo[] : []
    : [];
  const match = findComponentMatch(components, effectiveComponentName.toLowerCase());

  if (match) {
    if (typeof match.objectPath === 'string' && match.objectPath.trim().length > 0) return match.objectPath.trim();
    if (typeof match.path === 'string' && match.path.trim().length > 0) return match.path.trim();
    if (typeof match.name === 'string' && match.name.trim().length > 0) return `${actorName}.${match.name}`;
  }

  return `${actorName}.${effectiveComponentName}`;
}

export async function handleGetComponents(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  if (!actorName) throw new Error('Invalid actorName');

  return cleanObject(await executeAutomationRequest(
    context.tools,
    'inspect',
    { action: 'get_components', actorName, objectPath: actorName },
    'Failed to get components'
  ) as InspectResponse);
}

export async function handleGetComponentProperty(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  const params = normalizeArgs(context.args, [
    { key: 'componentName', required: true },
    { key: 'propertyName', aliases: ['propertyPath'], required: true }
  ]);
  if (!actorName) throw new Error('Invalid actorName: required to resolve componentName');

  return cleanObject(await executeAutomationRequest(context.tools, 'control_actor', {
    action: 'get_component_property',
    actorName,
    componentName: extractString(params, 'componentName'),
    propertyName: extractString(params, 'propertyName')
  }) as Record<string, unknown>);
}

export async function handleSetComponentProperty(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  const params = normalizeArgs(context.args, [
    { key: 'componentName', required: true },
    { key: 'propertyName', aliases: ['propertyPath'], required: true },
    { key: 'value' }
  ]);
  if (!actorName) throw new Error('Invalid actorName: required to resolve componentName');

  return cleanObject(await executeAutomationRequest(context.tools, 'control_actor', {
    action: 'set_component_property',
    actorName,
    componentName: extractString(params, 'componentName'),
    properties: {
      [extractString(params, 'propertyName')]: params.value
    }
  }) as Record<string, unknown>);
}

export async function handleGetComponentDetails(context: InspectHandlerContext): Promise<Record<string, unknown>> {
  const actorName = await resolveObjectPath(context.args, context.tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  const params = normalizeArgs(context.args, [{ key: 'componentName', required: true }]);
  if (!actorName) {
    // Blueprint/CDO shape: callers coming from get_blueprint_details naturally pass a blueprintPath here,
    // but this action inspects WORLD actors. Instead of failing, delegate to inspect_cdo (which reads a
    // Blueprint's default components without spawning an actor) filtered to the requested component.
    const blueprintPath = context.inspectArgs.blueprintPath
      || (typeof context.normalizedArgs.blueprintPath === 'string' ? context.normalizedArgs.blueprintPath : '')
      || (typeof context.normalizedArgs.assetPath === 'string' ? context.normalizedArgs.assetPath : '');
    if (blueprintPath) {
      const cdoResult = await executeAutomationRequest(context.tools, 'inspect', {
        ...context.normalizedArgs,
        action: 'inspect_cdo',
        blueprintPath,
        componentName: extractString(params, 'componentName')
      }) as Record<string, unknown>;
      return cleanObject({
        ...cdoResult,
        note: 'get_component_details inspects world actors; the blueprintPath argument was routed to inspect_cdo (Blueprint default/CDO components).'
      }) as Record<string, unknown>;
    }
    throw new Error(
      'Invalid actorName: get_component_details inspects WORLD actors (pass actorName). ' +
      'For a Blueprint\'s default components, pass blueprintPath (routed to inspect_cdo) or call inspect_cdo with componentName.'
    );
  }

  const componentName = extractString(params, 'componentName');
  const res = await executeAutomationRequest(
    context.tools,
    'control_actor',
    { action: 'get_components', actorName },
    'Failed to get component details'
  ) as InspectResponse;
  const resultData = res.result as Record<string, unknown> | undefined;
  const components = Array.isArray(res.components)
    ? res.components
    : (resultData && Array.isArray(resultData.components)) ? resultData.components as ComponentInfo[] : [];
  const component = components.find(item => String(item.name || '').toLowerCase() === componentName.toLowerCase());
  if (!component) throw new Error(`Component not found: ${componentName}`);

  return cleanObject({ success: true, actorName, componentName, component });
}
