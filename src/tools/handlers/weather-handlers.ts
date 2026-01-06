/**
 * Weather Handlers (Phase 28)
 *
 * Weather system for environmental effects including:
 * - Wind: configure_wind
 * - Weather System: create_weather_system
 * - Precipitation: configure_rain_particles, configure_snow_particles
 * - Atmospheric: configure_lightning
 *
 * @module weather-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Normalize path fields to ensure they start with /Game/ and use forward slashes.
 * Returns a copy of the args with normalized paths.
 */
function normalizePathFields(args: Record<string, unknown>): Record<string, unknown> {
  const result = { ...args };
  const pathFields = [
    'niagaraSystemPath', 'particleSystemPath', 'materialPath'
  ];

  for (const field of pathFields) {
    const value = result[field];
    if (typeof value === 'string' && value.length > 0) {
      let normalized = value.replace(/\\/g, '/');
      // Replace /Content/ with /Game/ for common user mistake
      if (normalized.startsWith('/Content/')) {
        normalized = '/Game/' + normalized.slice('/Content/'.length);
      }
      // Allow /Script/ paths for built-in UE classes
      // Allow plugin paths like /MyPlugin/Assets to pass through unchanged
      if (!normalized.startsWith('/')) {
        normalized = '/Game/' + normalized;
      }
      result[field] = normalized;
    }
  }

  return result;
}

/**
 * Handles all weather actions for the manage_weather tool.
 */
export async function handleWeatherTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  // Normalize path fields before sending to C++
  const argsRecord = normalizePathFields(args as Record<string, unknown>);
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  // C++ handler reads "action" field from payload
  const sendRequest = async (actionName: string): Promise<Record<string, unknown>> => {
    const payload = { ...argsRecord, action: actionName };
    const result = await executeAutomationRequest(
      tools,
      'manage_weather',
      payload as HandlerArgs,
      `Automation bridge not available for weather action: ${actionName}`,
      { timeoutMs }
    );
    return cleanObject(result) as Record<string, unknown>;
  };

  switch (action) {
    // ========================================================================
    // Wind Configuration
    // ========================================================================
    case 'configure_wind':
      return sendRequest('configure_wind');

    // ========================================================================
    // Weather System Creation
    // ========================================================================
    case 'create_weather_system':
      return sendRequest('create_weather_system');

    // ========================================================================
    // Precipitation Effects
    // ========================================================================
    case 'configure_rain_particles':
      return sendRequest('configure_rain_particles');

    case 'configure_snow_particles':
      return sendRequest('configure_snow_particles');

    // ========================================================================
    // Atmospheric Effects
    // ========================================================================
    case 'configure_lightning':
      return sendRequest('configure_lightning');

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown weather action: ${action}`
      });
  }
}
