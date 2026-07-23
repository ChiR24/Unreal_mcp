import { isRecord } from '../utils/validation/type-guards.js';
import { handleManageToolsCall } from './tool-registry-manage-tools.js';
import { getString, gatewayError, isGatewayFailure, nextGatewayCorrelationId } from './gateway/gateway-shared.js';
import { describeGatewayCapability } from './gateway/gateway-describe.js';
import { searchGatewayCapabilities } from './gateway/gateway-search.js';
import { executeGatewayCall, type GatewayContext } from './gateway/gateway-execute.js';

export type { GatewayContext };

async function configureGateway(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  const action = getString(args, 'action');
  if (!action) return gatewayError('configure', 'MISSING_ACTION', 'configure requires a manage_tools action.');
  if (!isRecord(args.params) && args.params !== undefined) return gatewayError('configure', 'INVALID_PARAMS', 'params must be an object.');
  const result = await handleManageToolsCall({ ...(isRecord(args.params) ? args.params : {}), action });
  const envelope: Record<string, unknown> = { success: result.success === true, operation: 'configure', action, result };
  // get_status surfaces the session's derived structural client profile; hoist it
  // to the envelope top level so a gateway caller reads it without unwrapping `result`.
  if (result.clientProfile !== undefined) {
    envelope.clientProfile = result.clientProfile;
  }
  return envelope;
}

async function dispatchGatewayOperation(
  operation: string,
  args: Record<string, unknown>,
  context: GatewayContext
): Promise<Record<string, unknown>> {
  switch (operation) {
    case 'search': return searchGatewayCapabilities(args);
    case 'describe': return describeGatewayCapability(args);
    case 'execute': return await executeGatewayCall(args, context);
    case 'configure': return await configureGateway(args);
    default: return gatewayError(operation, 'UNKNOWN_OPERATION', 'operation must be search, describe, execute, or configure.');
  }
}

export async function handleUnrealGatewayCall(args: Record<string, unknown>, context: GatewayContext): Promise<Record<string, unknown>> {
  const operation = getString(args, 'operation') ?? 'unknown';
  const correlationId = nextGatewayCorrelationId();
  const tool = getString(args, 'tool');
  const action = getString(args, 'action');
  context.logger.debug('gateway request received', { correlationId, operation, tool, action });

  const result = await dispatchGatewayOperation(operation, args, context);

  if (isGatewayFailure(result)) {
    context.logger.warn('gateway request failed', {
      correlationId,
      operation,
      tool,
      action,
      errorCode: typeof result.errorCode === 'string' ? result.errorCode : undefined
    });
  } else {
    context.logger.debug('gateway request completed', { correlationId, operation, tool, action });
  }

  return result;
}

export { searchGatewayCapabilities as searchGatewayCatalog, describeGatewayCapability };
