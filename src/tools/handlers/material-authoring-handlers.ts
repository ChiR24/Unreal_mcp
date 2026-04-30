/**
 * Material Authoring Handlers for Phase 8
 *
 * Provides comprehensive material creation, shader authoring, and material function capabilities.
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import type { AutomationResponse } from '../../types/automation-responses.js';
import { executeAutomationRequest } from './common-handlers.js';
import { sanitizePath } from '../../utils/path-security.js';
import {
  normalizeArgs,
  extractString,
  extractOptionalString,
  extractOptionalNumber,
  extractOptionalBoolean,
  extractOptionalObject,
} from './argument-helper.js';


/** Helper to parse a full material path into name and directory */
function parseMaterialPath(fullPath: string | undefined): { name: string; path: string } | null {
  if (!fullPath) return null;
  const lastSlash = fullPath.lastIndexOf('/');
  if (lastSlash < 0) return { name: fullPath, path: '/Game' };
  const name = fullPath.substring(lastSlash + 1);
  const path = fullPath.substring(0, lastSlash);
  return { name, path };
}
import { ResponseFactory } from '../../utils/response-factory.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';
import { MATERIAL_AUTHORING_ACTIONS } from '../consolidated-tool-definitions.js';

/** Normalize asset path: format conversion + security validation */
function normalizeAssetPath(p: string): string {
  let normalized = p.replace(/\\/g, '/');
  // /Content/Foo → /Game/Foo
  if (normalized.startsWith('/Content/')) {
    normalized = '/Game/' + normalized.slice('/Content/'.length);
  }
  // Content/Foo → /Game/Foo (no leading slash)
  if (normalized.startsWith('Content/')) {
    normalized = '/Game/' + normalized.slice('Content/'.length);
  }
  // Game/Foo → /Game/Foo (missing leading slash)
  if (normalized.startsWith('Game/')) {
    normalized = '/' + normalized;
  }
  // Bare name with no slash → /Game/ prefix; paths with slashes get leading / (custom mount points)
  if (!normalized.startsWith('/') && normalized.length > 0) {
    normalized = normalized.includes('/') ? '/' + normalized : '/Game/' + normalized;
  }
  // Validate with shared security utility (blocks traversal, illegal chars, ensures valid root)
  return sanitizePath(normalized);
}

/**
 * Handle material authoring actions
 */
export async function handleMaterialAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  try {
    switch (action) {
      // ===== 8.1 Material Creation =====
      case 'create_material': {
        // Check if materialPath is provided (full path like /Game/MCPTest/M_Test)
        const rawArgs = args as Record<string, unknown>;
        const materialPath = extractOptionalString(rawArgs, 'materialPath') ?? 
                            extractOptionalString(rawArgs, 'material_path') ??
                            extractOptionalString(rawArgs, 'assetPath');
        
        let name: string;
        let path: string;
        
        if (materialPath) {
          // Normalize and parse full path into name and directory
          const parsed = parseMaterialPath(normalizeAssetPath(materialPath));
          if (!parsed || !parsed.name || !parsed.path) {
            return ResponseFactory.error('manage_material_authoring.create_material: invalid materialPath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
        } else {
          // Use normalizeArgs for individual name/path
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          ]);
          name = extractString(params, 'name');
          path = normalizeAssetPath(extractOptionalString(params, 'path') ?? '/Game/Materials');
        }

        const materialDomain = extractOptionalString(rawArgs, 'materialDomain') ?? 
                              extractOptionalString(rawArgs, 'domain') ?? 'Surface';
        const blendMode = extractOptionalString(rawArgs, 'blendMode') ?? 'Opaque';
        const shadingModel = extractOptionalString(rawArgs, 'shadingModel') ?? 'DefaultLit';
        const twoSided = extractOptionalBoolean(rawArgs, 'twoSided') ?? false;
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material',
          name,
          path,
          materialDomain,
          blendMode,
          shadingModel,
          twoSided,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material '${name}' created`);
      }

      // Set the blend mode on a material (Opaque, Translucent, Masked, etc.)
      case 'set_blend_mode': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'blendMode', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendMode = extractString(params, 'blendMode');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_blend_mode',
          assetPath,
          blendMode,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend mode', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend mode set to ${blendMode}`);
      }

      // Set the shading model (DefaultLit, Unlit, Subsurface, etc.)
      case 'set_shading_model': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'shadingModel', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const shadingModel = extractString(params, 'shadingModel');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_shading_model',
          assetPath,
          shadingModel,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set shading model', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Shading model set to ${shadingModel}`);
      }

      // Set the material domain (Surface, DeferredDecal, PostProcess, etc.)
      case 'set_material_domain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'domain', aliases: ['materialDomain'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const domain = extractString(params, 'domain');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_material_domain',
          assetPath,
          materialDomain: domain,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set material domain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material domain set to ${domain}`);
      }

      // ===== 8.2 Material Expressions =====
      case 'add_texture_sample': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'texturePath', required: true },
          { key: 'parameterName', aliases: ['name'] },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'samplerType', default: 'Color' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const texturePath = extractString(params, 'texturePath');
        const parameterName = extractOptionalString(params, 'parameterName');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const samplerType = extractOptionalString(params, 'samplerType') ?? 'Color';

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_texture_sample',
          assetPath,
          texturePath,
          parameterName,
          x,
          y,
          samplerType,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture sample added');
      }

      // Add a texture coordinate (UV) node
      case 'add_texture_coordinate': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'coordinateIndex', default: 0 },
          { key: 'uTiling', default: 1.0 },
          { key: 'vTiling', default: 1.0 },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const coordinateIndex = extractOptionalNumber(params, 'coordinateIndex') ?? 0;
        const uTiling = extractOptionalNumber(params, 'uTiling') ?? 1.0;
        const vTiling = extractOptionalNumber(params, 'vTiling') ?? 1.0;
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_texture_coordinate',
          assetPath,
          coordinateIndex,
          uTiling,
          vTiling,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture coordinate', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture coordinate added');
      }

      // Add a scalar parameter expression
      case 'add_scalar_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: 0.0 },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalNumber(params, 'defaultValue') ?? 0.0;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_scalar_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' added`);
      }

      // Add a vector parameter expression (color/vector)
      case 'add_vector_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', aliases: ['color'] },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalObject(params, 'defaultValue') ?? { r: 1, g: 1, b: 1, a: 1 };
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_vector_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' added`);
      }

      // Add a static switch parameter for conditional material logic
      case 'add_static_switch_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: false },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalBoolean(params, 'defaultValue') ?? false;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_static_switch_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add static switch', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Static switch '${parameterName}' added`);
      }

      // Add a math operation node (Add, Multiply, Lerp, etc.)
      case 'add_math_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'operation', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'constA', aliases: ['valueA'] },
          { key: 'constB', aliases: ['valueB'] },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const operation = extractString(params, 'operation');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const constA = extractOptionalNumber(params, 'constA');
        const constB = extractOptionalNumber(params, 'constB');

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_math_node',
          assetPath,
          operation,
          x,
          y,
          constA,
          constB,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add math node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Math node '${operation}' added`);
      }

      // Add utility expression nodes (world position, normals, UV animation, noise, etc.)
      case 'add_world_position':
      case 'add_vertex_normal':
      case 'add_pixel_depth':
      case 'add_fresnel':
      case 'add_reflection_vector':
      case 'add_panner':
      case 'add_rotator':
      case 'add_noise':
      case 'add_voronoi': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          x,
          y,
          ...args, // Pass through any additional params
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      // Add conditional nodes (if/switch) for branching material logic
      case 'add_if':
      case 'add_switch': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      // Add a custom HLSL expression node with configurable inputs/outputs
      case 'add_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'code', aliases: ['hlsl'], required: true },
          { key: 'outputType', default: 'Float1' },
          { key: 'description' },
          { key: 'inputs' },
          { key: 'additionalOutputs' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const code = extractString(params, 'code');
        const outputType = extractOptionalString(params, 'outputType') ?? 'Float1';
        const description = extractOptionalString(params, 'description');
        const inputs = (params as Record<string, unknown>).inputs;
        const additionalOutputs = (params as Record<string, unknown>).additionalOutputs;
        if (inputs != null && !Array.isArray(inputs)) {
          return ResponseFactory.error('manage_material_authoring.add_custom_expression: inputs must be an array', 'INVALID_INPUTS');
        }
        if (inputs != null && Array.isArray(inputs)) {
          for (let i = 0; i < inputs.length; i++) {
            const item = inputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: inputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_INPUTS'
              );
            }
          }
        }
        if (additionalOutputs != null && !Array.isArray(additionalOutputs)) {
          return ResponseFactory.error('manage_material_authoring.add_custom_expression: additionalOutputs must be an array', 'INVALID_OUTPUTS');
        }
        if (additionalOutputs != null && Array.isArray(additionalOutputs)) {
          for (let i = 0; i < additionalOutputs.length; i++) {
            const item = additionalOutputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: additionalOutputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_OUTPUTS'
              );
            }
            if (item.type != null && typeof item.type !== 'string') {
              return ResponseFactory.error(
                `manage_material_authoring.add_custom_expression: additionalOutputs[${i}].type must be a string if provided`,
                'INVALID_OUTPUTS'
              );
            }
          }
        }
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const payload: Record<string, unknown> = {
          subAction: 'add_custom_expression',
          assetPath,
          code,
          outputType,
          description,
          x,
          y,
        };
        if (inputs != null) {
          payload.inputs = inputs;
        }
if (additionalOutputs != null) {
  payload.outputs = additionalOutputs;
}

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add custom expression', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Custom HLSL expression added');
      }

      // Connect two material expression nodes via their pins
      case 'connect_nodes':
      case 'connect_material_pins': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        
        // Try both formats: node-based and pin-based
        const sourceNodeId = extractOptionalString(rawArgs, 'sourceNodeId') ?? 
                            extractOptionalString(rawArgs, 'fromNode') ?? '';
        const targetNodeId = extractOptionalString(rawArgs, 'targetNodeId') ?? 
                            extractOptionalString(rawArgs, 'toNode') ?? '';
        const sourcePin = extractOptionalString(rawArgs, 'sourcePin') ?? 
                         extractOptionalString(rawArgs, 'fromPin') ?? '';
        const targetPin = extractOptionalString(rawArgs, 'targetPin') ?? 
                         extractOptionalString(rawArgs, 'toPin') ?? 
                         extractOptionalString(rawArgs, 'inputName') ?? '';
        
        // If node IDs not provided, use pin names as identifiers
        const effectiveSourceId = sourceNodeId || sourcePin;
        const effectiveTargetId = targetNodeId || targetPin;
        
        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'connect_nodes',
          assetPath,
          sourceNodeId: effectiveSourceId,
          sourcePin,
          targetNodeId: effectiveTargetId,
          inputName: targetPin,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes connected');
      }

      // Disconnect material expression nodes or break specific pin connections
      case 'disconnect_nodes':
      case 'break_material_connections': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        // Accept both nodeId and pinName as identifiers
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ?? 
                      extractOptionalString(rawArgs, 'pinName') ?? '';
        const pinName = extractOptionalString(rawArgs, 'pinName');
        
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.disconnect_nodes: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.disconnect_nodes: missing required argument nodeId (or pinName)', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'disconnect_nodes',
          assetPath,
          nodeId,
          pinName,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to disconnect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes disconnected');
      }

      // ===== 8.3 Material Functions & Layers =====
      case 'create_material_function': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials/Functions' },
          { key: 'description' },
          { key: 'exposeToLibrary', default: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials/Functions';
        const description = extractOptionalString(params, 'description');
        const exposeToLibrary = extractOptionalBoolean(params, 'exposeToLibrary') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material_function',
          name,
          path,
          description,
          exposeToLibrary,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material function '${name}' created`);
      }

      // Add an input or output pin to a material function
      case 'add_function_input':
      case 'add_function_output': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath'], required: true },
          { key: 'inputName', aliases: ['name', 'outputName'], required: true },
          { key: 'inputType', aliases: ['type', 'outputType'], default: 'Float3' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const inputName = extractString(params, 'inputName');
        const inputType = extractOptionalString(params, 'inputType') ?? 'Float3';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          assetPath,
          inputName,
          inputType,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add function ${action === 'add_function_input' ? 'input' : 'output'}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Function ${action === 'add_function_input' ? 'input' : 'output'} '${inputName}' added`);
      }

      // Insert a material function call node into a material graph
      case 'use_material_function': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'functionPath', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const functionPath = normalizeAssetPath(extractString(params, 'functionPath'));
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'use_material_function',
          assetPath,
          functionPath,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to use material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material function added');
      }

      // ===== 8.4 Material Instances =====
      case 'create_material_instance': {
        // Support both old format (name+path+parentMaterial) and new format (instancePath+parentMaterialPath)
        const rawArgs = args as Record<string, unknown>;
        const instancePath = extractOptionalString(rawArgs, 'instancePath') ?? 
                            extractOptionalString(rawArgs, 'instance_path') ??
                            extractOptionalString(rawArgs, 'materialPath');
        const parentMaterialPath = extractOptionalString(rawArgs, 'parentMaterialPath') ?? 
                                  extractOptionalString(rawArgs, 'parent_material_path') ??
                                  extractOptionalString(rawArgs, 'parentMaterial') ??
                                  extractOptionalString(rawArgs, 'parent');
        
        let name: string;
        let path: string;
        let parentMaterial: string;
        
        if (instancePath) {
          // Normalize and parse full path into name and directory
          const parsed = parseMaterialPath(normalizeAssetPath(instancePath));
          if (!parsed || !parsed.name || !parsed.path) {
            return ResponseFactory.error('manage_material_authoring.create_material_instance: invalid instancePath format', 'INVALID_ARGUMENT');
          }
          name = parsed.name;
          path = parsed.path;
          parentMaterial = parentMaterialPath ?? '';
        } else {
          // Use normalizeArgs for individual name/path
          const params = normalizeArgs(args, [
            { key: 'name', required: true },
            { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
            { key: 'parentMaterial', aliases: ['parent'], required: true },
          ]);
          name = extractString(params, 'name');
          path = normalizeAssetPath(extractOptionalString(params, 'path') ?? '/Game/Materials');
          parentMaterial = extractString(params, 'parentMaterial');
        }
        
        if (!parentMaterial) {
          return ResponseFactory.error('manage_material_authoring.create_material_instance: parentMaterialPath or parent is required', 'MISSING_PARENT');
        }
        
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'create_material_instance',
          name,
          path,
          parentMaterial,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material instance', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material instance '${name}' created`);
      }

      // Set a scalar parameter override on a material instance
      case 'set_scalar_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalNumber(params, 'value') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_scalar_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' set to ${value}`);
      }

      // Set a vector/color parameter override on a material instance
      case 'set_vector_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', aliases: ['color'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalObject(params, 'value') ?? { r: 1, g: 1, b: 1, a: 1 };
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_vector_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' set`);
      }

      // Set a texture parameter override on a material instance
      case 'set_texture_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'texturePath', aliases: ['value'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const texturePath = extractString(params, 'texturePath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_texture_parameter_value',
          assetPath,
          parameterName,
          texturePath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set texture parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture parameter '${parameterName}' set`);
      }

      // ===== 8.5 Specialized Materials =====
      // Create a specialized material (landscape, decal, or post-process)
      case 'create_landscape_material':
      case 'create_decal_material':
      case 'create_post_process_material': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: action,
          name,
          path,
          save,
          ...args, // Pass through extra params like layers for landscape
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action.replace(/_/g, ' ')} created`);
      }

      // Add a landscape layer (paint layer) to a landscape material
      case 'add_landscape_layer': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layerName', required: true },
          { key: 'blendType', default: 'LB_WeightBlend' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layerName = extractString(params, 'layerName');
        const blendType = extractOptionalString(params, 'blendType') ?? 'LB_WeightBlend';

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_landscape_layer',
          assetPath,
          layerName,
          blendType,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add landscape layer', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Landscape layer '${layerName}' added`);
      }

      // Configure the layer blend node with layer weight assignments
      case 'configure_layer_blend': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layers', required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layers = params.layers;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'configure_layer_blend',
          assetPath,
          layers,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure layer blend', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Layer blend configured');
      }

      // Compile (rebuild) the material shader and optionally save
      case 'compile_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'compile_material',
          assetPath,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to compile material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material compiled');
      }

      // Retrieve material metadata, expressions, and property overview
      case 'get_material_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'filter' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const filter = extractOptionalString(params, 'filter');

        const payload: Record<string, unknown> = {
          subAction: 'get_material_info',
          assetPath,
        };
        if (filter) payload.filter = filter;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material info retrieved');
      }

      // ===== 8.5 Graph Query & Mutation =====

      // Search for nodes by type or name
      case 'find_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeType' },
          { key: 'name' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeType = extractOptionalString(params, 'nodeType');
        const name = extractOptionalString(params, 'name');

        if (!nodeType && !name) {
          return ResponseFactory.error('manage_material_authoring.find_node: requires at least one of nodeType or name', 'MISSING_SEARCH_CRITERIA');
        }

        const payload: Record<string, unknown> = {
          subAction: 'find_node',
          assetPath,
        };
        if (nodeType) payload.nodeType = nodeType;
        if (name) payload.name = name;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to find node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node search complete');
      }

      // Get input/output connections for a node
      case 'get_node_connections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
          { key: 'direction' },
          { key: 'depth', default: 1 },
          { key: 'upstream' },
          { key: 'downstream' },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const payload: Record<string, unknown> = {
          subAction: 'get_node_connections',
          assetPath,
          nodeId,
        };
        const direction = extractOptionalString(params, 'direction');
        const depth = extractOptionalNumber(params, 'depth');
        const upstream = extractOptionalBoolean(params, 'upstream');
        const downstream = extractOptionalBoolean(params, 'downstream');
        if (direction) payload.direction = direction;
        if (depth !== undefined && depth !== null) payload.depth = depth;
        if (upstream !== undefined && upstream !== null) payload.upstream = upstream;
        if (downstream !== undefined && downstream !== null) payload.downstream = downstream;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node connections', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node connections retrieved');
      }

      // Get editable properties of a node
      case 'get_node_properties': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_node_properties',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node properties', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node properties retrieved');
      }

      // Set a static switch parameter value on a material
      case 'set_static_switch_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalBoolean(params, 'value');
        if (value === undefined) {
          return ResponseFactory.error(
            'manage_material_authoring.set_static_switch_parameter_value: value must be a boolean',
            'INVALID_VALUE'
          );
        }
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_static_switch_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set static switch parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Static switch parameter set');
      }

      // Delete one or more expression nodes
      case 'delete_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = normalizeAssetPath(extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'functionPath') ?? '');
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.delete_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        const nodeId = extractOptionalString(rawArgs, 'nodeId');
        const nodeIdsRaw = Array.isArray(rawArgs.nodeIds) ? rawArgs.nodeIds : undefined;
        const nodeIds = nodeIdsRaw?.filter((id): id is string => typeof id === 'string' && id.trim().length > 0);
        if (nodeIdsRaw && (!nodeIds || nodeIds.length !== nodeIdsRaw.length)) {
          return ResponseFactory.error(
            'manage_material_authoring.delete_node: nodeIds must be an array of non-empty strings',
            'INVALID_NODE_IDS'
          );
        }
        if (!nodeId && (!nodeIds || nodeIds.length === 0)) {
          return ResponseFactory.error(
            'manage_material_authoring.delete_node: provide nodeId or a non-empty nodeIds array',
            'MISSING_NODE_ID'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'delete_node',
          assetPath,
        };
        if (nodeId) payload.nodeId = nodeId;
        if (nodeIds && nodeIds.length > 0) payload.nodeIds = nodeIds;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to delete node(s)', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node(s) deleted');
      }

      // Update code, description, or pins on a custom expression node
      case 'update_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'nodeId', required: true },
          { key: 'code' },
          { key: 'description' },
          { key: 'outputType' },
          { key: 'inputs' },
          { key: 'additionalOutputs' },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const nodeId = extractString(params, 'nodeId');

        const payload: Record<string, unknown> = {
          subAction: 'update_custom_expression',
          assetPath,
          nodeId,
        };
        const code = extractOptionalString(params, 'code');
        const description = extractOptionalString(params, 'description');
        const outputType = extractOptionalString(params, 'outputType');
        const inputs = (params as Record<string, unknown>).inputs;
        const additionalOutputs = (params as Record<string, unknown>).additionalOutputs;
        if (inputs != null && !Array.isArray(inputs)) {
          return ResponseFactory.error('manage_material_authoring.update_custom_expression: inputs must be an array', 'INVALID_INPUTS');
        }
        if (inputs != null && Array.isArray(inputs)) {
          for (let i = 0; i < inputs.length; i++) {
            const item = inputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: inputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_INPUTS'
              );
            }
          }
        }
        if (additionalOutputs != null && !Array.isArray(additionalOutputs)) {
          return ResponseFactory.error('manage_material_authoring.update_custom_expression: additionalOutputs must be an array', 'INVALID_OUTPUTS');
        }
        if (additionalOutputs != null && Array.isArray(additionalOutputs)) {
          for (let i = 0; i < additionalOutputs.length; i++) {
            const item = additionalOutputs[i] as Record<string, unknown>;
            if (!item || typeof item !== 'object' || typeof item.name !== 'string' || !item.name.trim()) {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: additionalOutputs[${i}] must be an object with a non-empty string "name"`,
                'INVALID_OUTPUTS'
              );
            }
            if (item.type != null && typeof item.type !== 'string') {
              return ResponseFactory.error(
                `manage_material_authoring.update_custom_expression: additionalOutputs[${i}].type must be a string if provided`,
                'INVALID_OUTPUTS'
              );
            }
          }
        }
        const hasCode = code !== undefined && code !== null;
        const hasDescription = description !== undefined && description !== null;
        const hasOutputType = outputType !== undefined && outputType !== null;
  const hasInputs = inputs !== undefined && inputs !== null;
  const hasAdditionalOutputs = additionalOutputs !== undefined && additionalOutputs !== null;
        if (!hasCode && !hasDescription && !hasOutputType && !hasInputs && !hasAdditionalOutputs) {
          return ResponseFactory.error(
            'manage_material_authoring.update_custom_expression: provide at least one field to update',
            'MISSING_UPDATE_FIELDS'
          );
        }
        if (hasCode) payload.code = code;
        if (hasDescription) payload.description = description;
        if (hasOutputType) payload.outputType = outputType;
        if (hasInputs) payload.inputs = inputs;
        if (hasAdditionalOutputs) payload.outputs = additionalOutputs;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to update custom expression', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Custom expression updated');
      }

      // Trace the node chain between two nodes or from start to an output pin
      case 'get_node_chain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath', 'functionPath'], required: true },
          { key: 'startNodeId', required: true },
          { key: 'endNodeId' },
          { key: 'endPin' },
        ]);
        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));
        const startNodeId = extractString(params, 'startNodeId');
        const endNodeId = extractOptionalString(params, 'endNodeId');
        const endPin = extractOptionalString(params, 'endPin');
        if (!endNodeId && !endPin) {
          return ResponseFactory.error(
            'manage_material_authoring.get_node_chain: provide endNodeId or endPin',
            'INVALID_ARGUMENT'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'get_node_chain',
          assetPath,
          startNodeId,
        };
        if (endNodeId) payload.endNodeId = endNodeId;
        if (endPin) payload.endPin = endPin;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to trace node chain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node chain traced');
      }

      // Get the connected subgraph or find orphan nodes
      case 'get_connected_subgraph': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = normalizeAssetPath(extractOptionalString(rawArgs, 'assetPath') ??
                         extractOptionalString(rawArgs, 'materialPath') ??
                         extractOptionalString(rawArgs, 'functionPath') ?? '');
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.get_connected_subgraph: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        const nodeId = extractOptionalString(rawArgs, 'nodeId');
        const orphansOnly = extractOptionalBoolean(rawArgs, 'orphansOnly') ?? false;
        if (!nodeId && !orphansOnly) {
          return ResponseFactory.error(
            'manage_material_authoring.get_connected_subgraph: provide nodeId or set orphansOnly=true',
            'MISSING_NODE_ID'
          );
        }

        const payload: Record<string, unknown> = {
          subAction: 'get_connected_subgraph',
          assetPath,
          orphansOnly,
        };
        if (nodeId) payload.nodeId = nodeId;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, payload)) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get connected subgraph', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Connected subgraph retrieved');
      }

      // Get material function inputs, outputs, and expression details
      case 'get_material_function_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath', 'materialFunctionPath'], required: true },
        ]);

        const assetPath = normalizeAssetPath(extractString(params, 'assetPath'));

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_material_function_info',
          assetPath,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material function info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material function info retrieved');
      }

      // ===== 8.6 Aliases and Additional Actions =====

      // Alias: add_material_node -> add_math_node
      case 'add_material_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeType = extractOptionalString(rawArgs, 'nodeType') ?? 
                        extractOptionalString(rawArgs, 'type') ?? '';
        const x = extractOptionalNumber(rawArgs, 'x') ?? 0;
        const y = extractOptionalNumber(rawArgs, 'y') ?? 0;
        
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.add_material_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeType) {
          return ResponseFactory.error('manage_material_authoring.add_material_node: missing required argument nodeType', 'MISSING_NODE_TYPE');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'add_material_node',
          assetPath,
          nodeType,
          x,
          y,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add material node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material node '${nodeType}' added`);
      }

      // Alias connect_material_pins -> connect_nodes is handled via fallthrough at the connect_nodes case above

      // Alias: rebuild_material -> compile_material
      case 'rebuild_material':
        return handleMaterialAuthoringTools('compile_material', args, tools);

      // Generic parameter setter
      case 'set_material_parameter': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? 
                         extractOptionalString(rawArgs, 'instancePath') ?? '';
        const parameterName = extractOptionalString(rawArgs, 'parameterName') ?? '';
        const parameterType = extractOptionalString(rawArgs, 'parameterType') ?? 'scalar';
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;
        const value = rawArgs.value;
        
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!parameterName) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument parameterName', 'MISSING_PARAMETER_NAME');
        }
        if (value === undefined) {
          return ResponseFactory.error('manage_material_authoring.set_material_parameter: missing required argument value', 'MISSING_VALUE');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_material_parameter',
          assetPath,
          parameterName,
          value,
          parameterType,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Parameter '${parameterName}' set`);
      }

      // Get node details
      case 'get_material_node_details': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ?? '';
        
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.get_material_node_details: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.get_material_node_details: missing required argument nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'get_material_node_details',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get node details', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Node details retrieved');
      }

      // Remove material node
      case 'remove_material_node': {
        const rawArgs = args as Record<string, unknown>;
        const assetPath = extractOptionalString(rawArgs, 'assetPath') ?? 
                         extractOptionalString(rawArgs, 'materialPath') ?? '';
        const nodeId = extractOptionalString(rawArgs, 'nodeId') ?? '';
        
        if (!assetPath) {
          return ResponseFactory.error('manage_material_authoring.remove_material_node: missing required argument assetPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('manage_material_authoring.remove_material_node: missing required argument nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'remove_material_node',
          assetPath,
          nodeId,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to remove node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material node removed');
      }

      // Set two-sided property
      case 'set_two_sided': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'twoSided', aliases: ['enabled'], default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const twoSided = extractOptionalBoolean(params, 'twoSided') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_two_sided',
          assetPath,
          twoSided,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set two-sided', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Two-sided set to ${twoSided}`);
      }

      // Set cast shadows property
      case 'set_cast_shadows': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'castShadows', aliases: ['enabled'], default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const castShadows = extractOptionalBoolean(params, 'castShadows') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, TOOL_ACTIONS.MANAGE_MATERIAL_AUTHORING, {
          subAction: 'set_cast_shadows',
          assetPath,
          castShadows,
          save,
        })) as AutomationResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set cast shadows', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Cast shadows set to ${castShadows}`);
      }

      default:
        return ResponseFactory.error(
          `Unknown material authoring action: ${action}. Available actions: ${MATERIAL_AUTHORING_ACTIONS.join(', ')}`,
          'UNKNOWN_ACTION'
        );
    }
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Material authoring error: ${err.message}`, 'MATERIAL_ERROR');
  }
}
