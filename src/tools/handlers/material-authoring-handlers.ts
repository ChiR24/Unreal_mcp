/**
 * Material Authoring Handlers for Phase 8
 *
 * Provides comprehensive material creation and shader authoring capabilities.
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import {
  normalizeArgs,
  extractString,
  extractOptionalString,
  extractOptionalNumber,
  extractOptionalBoolean,
} from './argument-helper.js';

/** Helper to extract optional object from params */
function extractOptionalObject(params: Record<string, unknown>, key: string): Record<string, unknown> | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (typeof val === 'object' && !Array.isArray(val)) return val as Record<string, unknown>;
  return undefined;
}

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

/** Material authoring response */
interface MaterialAuthoringResponse {
  success?: boolean;
  message?: string;
  error?: string;
  errorCode?: string;
  result?: Record<string, unknown>;
  assetPath?: string;
  nodeId?: string;
  [key: string]: unknown;
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
          // Parse full path into name and directory
          const parsed = parseMaterialPath(materialPath);
          if (!parsed) {
            return ResponseFactory.error('Invalid materialPath format', 'INVALID_ARGUMENT');
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
          path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        }
        
        const materialDomain = extractOptionalString(rawArgs, 'materialDomain') ?? 
                              extractOptionalString(rawArgs, 'domain') ?? 'Surface';
        const blendMode = extractOptionalString(rawArgs, 'blendMode') ?? 'Opaque';
        const shadingModel = extractOptionalString(rawArgs, 'shadingModel') ?? 'DefaultLit';
        const twoSided = extractOptionalBoolean(rawArgs, 'twoSided') ?? false;
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material',
          name,
          path,
          materialDomain,
          blendMode,
          shadingModel,
          twoSided,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material '${name}' created`);
      }

      case 'set_blend_mode': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'blendMode', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendMode = extractString(params, 'blendMode');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_blend_mode',
          assetPath,
          blendMode,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend mode', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend mode set to ${blendMode}`);
      }

      case 'set_shading_model': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'shadingModel', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const shadingModel = extractString(params, 'shadingModel');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_shading_model',
          assetPath,
          shadingModel,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set shading model', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Shading model set to ${shadingModel}`);
      }

      case 'set_material_domain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'domain', aliases: ['materialDomain'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const domain = extractString(params, 'domain');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_material_domain',
          assetPath,
          materialDomain: domain,
          save,
        })) as MaterialAuthoringResponse;

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_texture_sample',
          assetPath,
          texturePath,
          parameterName,
          x,
          y,
          samplerType,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture sample added');
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_texture_coordinate',
          assetPath,
          coordinateIndex,
          uTiling,
          vTiling,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture coordinate', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture coordinate added');
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_scalar_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' added`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_vector_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' added`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_static_switch_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add static switch', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Static switch '${parameterName}' added`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_math_node',
          assetPath,
          operation,
          x,
          y,
          constA,
          constB,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add math node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Math node '${operation}' added`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          x,
          y,
          ...args, // Pass through any additional params
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      case 'add_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'code', aliases: ['hlsl'], required: true },
          { key: 'outputType', default: 'Float1' },
          { key: 'description' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const code = extractString(params, 'code');
        const outputType = extractOptionalString(params, 'outputType') ?? 'Float1';
        const description = extractOptionalString(params, 'description');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_custom_expression',
          assetPath,
          code,
          outputType,
          description,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add custom expression', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Custom HLSL expression added');
      }

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
        
        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'connect_nodes',
          assetPath,
          sourceNodeId: effectiveSourceId,
          sourcePin,
          targetNodeId: effectiveTargetId,
          inputName: targetPin,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes connected');
      }

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
          return ResponseFactory.error('Missing required argument: assetPath or materialPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('Missing required argument: nodeId (or pinName)', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'disconnect_nodes',
          assetPath,
          nodeId,
          pinName,
        })) as MaterialAuthoringResponse;

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material_function',
          name,
          path,
          description,
          exposeToLibrary,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material function '${name}' created`);
      }

      case 'add_function_input':
      case 'add_function_output': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath'], required: true },
          { key: 'inputName', aliases: ['name', 'outputName'], required: true },
          { key: 'inputType', aliases: ['type', 'outputType'], default: 'Float3' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const inputName = extractString(params, 'inputName');
        const inputType = extractOptionalString(params, 'inputType') ?? 'Float3';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          inputName,
          inputType,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add function ${action === 'add_function_input' ? 'input' : 'output'}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Function ${action === 'add_function_input' ? 'input' : 'output'} '${inputName}' added`);
      }

      case 'use_material_function': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'functionPath', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const functionPath = extractString(params, 'functionPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'use_material_function',
          assetPath,
          functionPath,
          x,
          y,
        })) as MaterialAuthoringResponse;

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
          // Parse full path into name and directory
          const parsed = parseMaterialPath(instancePath);
          if (!parsed) {
            return ResponseFactory.error('Invalid instancePath format', 'INVALID_ARGUMENT');
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
          path = extractOptionalString(params, 'path') ?? '/Game/Materials';
          parentMaterial = extractString(params, 'parentMaterial');
        }
        
        if (!parentMaterial) {
          return ResponseFactory.error('parentMaterialPath or parent is required', 'MISSING_PARENT');
        }
        
        const save = extractOptionalBoolean(rawArgs, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material_instance',
          name,
          path,
          parentMaterial,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material instance', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material instance '${name}' created`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_scalar_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' set to ${value}`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_vector_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' set`);
      }

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_texture_parameter_value',
          assetPath,
          parameterName,
          texturePath,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set texture parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture parameter '${parameterName}' set`);
      }

      // ===== 8.5 Specialized Materials =====
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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          name,
          path,
          save,
          ...args, // Pass through extra params like layers for landscape
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action.replace(/_/g, ' ')} created`);
      }

      case 'add_landscape_layer': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layerName', required: true },
          { key: 'blendType', default: 'LB_WeightBlend' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layerName = extractString(params, 'layerName');
        const blendType = extractOptionalString(params, 'blendType') ?? 'LB_WeightBlend';

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_landscape_layer',
          assetPath,
          layerName,
          blendType,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add landscape layer', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Landscape layer '${layerName}' added`);
      }

      case 'configure_layer_blend': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layers', required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layers = params.layers;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'configure_layer_blend',
          assetPath,
          layers,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure layer blend', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Layer blend configured');
      }

      case 'compile_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'compile_material',
          assetPath,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to compile material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material compiled');
      }

      case 'get_material_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'get_material_info',
          assetPath,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material info retrieved');
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
          return ResponseFactory.error('Missing required argument: assetPath or materialPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeType) {
          return ResponseFactory.error('Missing required argument: nodeType', 'MISSING_NODE_TYPE');
        }

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_material_node',
          assetPath,
          nodeType,
          x,
          y,
        })) as MaterialAuthoringResponse;

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
          return ResponseFactory.error('Missing required argument: assetPath (or instancePath, materialPath)', 'MISSING_ASSET_PATH');
        }
        if (!parameterName) {
          return ResponseFactory.error('Missing required argument: parameterName', 'MISSING_PARAMETER_NAME');
        }
        if (value === undefined) {
          return ResponseFactory.error('Missing required argument: value', 'MISSING_VALUE');
        }

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_material_parameter',
          assetPath,
          parameterName,
          value,
          parameterType,
          save,
        })) as MaterialAuthoringResponse;

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
          return ResponseFactory.error('Missing required argument: assetPath or materialPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('Missing required argument: nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'get_material_node_details',
          assetPath,
          nodeId,
        })) as MaterialAuthoringResponse;

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
          return ResponseFactory.error('Missing required argument: assetPath or materialPath', 'MISSING_ASSET_PATH');
        }
        if (!nodeId) {
          return ResponseFactory.error('Missing required argument: nodeId', 'MISSING_NODE_ID');
        }

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'remove_material_node',
          assetPath,
          nodeId,
        })) as MaterialAuthoringResponse;

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_two_sided',
          assetPath,
          twoSided,
          save,
        })) as MaterialAuthoringResponse;

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

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_cast_shadows',
          assetPath,
          castShadows,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set cast shadows', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Cast shadows set to ${castShadows}`);
      }

      default:
        return ResponseFactory.error(
          `Unknown material authoring action: ${action}. Available actions: create_material, set_blend_mode, set_shading_model, add_texture_sample, add_scalar_parameter, add_vector_parameter, add_math_node, connect_nodes, create_material_instance, set_scalar_parameter_value, set_vector_parameter_value, set_texture_parameter_value, compile_material, get_material_info`,
          'UNKNOWN_ACTION'
        );
    }
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Material authoring error: ${err.message}`, 'MATERIAL_ERROR');
  }
}
