import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { createInspectContext } from './inspect-actions.js';
import { handleActorInspectAction } from './inspect-actor-actions.js';
import {
  handleGetComponentDetails,
  handleGetComponentProperty,
  handleGetComponents,
  handleSetComponentProperty
} from './inspect-components.js';
import {
  handleGlobalInspectAction,
  handleInspectCdo,
  handleInspectClass,
  handleRuntimeReport
} from './inspect-global-actions.js';
import { handleInspectObject } from './inspect-object-actions.js';
import { handleGetProperty, handleSetProperty } from './inspect-property-actions.js';

export async function handleInspectTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const { normalizedAction, context } = createInspectContext(action, args, tools);

  switch (normalizedAction) {
    case 'inspect_object':
      return handleInspectObject(context);
    case 'get_property':
      return handleGetProperty(context);
    case 'set_property':
      return handleSetProperty(context);
    case 'get_components':
      return handleGetComponents(context);
    case 'get_component_property':
      return handleGetComponentProperty(context);
    case 'set_component_property':
      return handleSetComponentProperty(context);
    case 'get_component_details':
      return handleGetComponentDetails(context);
    case 'runtime_report':
      return handleRuntimeReport(context);
    case 'inspect_class':
      return handleInspectClass(context);
    case 'inspect_cdo':
      return handleInspectCdo(context);
    default: {
      const actorResult = await handleActorInspectAction(normalizedAction, context);
      if (actorResult) return actorResult;

      const globalResult = await handleGlobalInspectAction(normalizedAction, context);
      if (globalResult) return globalResult;

      // Forward the NORMALIZED action so an alias survives to the C++ layer (BUG-67fa5c: get_level_details is
      // aliased to get_world_settings, which is handled C++-side; passing raw `args` kept action=
      // get_level_details → the object-required path fired 'objectPath, actorName, or name required'). For every
      // non-aliased action normalizedAction === args.action, so this is a no-op there.
      return cleanObject(await executeAutomationRequest(
        tools,
        'inspect',
        { ...args, action: normalizedAction },
        'Automation bridge not available for inspect operations'
      )) as Record<string, unknown>;
    }
  }
}
