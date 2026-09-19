import type { AutomationBridge } from './automation/index.js';
import type { StandardActionResponse } from './types/tools/tool-interfaces.js';
import { isStringArray, resultRecord } from './unreal-bridge-response.js';
import type {
  AutomationRequestResponse,
  ObjectPropertyReadParams,
  ObjectPropertyWriteParams
} from './unreal-bridge-types.js';

function validatePropertyTarget(objectPath: string, propertyName: string): void {
  if (!objectPath || typeof objectPath !== 'string') {
    throw new Error('Invalid objectPath: must be a non-empty string');
  }
  if (!propertyName || typeof propertyName !== 'string') {
    throw new Error('Invalid propertyName: must be a non-empty string');
  }
}

function propertyBridgeUnavailable(objectPath: string, propertyName: string): StandardActionResponse {
  return {
    success: false,
    objectPath,
    propertyName,
    error: 'Automation bridge not connected',
    transport: 'automation_bridge'
  };
}

// The two failure shapes are identical for reads and writes, so they live here
// once: a bridge call that came back unsuccessful, and a bridge call that threw.
function propertyBridgeFailure(
  objectPath: string,
  propertyName: string,
  response: AutomationRequestResponse,
  raw: Record<string, unknown> | undefined
): StandardActionResponse {
  return {
    success: false,
    objectPath,
    propertyName,
    error: response.error || response.message || 'AUTOMATION_BRIDGE_FAILURE',
    transport: 'automation_bridge',
    raw,
    bridge: {
      requestId: response.requestId,
      success: false,
      error: response.error
    }
  };
}

function propertyBridgeThrew(
  objectPath: string,
  propertyName: string,
  error: unknown
): StandardActionResponse {
  return {
    success: false,
    objectPath,
    propertyName,
    error: error instanceof Error ? error.message : String(error),
    transport: 'automation_bridge'
  };
}

export async function getObjectProperty(
  bridge: AutomationBridge | undefined,
  params: ObjectPropertyReadParams
): Promise<StandardActionResponse> {
  const { objectPath, propertyName, timeoutMs } = params;
  validatePropertyTarget(objectPath, propertyName);

  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    return {
      success: true,
      objectPath,
      propertyName,
      value: 'MockValue',
      propertyValue: 'MockValue',
      transport: 'mock_bridge',
      message: 'Mock property read successful'
    };
  }

  if (!bridge || typeof bridge.sendAutomationRequest !== 'function') {
    return propertyBridgeUnavailable(objectPath, propertyName);
  }

  try {
    const response = await bridge.sendAutomationRequest<AutomationRequestResponse>(
      'get_object_property',
      { objectPath, propertyName },
      timeoutMs ? { timeoutMs } : undefined
    );

    const success = response.success !== false;
    const raw = resultRecord(response);
    const value = raw?.value ?? raw?.propertyValue ?? (success ? raw : undefined);

    if (success) {
      return {
        success: true,
        objectPath,
        propertyName,
        value,
        propertyValue: value,
        transport: 'automation_bridge',
        message: response.message,
        warnings: isStringArray(raw?.warnings) ? raw.warnings : undefined,
        raw,
        bridge: {
          requestId: response.requestId,
          success: true,
          error: response.error
        }
      };
    }

    return propertyBridgeFailure(objectPath, propertyName, response, raw);
  } catch (error) {
    return propertyBridgeThrew(objectPath, propertyName, error);
  }
}

export async function setObjectProperty(
  bridge: AutomationBridge | undefined,
  params: ObjectPropertyWriteParams
): Promise<StandardActionResponse> {
  const { objectPath, propertyName, value, markDirty, timeoutMs } = params;
  validatePropertyTarget(objectPath, propertyName);

  if (process.env.MOCK_UNREAL_CONNECTION === 'true') {
    return {
      success: true,
      objectPath,
      propertyName,
      message: 'Mock property set successful',
      transport: 'mock_bridge'
    };
  }

  if (!bridge || typeof bridge.sendAutomationRequest !== 'function') {
    return propertyBridgeUnavailable(objectPath, propertyName);
  }

  const payload: Record<string, unknown> = { objectPath, propertyName, value };
  if (markDirty !== undefined) {
    payload.markDirty = Boolean(markDirty);
  }

  try {
    const response = await bridge.sendAutomationRequest<AutomationRequestResponse>(
      'set_object_property',
      payload,
      timeoutMs ? { timeoutMs } : undefined
    );

    const success = response.success !== false;
    const raw = resultRecord(response);
    const rawMessage = typeof raw?.message === 'string' ? raw.message : undefined;

    if (success) {
      return {
        success: true,
        objectPath,
        propertyName,
        message: response.message || rawMessage,
        transport: 'automation_bridge',
        raw,
        bridge: {
          requestId: response.requestId,
          success: true,
          error: response.error
        }
      };
    }

    return propertyBridgeFailure(objectPath, propertyName, response, raw);
  } catch (error) {
    return propertyBridgeThrew(objectPath, propertyName, error);
  }
}
