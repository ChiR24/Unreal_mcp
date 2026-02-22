import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import type { HandlerArgs, InputArgs } from '../../types/handler-types.js';
import { InputTools } from '../input.js';
import { executeAutomationRequest } from './common-handlers.js';
import { sanitizePath } from '../../utils/path-security.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/** Valid parameters for each input action */
const VALID_PARAMS_BY_ACTION: Record<string, Set<string>> = {
    create_input_action: new Set(['action', 'name', 'path', 'timeoutMs']),
    create_input_mapping_context: new Set(['action', 'name', 'path', 'timeoutMs']),
    add_mapping: new Set(['action', 'contextPath', 'actionPath', 'key', 'timeoutMs']),
    remove_mapping: new Set(['action', 'contextPath', 'actionPath', 'timeoutMs']),
    map_input_action: new Set(['action', 'contextPath', 'actionPath', 'key', 'timeoutMs']),
    set_input_trigger: new Set(['action', 'actionPath', 'triggerType', 'timeoutMs']),
    set_input_modifier: new Set(['action', 'actionPath', 'modifierType', 'timeoutMs']),
    enable_input_mapping: new Set(['action', 'contextPath', 'priority', 'timeoutMs']),
    disable_input_action: new Set(['action', 'actionPath', 'timeoutMs']),
    get_input_info: new Set(['action', 'assetPath', 'timeoutMs']),
};

/** Validate that no extraneous parameters are present */
function validateNoExtraParams(action: string, args: Record<string, unknown>): { valid: boolean; error?: string } {
    const validParams = VALID_PARAMS_BY_ACTION[action];
    if (!validParams) {
        return { valid: false, error: `Unknown action: ${action}` };
    }
    
    const providedParams = Object.keys(args);
    const extraParams = providedParams.filter(p => !validParams.has(p));
    
    if (extraParams.length > 0) {
        return { 
            valid: false, 
            error: `Invalid parameters for ${action}: ${extraParams.join(', ')}. Valid params: ${[...validParams].join(', ')}` 
        };
    }
    
    return { valid: true };
}

/** Validate and sanitize path parameters */
function validateAndSanitizePaths(paths: Record<string, unknown>): { valid: boolean; sanitized: Record<string, string>; error?: string } {
    const sanitized: Record<string, string> = {};
    
    for (const [key, value] of Object.entries(paths)) {
        if (value === undefined || value === null) continue;
        
        if (typeof value !== 'string') {
            return { valid: false, sanitized: {}, error: `${key} must be a string` };
        }
        
        if (value.length === 0) continue;
        
        try {
            sanitized[key] = sanitizePath(value);
        } catch (e) {
            const message = e instanceof Error ? e.message : String(e);
            return { 
                valid: false, 
                sanitized: {}, 
                error: `Invalid ${key}: ${message}` 
            };
        }
    }
    
    return { valid: true, sanitized };
}

/** Required path parameters for each action */
const REQUIRED_PATHS_BY_ACTION: Record<string, string[]> = {
    create_input_action: ['path'],
    create_input_mapping_context: ['path'],
    add_mapping: ['contextPath', 'actionPath'],
    remove_mapping: ['contextPath', 'actionPath'],
    map_input_action: ['contextPath', 'actionPath'],
    set_input_trigger: ['actionPath'],
    set_input_modifier: ['actionPath'],
    enable_input_mapping: ['contextPath'],
    disable_input_action: ['actionPath'],
    get_input_info: ['assetPath'],
};

export async function handleInputTools(
    action: string,
    args: HandlerArgs,
    tools: ITools
): Promise<Record<string, unknown>> {
    const argsTyped = args as InputArgs;
    const argsRecord = args as Record<string, unknown>;
    const inputTools = tools.inputTools as InputTools;
    if (!inputTools) {
        return ResponseFactory.error('Input tools not available');
    }

    const timeoutMs = getTimeoutMs();

    // Validate no extraneous parameters
    const paramValidation = validateNoExtraParams(action, argsRecord);
    if (!paramValidation.valid) {
        return ResponseFactory.error(paramValidation.error || 'Invalid parameters');
    }

    // All actions are dispatched to C++ via automation bridge
    const sendRequest = async (subAction: string): Promise<Record<string, unknown>> => {
      // Validate and sanitize path parameters
      const requiredPaths = REQUIRED_PATHS_BY_ACTION[subAction] || [];
      const pathParams: Record<string, unknown> = {};
      
      for (const pathKey of requiredPaths) {
          if (argsRecord[pathKey] !== undefined) {
              pathParams[pathKey] = argsRecord[pathKey];
          }
      }
      
      // Also check for optional path params (path, assetPath for non-required)
      if (argsRecord.path !== undefined) pathParams.path = argsRecord.path;
      if (argsRecord.assetPath !== undefined) pathParams.assetPath = argsRecord.assetPath;
      
      const pathValidation = validateAndSanitizePaths(pathParams);
      if (!pathValidation.valid) {
          return ResponseFactory.error(pathValidation.error || 'Invalid path');
      }
      
      // Build sanitized payload
      const sanitizedPayload: Record<string, unknown> = { ...argsRecord, subAction };
      
      // Apply sanitized paths back to payload
      for (const [key, value] of Object.entries(pathValidation.sanitized)) {
          sanitizedPayload[key] = value;
      }
      
      const result = await executeAutomationRequest(
        tools,
        'manage_input',
        sanitizedPayload as HandlerArgs,
        `Automation bridge not available for input action: ${subAction}`,
        { timeoutMs }
      );
      return cleanObject(result) as Record<string, unknown>;
    };

    switch (action) {
        case 'create_input_action': {
            // Validate path parameter
            const pathValidation = validateAndSanitizePaths({ path: argsTyped.path });
            if (!pathValidation.valid) {
                return ResponseFactory.error(pathValidation.error || 'Invalid path');
            }
            const sanitizedPath = pathValidation.sanitized.path ?? (argsTyped.path ?? '');
            return cleanObject(await inputTools.createInputAction(argsTyped.name || '', sanitizedPath)) as Record<string, unknown>;
        }
        case 'create_input_mapping_context': {
            // Validate path parameter
            const pathValidation = validateAndSanitizePaths({ path: argsTyped.path });
            if (!pathValidation.valid) {
                return ResponseFactory.error(pathValidation.error || 'Invalid path');
            }
            const sanitizedPath = pathValidation.sanitized.path ?? (argsTyped.path ?? '');
            return cleanObject(await inputTools.createInputMappingContext(argsTyped.name || '', sanitizedPath)) as Record<string, unknown>;
        }
        case 'add_mapping': {
            // Validate path parameters
            const pathValidation = validateAndSanitizePaths({ 
                contextPath: argsTyped.contextPath, 
                actionPath: argsTyped.actionPath 
            });
            if (!pathValidation.valid) {
                return ResponseFactory.error(pathValidation.error || 'Invalid path');
            }
            const sanitizedContextPath = pathValidation.sanitized.contextPath ?? (argsTyped.contextPath ?? '');
            const sanitizedActionPath = pathValidation.sanitized.actionPath ?? (argsTyped.actionPath ?? '');
            return cleanObject(await inputTools.addMapping(
                sanitizedContextPath,
                sanitizedActionPath,
                argsTyped.key ?? ''
            )) as Record<string, unknown>;
        }
        case 'remove_mapping': {
            // Validate path parameters
            const pathValidation = validateAndSanitizePaths({ 
                contextPath: argsTyped.contextPath, 
                actionPath: argsTyped.actionPath 
            });
            if (!pathValidation.valid) {
                return ResponseFactory.error(pathValidation.error || 'Invalid path');
            }
            const sanitizedContextPath = pathValidation.sanitized.contextPath ?? (argsTyped.contextPath ?? '');
            const sanitizedActionPath = pathValidation.sanitized.actionPath ?? (argsTyped.actionPath ?? '');
            return cleanObject(await inputTools.removeMapping(
                sanitizedContextPath,
                sanitizedActionPath
            )) as Record<string, unknown>;
        }

        // New actions - dispatched to C++ via automation bridge
        case 'map_input_action':
            return sendRequest('map_input_action');

        case 'set_input_trigger':
            return sendRequest('set_input_trigger');

        case 'set_input_modifier':
            return sendRequest('set_input_modifier');

        case 'enable_input_mapping':
            return sendRequest('enable_input_mapping');

        case 'disable_input_action':
            return sendRequest('disable_input_action');

        case 'get_input_info':
            return sendRequest('get_input_info');

        default:
            return ResponseFactory.error(`Unknown input action: ${action}`);
    }
}
