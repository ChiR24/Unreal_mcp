import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { createUnknownActionResponse } from '../foundation/dispatch/common-handlers.js';
import { MATERIAL_AUTHORING_ACTIONS } from '../../catalog/consolidated-tool-definitions.js';
import { handleMaterialCreationAction } from './material-authoring-creation.js';
import { handleMaterialParameterAction } from './material-authoring-parameters.js';
import { handleMaterialMathAction } from './material-authoring-math.js';
import { handleMaterialConnectionAction } from './material-authoring-connections.js';
import { handleMaterialFunctionAction } from './material-authoring-functions.js';
import { handleMaterialInstanceAction } from './material-authoring-instances.js';
import { handleMaterialSpecializedAction } from './material-authoring-specialized.js';
import { handleMaterialInfoAction } from './material-authoring-info.js';
import { handleMaterialNodeDeleteAction } from './material-authoring-node-delete.js';
import { handleMaterialNodeGraphAction } from './material-authoring-node-graph.js';
import { handleMaterialNodeGenericAction } from './material-authoring-node-generic.js';

export async function handleMaterialAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  try {
    const creationResult = await handleMaterialCreationAction(action, args, tools);
    if (creationResult !== undefined) return creationResult;
    const parameterResult = await handleMaterialParameterAction(action, args, tools);
    if (parameterResult !== undefined) return parameterResult;
    const mathResult = await handleMaterialMathAction(action, args, tools);
    if (mathResult !== undefined) return mathResult;
    const connectionResult = await handleMaterialConnectionAction(action, args, tools);
    if (connectionResult !== undefined) return connectionResult;
    const functionResult = await handleMaterialFunctionAction(action, args, tools);
    if (functionResult !== undefined) return functionResult;
    const instanceResult = await handleMaterialInstanceAction(action, args, tools);
    if (instanceResult !== undefined) return instanceResult;
    const specializedResult = await handleMaterialSpecializedAction(action, args, tools);
    if (specializedResult !== undefined) return specializedResult;
    const infoResult = await handleMaterialInfoAction(action, args, tools);
    if (infoResult !== undefined) return infoResult;
    const nodeDeleteResult = await handleMaterialNodeDeleteAction(action, args, tools);
    if (nodeDeleteResult !== undefined) return nodeDeleteResult;
    const nodeGraphResult = await handleMaterialNodeGraphAction(action, args, tools);
    if (nodeGraphResult !== undefined) return nodeGraphResult;
    const nodeGenericResult = await handleMaterialNodeGenericAction(action, args, tools);
    if (nodeGenericResult !== undefined) return nodeGenericResult;

    return createUnknownActionResponse(
      `Unknown material authoring action: ${action}. Available actions: ${MATERIAL_AUTHORING_ACTIONS.join(', ')}`
    );
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Material authoring error: ${err.message}`, 'MATERIAL_ERROR');
  }
}
