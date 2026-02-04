#!/usr/bin/env node
import path from 'node:path';
import { pathToFileURL } from 'node:url';
/**
 * Authoring Tools Integration Tests
 * 
 * Tools: manage_material_authoring (73), manage_geometry (80), manage_skeleton (54),
 *        manage_audio (134), manage_sequence (100), manage_widget_authoring (73)
 * Total Actions: 514
 * Test Cases: ~1028 (2x coverage: success + edge cases)
 * 
 * Usage:
 *   node tests/category-tests/authoring-tools.test.mjs
 */

import { runToolTests } from '../test-runner.mjs';

const TEST_FOLDER = '/Game/AuthoringToolsTest';

// ============================================================================
// MANAGE_MATERIAL_AUTHORING (73 actions)
// ============================================================================
const manageMaterialAuthoringTests = [
  // === Material Creation ===
  { scenario: 'MatAuth: create_material', toolName: 'manage_material_authoring', arguments: { action: 'create_material', name: 'M_AuthTest', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: create_material_instance', toolName: 'manage_material_authoring', arguments: { action: 'create_material_instance', name: 'MI_AuthTest', path: TEST_FOLDER, parentMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|already exists' },
  { scenario: 'MatAuth: duplicate_material', toolName: 'manage_material_authoring', arguments: { action: 'duplicate_material', sourcePath: '/Engine/EngineMaterials/DefaultMaterial', destPath: TEST_FOLDER, name: 'M_Duplicate' }, expected: 'success|not found' },
  
  // === Material Nodes ===
  { scenario: 'MatAuth: add_material_node TextureSample', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'TextureSample', nodeName: 'BaseColorTex' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Constant3Vector', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Constant3Vector', nodeName: 'ColorConst' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Multiply', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Multiply', nodeName: 'Mult1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Add', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Add', nodeName: 'Add1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Lerp', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Lerp', nodeName: 'Lerp1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Time', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Time', nodeName: 'TimeNode' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Panner', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Panner', nodeName: 'UVPanner' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node TexCoord', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'TexCoord', nodeName: 'UV0' }, expected: 'success|not found' },
  
  // === Node Connections ===
  { scenario: 'MatAuth: connect_material_pins', toolName: 'manage_material_authoring', arguments: { action: 'connect_material_pins', materialPath: `${TEST_FOLDER}/M_AuthTest`, fromNode: 'ColorConst', fromPin: 'RGB', toNode: 'BaseColor', toPin: 'BaseColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: disconnect_material_pin', toolName: 'manage_material_authoring', arguments: { action: 'disconnect_material_pin', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeName: 'BaseColor', pinName: 'BaseColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: remove_material_node', toolName: 'manage_material_authoring', arguments: { action: 'remove_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeName: 'TimeNode' }, expected: 'success|not found' },
  
  // === Material Properties ===
  { scenario: 'MatAuth: set_material_property BlendMode', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'BlendMode', value: 'Translucent' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property ShadingModel', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'ShadingModel', value: 'Unlit' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property TwoSided', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'TwoSided', value: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_material_property', toolName: 'manage_material_authoring', arguments: { action: 'get_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'BlendMode' }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_material_info', toolName: 'manage_material_authoring', arguments: { action: 'get_material_info', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_material_nodes', toolName: 'manage_material_authoring', arguments: { action: 'get_material_nodes', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  
  // === Material Instance Parameters ===
  { scenario: 'MatAuth: set_scalar_parameter', toolName: 'manage_material_authoring', arguments: { action: 'set_scalar_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'Metallic', value: 0.8 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_vector_parameter', toolName: 'manage_material_authoring', arguments: { action: 'set_vector_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'BaseColor', value: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_texture_parameter', toolName: 'manage_material_authoring', arguments: { action: 'set_texture_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'DiffuseTexture', texturePath: '/Engine/EngineResources/DefaultTexture' }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_scalar_parameter', toolName: 'manage_material_authoring', arguments: { action: 'get_scalar_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'Metallic' }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_vector_parameter', toolName: 'manage_material_authoring', arguments: { action: 'get_vector_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'BaseColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: list_material_parameters', toolName: 'manage_material_authoring', arguments: { action: 'list_material_parameters', instancePath: `${TEST_FOLDER}/MI_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: reset_material_parameter', toolName: 'manage_material_authoring', arguments: { action: 'reset_material_parameter', instancePath: `${TEST_FOLDER}/MI_AuthTest`, parameterName: 'Metallic' }, expected: 'success|not found' },
  
  // === Material Expressions ===
  { scenario: 'MatAuth: add_expression Fresnel', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Fresnel' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Noise', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Noise' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression WorldPosition', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'WorldPosition' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression VertexNormalWS', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'VertexNormalWS' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression CameraVector', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'CameraVector' }, expected: 'success|not found' },
  
  // === Material Functions ===
  { scenario: 'MatAuth: create_material_function', toolName: 'manage_material_authoring', arguments: { action: 'create_material_function', name: 'MF_TestFunc', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_function_input', toolName: 'manage_material_authoring', arguments: { action: 'add_function_input', functionPath: `${TEST_FOLDER}/MF_TestFunc`, inputName: 'Color', inputType: 'Vector3' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_function_output', toolName: 'manage_material_authoring', arguments: { action: 'add_function_output', functionPath: `${TEST_FOLDER}/MF_TestFunc`, outputName: 'Result', outputType: 'Vector3' }, expected: 'success|not found' },
  { scenario: 'MatAuth: call_material_function', toolName: 'manage_material_authoring', arguments: { action: 'call_material_function', materialPath: `${TEST_FOLDER}/M_AuthTest`, functionPath: `${TEST_FOLDER}/MF_TestFunc` }, expected: 'success|not found' },
  
  // === Landscape Materials ===
  { scenario: 'MatAuth: create_landscape_material', toolName: 'manage_material_authoring', arguments: { action: 'create_landscape_material', name: 'M_Landscape', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_landscape_layer', toolName: 'manage_material_authoring', arguments: { action: 'add_landscape_layer', materialPath: `${TEST_FOLDER}/M_Landscape`, layerName: 'Grass' }, expected: 'success|not found' },
  { scenario: 'MatAuth: configure_landscape_blend', toolName: 'manage_material_authoring', arguments: { action: 'configure_landscape_blend', materialPath: `${TEST_FOLDER}/M_Landscape`, blendType: 'HeightBlend' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_landscape_layer_weight', toolName: 'manage_material_authoring', arguments: { action: 'set_landscape_layer_weight', materialPath: `${TEST_FOLDER}/M_Landscape`, layerName: 'Grass', weight: 1.0 }, expected: 'success|not found' },
  
  // === Decal Materials ===
  { scenario: 'MatAuth: create_decal_material', toolName: 'manage_material_authoring', arguments: { action: 'create_decal_material', name: 'M_Decal', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: configure_decal_properties', toolName: 'manage_material_authoring', arguments: { action: 'configure_decal_properties', materialPath: `${TEST_FOLDER}/M_Decal`, blendMode: 'Translucent', sortPriority: 1 }, expected: 'success|not found' },
  
  // === Post Process Materials ===
  { scenario: 'MatAuth: create_postprocess_material', toolName: 'manage_material_authoring', arguments: { action: 'create_postprocess_material', name: 'M_PP', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: configure_postprocess_blendables', toolName: 'manage_material_authoring', arguments: { action: 'configure_postprocess_blendables', materialPath: `${TEST_FOLDER}/M_PP`, blendableLocation: 'BeforeTonemapping' }, expected: 'success|not found' },
  
  // === Texture Operations ===
  { scenario: 'MatAuth: create_texture_render_target', toolName: 'manage_material_authoring', arguments: { action: 'create_texture_render_target', name: 'RT_MatAuth', path: TEST_FOLDER, width: 512, height: 512 }, expected: 'success|already exists' },
  { scenario: 'MatAuth: import_texture', toolName: 'manage_material_authoring', arguments: { action: 'import_texture', sourcePath: 'C:/temp/test.png', destPath: TEST_FOLDER, name: 'T_Imported' }, expected: 'success|not found|file not found' },
  { scenario: 'MatAuth: configure_texture_compression', toolName: 'manage_material_authoring', arguments: { action: 'configure_texture_compression', texturePath: `${TEST_FOLDER}/T_Imported`, compressionSettings: 'TC_Default' }, expected: 'success|not found' },
  { scenario: 'MatAuth: configure_texture_lod', toolName: 'manage_material_authoring', arguments: { action: 'configure_texture_lod', texturePath: `${TEST_FOLDER}/T_Imported`, lodBias: 0, lodGroup: 'World' }, expected: 'success|not found' },
  
  // === Material Compilation ===
  { scenario: 'MatAuth: compile_material', toolName: 'manage_material_authoring', arguments: { action: 'compile_material', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_material_stats', toolName: 'manage_material_authoring', arguments: { action: 'get_material_stats', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: validate_material', toolName: 'manage_material_authoring', arguments: { action: 'validate_material', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: batch_compile_materials', toolName: 'manage_material_authoring', arguments: { action: 'batch_compile_materials', directory: TEST_FOLDER }, expected: 'success' },
  
  // === Subsurface Profiles ===
  { scenario: 'MatAuth: create_subsurface_profile', toolName: 'manage_material_authoring', arguments: { action: 'create_subsurface_profile', name: 'SSP_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: configure_subsurface_profile', toolName: 'manage_material_authoring', arguments: { action: 'configure_subsurface_profile', profilePath: `${TEST_FOLDER}/SSP_Test`, scatterRadius: 1.2, subsurfaceColor: { r: 0.8, g: 0.2, b: 0.1 } }, expected: 'success|not found' },
  
  // === Material Layers ===
  { scenario: 'MatAuth: create_material_layer', toolName: 'manage_material_authoring', arguments: { action: 'create_material_layer', name: 'ML_Base', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: create_material_layer_blend', toolName: 'manage_material_authoring', arguments: { action: 'create_material_layer_blend', name: 'MLB_Blend', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_layer_to_material', toolName: 'manage_material_authoring', arguments: { action: 'add_layer_to_material', materialPath: `${TEST_FOLDER}/M_AuthTest`, layerPath: `${TEST_FOLDER}/ML_Base` }, expected: 'success|not found' },
  
  // === Virtual Texturing ===
  { scenario: 'MatAuth: configure_virtual_texture', toolName: 'manage_material_authoring', arguments: { action: 'configure_virtual_texture', materialPath: `${TEST_FOLDER}/M_AuthTest`, enableVT: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: create_runtime_virtual_texture', toolName: 'manage_material_authoring', arguments: { action: 'create_runtime_virtual_texture', name: 'RVT_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  
  // === Physical Materials ===
  { scenario: 'MatAuth: create_physical_material', toolName: 'manage_material_authoring', arguments: { action: 'create_physical_material', name: 'PM_Metal', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: configure_physical_material', toolName: 'manage_material_authoring', arguments: { action: 'configure_physical_material', physicalMaterialPath: `${TEST_FOLDER}/PM_Metal`, friction: 0.7, restitution: 0.3, surfaceType: 'Metal' }, expected: 'success|not found' },
  
  // === Nanite Materials ===
  { scenario: 'MatAuth: configure_nanite_material', toolName: 'manage_material_authoring', arguments: { action: 'configure_nanite_material', materialPath: `${TEST_FOLDER}/M_AuthTest`, enableNanite: true }, expected: 'success|not found|not supported' },
  { scenario: 'MatAuth: configure_nanite_displacement', toolName: 'manage_material_authoring', arguments: { action: 'configure_nanite_displacement', materialPath: `${TEST_FOLDER}/M_AuthTest`, displacementMagnitude: 10.0 }, expected: 'success|not found|not supported' },
  
  // === Substrate Materials ===
  { scenario: 'MatAuth: convert_to_substrate', toolName: 'manage_material_authoring', arguments: { action: 'convert_to_substrate', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found|not supported' },
  { scenario: 'MatAuth: add_substrate_slab', toolName: 'manage_material_authoring', arguments: { action: 'add_substrate_slab', materialPath: `${TEST_FOLDER}/M_AuthTest`, slabType: 'Standard' }, expected: 'success|not found|not supported' },
  { scenario: 'MatAuth: add_substrate_slab_thin', toolName: 'manage_material_authoring', arguments: { action: 'add_substrate_slab', materialPath: `${TEST_FOLDER}/M_AuthTest`, slabType: 'ThinFilm' }, expected: 'success|not found|not supported' },
  { scenario: 'MatAuth: configure_substrate_slab', toolName: 'manage_material_authoring', arguments: { action: 'configure_substrate_slab', materialPath: `${TEST_FOLDER}/M_AuthTest`, slabIndex: 0, diffuseAlbedo: { r: 0.8, g: 0.2, b: 0.1 } }, expected: 'success|not found|not supported' },
  
  // === Additional Material Node Types ===
  { scenario: 'MatAuth: add_material_node Saturate', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Saturate', nodeName: 'Sat1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node OneMinus', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'OneMinus', nodeName: 'Invert1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Power', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Power', nodeName: 'Pow1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Clamp', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Clamp', nodeName: 'Clamp1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Sine', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Sine', nodeName: 'Sin1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Cosine', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Cosine', nodeName: 'Cos1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Floor', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Floor', nodeName: 'Floor1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Ceil', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Ceil', nodeName: 'Ceil1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Frac', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Frac', nodeName: 'Frac1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Abs', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Abs', nodeName: 'Abs1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Normalize', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Normalize', nodeName: 'Norm1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node DotProduct', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'DotProduct', nodeName: 'Dot1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node CrossProduct', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'CrossProduct', nodeName: 'Cross1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node Distance', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'Distance', nodeName: 'Dist1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node ComponentMask', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'ComponentMask', nodeName: 'Mask1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node AppendVector', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'AppendVector', nodeName: 'Append1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node BreakOutFloat3', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'BreakOutFloat3', nodeName: 'Break1' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_material_node MakeFloat3', toolName: 'manage_material_authoring', arguments: { action: 'add_material_node', materialPath: `${TEST_FOLDER}/M_AuthTest`, nodeType: 'MakeFloat3', nodeName: 'Make1' }, expected: 'success|not found' },
  
  // === Additional Expressions ===
  { scenario: 'MatAuth: add_expression DepthFade', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'DepthFade' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression PixelDepth', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'PixelDepth' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression SceneDepth', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'SceneDepth' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression SceneColor', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'SceneColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ObjectPosition', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ObjectPosition' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ActorPosition', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ActorPosition' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ObjectRadius', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ObjectRadius' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ObjectBounds', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ObjectBounds' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression VertexColor', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'VertexColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression PerInstanceRandom', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'PerInstanceRandom' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression TwoSidedSign', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'TwoSidedSign' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression LightVector', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'LightVector' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ReflectionVector', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ReflectionVector' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ScreenPosition', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ScreenPosition' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ViewSize', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ViewSize' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ParticleColor', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'ParticleColor' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression DynamicParameter', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'DynamicParameter' }, expected: 'success|not found' },
  
  // === Additional Material Properties ===
  { scenario: 'MatAuth: set_material_property OpacityMaskClipValue', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'OpacityMaskClipValue', value: 0.333 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property DitheredLODTransition', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'DitheredLODTransition', value: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property AllowNegativeEmissiveColor', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'AllowNegativeEmissiveColor', value: false }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property bUseMaterialAttributes', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'bUseMaterialAttributes', value: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property bCastDynamicShadowAsMasked', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'bCastDynamicShadowAsMasked', value: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property RefractionDepthBias', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'RefractionDepthBias', value: 0.5 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_property TranslucencyLightingMode', toolName: 'manage_material_authoring', arguments: { action: 'set_material_property', materialPath: `${TEST_FOLDER}/M_AuthTest`, propertyName: 'TranslucencyLightingMode', value: 'SurfacePerPixelLighting' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain surface', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'Surface' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain deferred_decal', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'DeferredDecal' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain light_function', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'LightFunction' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain volume', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'Volume' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain postprocess', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'PostProcess' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_material_domain ui', toolName: 'manage_material_authoring', arguments: { action: 'set_material_domain', materialPath: `${TEST_FOLDER}/M_AuthTest`, domain: 'UI' }, expected: 'success|not found' },
  
  // === Material Instance Dynamic ===
  { scenario: 'MatAuth: create_material_instance_dynamic', toolName: 'manage_material_authoring', arguments: { action: 'create_material_instance_dynamic', name: 'MID_Runtime', parentMaterial: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_mid_scalar', toolName: 'manage_material_authoring', arguments: { action: 'set_mid_scalar', instanceName: 'MID_Runtime', parameterName: 'Opacity', value: 0.5 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_mid_vector', toolName: 'manage_material_authoring', arguments: { action: 'set_mid_vector', instanceName: 'MID_Runtime', parameterName: 'Color', value: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_mid_texture', toolName: 'manage_material_authoring', arguments: { action: 'set_mid_texture', instanceName: 'MID_Runtime', parameterName: 'BaseColor', texturePath: '/Engine/EngineResources/DefaultTexture' }, expected: 'success|not found' },
  
  // === Material Parameter Collections ===
  { scenario: 'MatAuth: create_material_parameter_collection', toolName: 'manage_material_authoring', arguments: { action: 'create_material_parameter_collection', name: 'MPC_Global', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_scalar_to_collection', toolName: 'manage_material_authoring', arguments: { action: 'add_scalar_to_collection', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'WindStrength', defaultValue: 1.0 }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_vector_to_collection', toolName: 'manage_material_authoring', arguments: { action: 'add_vector_to_collection', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'SunDirection', defaultValue: { r: 0, g: 0.7, b: 0.7, a: 1 } }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_collection_scalar', toolName: 'manage_material_authoring', arguments: { action: 'set_collection_scalar', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'WindStrength', value: 2.0 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_collection_vector', toolName: 'manage_material_authoring', arguments: { action: 'set_collection_vector', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'SunDirection', value: { r: 0.5, g: 0.5, b: 0.7, a: 1 } }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_collection_scalar', toolName: 'manage_material_authoring', arguments: { action: 'get_collection_scalar', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'WindStrength' }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_collection_vector', toolName: 'manage_material_authoring', arguments: { action: 'get_collection_vector', collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'SunDirection' }, expected: 'success|not found' },
  { scenario: 'MatAuth: use_collection_parameter', toolName: 'manage_material_authoring', arguments: { action: 'use_collection_parameter', materialPath: `${TEST_FOLDER}/M_AuthTest`, collectionPath: `${TEST_FOLDER}/MPC_Global`, parameterName: 'WindStrength' }, expected: 'success|not found' },
  
  // === Texture Streaming & Settings ===
  { scenario: 'MatAuth: configure_texture_streaming', toolName: 'manage_material_authoring', arguments: { action: 'configure_texture_streaming', texturePath: `${TEST_FOLDER}/T_Imported`, neverStream: false, streamingPriority: 0 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_texture_filter', toolName: 'manage_material_authoring', arguments: { action: 'set_texture_filter', texturePath: `${TEST_FOLDER}/T_Imported`, filter: 'TF_Trilinear' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_texture_address_mode', toolName: 'manage_material_authoring', arguments: { action: 'set_texture_address_mode', texturePath: `${TEST_FOLDER}/T_Imported`, addressX: 'Wrap', addressY: 'Wrap' }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_texture_srgb', toolName: 'manage_material_authoring', arguments: { action: 'set_texture_srgb', texturePath: `${TEST_FOLDER}/T_Imported`, srgb: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: generate_texture_mips', toolName: 'manage_material_authoring', arguments: { action: 'generate_texture_mips', texturePath: `${TEST_FOLDER}/T_Imported` }, expected: 'success|not found' },
  { scenario: 'MatAuth: resize_texture', toolName: 'manage_material_authoring', arguments: { action: 'resize_texture', texturePath: `${TEST_FOLDER}/T_Imported`, maxSize: 1024 }, expected: 'success|not found' },
  
  // === Curve Atlas & Gradient ===
  { scenario: 'MatAuth: create_curve_atlas', toolName: 'manage_material_authoring', arguments: { action: 'create_curve_atlas', name: 'CA_Gradients', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_curve_to_atlas', toolName: 'manage_material_authoring', arguments: { action: 'add_curve_to_atlas', atlasPath: `${TEST_FOLDER}/CA_Gradients`, curvePath: `${TEST_FOLDER}/Curve_Fade` }, expected: 'success|not found' },
  { scenario: 'MatAuth: create_gradient_curve', toolName: 'manage_material_authoring', arguments: { action: 'create_gradient_curve', name: 'Curve_Fade', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: add_gradient_key', toolName: 'manage_material_authoring', arguments: { action: 'add_gradient_key', curvePath: `${TEST_FOLDER}/Curve_Fade`, time: 0, value: 0 }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_gradient_key_end', toolName: 'manage_material_authoring', arguments: { action: 'add_gradient_key', curvePath: `${TEST_FOLDER}/Curve_Fade`, time: 1, value: 1 }, expected: 'success|not found' },
  
  // === Material Utilities ===
  { scenario: 'MatAuth: list_materials', toolName: 'manage_material_authoring', arguments: { action: 'list_materials', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'MatAuth: list_material_instances', toolName: 'manage_material_authoring', arguments: { action: 'list_material_instances', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'MatAuth: list_material_functions', toolName: 'manage_material_authoring', arguments: { action: 'list_material_functions', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'MatAuth: list_textures', toolName: 'manage_material_authoring', arguments: { action: 'list_textures', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'MatAuth: get_material_dependencies', toolName: 'manage_material_authoring', arguments: { action: 'get_material_dependencies', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_material_referencers', toolName: 'manage_material_authoring', arguments: { action: 'get_material_referencers', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: find_unused_materials', toolName: 'manage_material_authoring', arguments: { action: 'find_unused_materials', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'MatAuth: batch_update_materials', toolName: 'manage_material_authoring', arguments: { action: 'batch_update_materials', directory: TEST_FOLDER, propertyName: 'TwoSided', value: false }, expected: 'success' },
  
  // === Additional Material Expressions (Expanded Coverage) ===
  // Math Expressions
  { scenario: 'MatAuth: add_expression Sin', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Sine' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Cos', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Cosine' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Tangent', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Tangent' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ArcSine', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Arcsine' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ArcCosine', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Arccosine' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ArcTangent', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Arctangent' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression ArcTangent2', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Arctangent2' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Sqrt', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'SquareRoot' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Exp', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Exponential' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Log', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Logarithm' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Log2', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Logarithm2' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Log10', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Logarithm10' }, expected: 'success|not found' },
  
  // Vector Expressions
  { scenario: 'MatAuth: add_expression TransformVector', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'TransformVector' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression TransformPosition', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'TransformPosition' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression RotateAboutAxis', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'RotateAboutAxis' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression SphereMask', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'SphereMask' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression BoxMask', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'BoxMask' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression DistanceField', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'DistanceFieldGradient' }, expected: 'success|not found' },
  
  // Texture Sampling
  { scenario: 'MatAuth: add_expression TextureSampleNormal', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'TextureSample', textureType: 'Normal' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression VirtualTextureObject', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'VirtualTextureSample' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression RuntimeVirtualTexture', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'RuntimeVirtualTextureSample' }, expected: 'success|not found' },
  
  // Utility Expressions  
  { scenario: 'MatAuth: add_expression Comment', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Comment' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression Reroute', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'Reroute' }, expected: 'success|not found' },
  { scenario: 'MatAuth: add_expression NamedReroute', toolName: 'manage_material_authoring', arguments: { action: 'add_expression', materialPath: `${TEST_FOLDER}/M_AuthTest`, expressionType: 'NamedReroute' }, expected: 'success|not found' },
  
  // Substrate Materials
  { scenario: 'MatAuth: create_substrate_material', toolName: 'manage_material_authoring', arguments: { action: 'create_substrate_material', name: 'M_Substrate', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'MatAuth: set_substrate_properties', toolName: 'manage_material_authoring', arguments: { action: 'set_substrate_properties', materialPath: `${TEST_FOLDER}/M_Substrate`, baseColor: { r: 0.5, g: 0.5, b: 0.5 }, roughness: 0.5, metallic: 0 }, expected: 'success|not found' },
  { scenario: 'MatAuth: configure_sss_profile', toolName: 'manage_material_authoring', arguments: { action: 'configure_sss_profile', materialPath: `${TEST_FOLDER}/M_AuthTest`, profileName: 'Skin' }, expected: 'success|not found' },
  
  // Virtual Textures
  { scenario: 'MatAuth: configure_virtual_texture', toolName: 'manage_material_authoring', arguments: { action: 'configure_virtual_texture', texturePath: `${TEST_FOLDER}/T_Imported`, enabled: true, virtualTextureSize: 4096 }, expected: 'success|not found' },
  { scenario: 'MatAuth: set_streaming_priority', toolName: 'manage_material_authoring', arguments: { action: 'set_streaming_priority', texturePath: `${TEST_FOLDER}/T_Imported`, priority: 1 }, expected: 'success|not found' },
  
  // Landscape Materials
  { scenario: 'MatAuth: configure_landscape_material_layer', toolName: 'manage_material_authoring', arguments: { action: 'configure_landscape_material_layer', materialPath: `${TEST_FOLDER}/M_AuthTest`, layerName: 'Grass', blendType: 'HeightBlend' }, expected: 'success|not found' },
  { scenario: 'MatAuth: create_material_expression_template', toolName: 'manage_material_authoring', arguments: { action: 'create_material_expression_template', name: 'MET_PBR', path: TEST_FOLDER, templateType: 'PBR' }, expected: 'success|already exists' },
  
  // Batch Operations
  { scenario: 'MatAuth: create_material_instance_batch', toolName: 'manage_material_authoring', arguments: { action: 'create_material_instance_batch', parentMaterial: `${TEST_FOLDER}/M_AuthTest`, instanceNames: ['MI_Red', 'MI_Green', 'MI_Blue'], path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'MatAuth: validate_material', toolName: 'manage_material_authoring', arguments: { action: 'validate_material', materialPath: `${TEST_FOLDER}/M_AuthTest` }, expected: 'success|not found' },
  { scenario: 'MatAuth: configure_material_lod', toolName: 'manage_material_authoring', arguments: { action: 'configure_material_lod', materialPath: `${TEST_FOLDER}/M_AuthTest`, lodBias: 0, lodSettings: { qualityLevel: 'High' } }, expected: 'success|not found' },
  { scenario: 'MatAuth: export_material_template', toolName: 'manage_material_authoring', arguments: { action: 'export_material_template', materialPath: `${TEST_FOLDER}/M_AuthTest`, exportPath: 'C:/Exports/M_AuthTest.json' }, expected: 'success|not found' },
  { scenario: 'MatAuth: configure_exposure', toolName: 'manage_material_authoring', arguments: { action: 'configure_exposure', materialPath: `${TEST_FOLDER}/M_AuthTest`, exposureBias: 0, autoExposure: true }, expected: 'success|not found' },
  { scenario: 'MatAuth: get_texture_info', toolName: 'manage_material_authoring', arguments: { action: 'get_texture_info', texturePath: `${TEST_FOLDER}/T_Imported` }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_GEOMETRY (80 actions)
// ============================================================================
const manageGeometryTests = [
  // === Procedural Mesh Creation ===
  { scenario: 'Geo: create_procedural_mesh', toolName: 'manage_geometry', arguments: { action: 'create_procedural_mesh', name: 'PM_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Geo: create_box', toolName: 'manage_geometry', arguments: { action: 'create_box', actorName: 'Geo_Box', location: { x: 0, y: 0, z: 100 }, dimensions: { x: 100, y: 100, z: 100 } }, expected: 'success' },
  { scenario: 'Geo: create_sphere', toolName: 'manage_geometry', arguments: { action: 'create_sphere', actorName: 'Geo_Sphere', location: { x: 200, y: 0, z: 100 }, radius: 50 }, expected: 'success' },
  { scenario: 'Geo: create_cylinder', toolName: 'manage_geometry', arguments: { action: 'create_cylinder', actorName: 'Geo_Cylinder', location: { x: 400, y: 0, z: 100 }, radius: 50, height: 200 }, expected: 'success' },
  { scenario: 'Geo: create_cone', toolName: 'manage_geometry', arguments: { action: 'create_cone', actorName: 'Geo_Cone', location: { x: 600, y: 0, z: 100 }, radius: 50, height: 150 }, expected: 'success' },
  { scenario: 'Geo: create_capsule', toolName: 'manage_geometry', arguments: { action: 'create_capsule', actorName: 'Geo_Capsule', location: { x: 800, y: 0, z: 100 }, radius: 30, halfHeight: 100 }, expected: 'success' },
  { scenario: 'Geo: create_torus', toolName: 'manage_geometry', arguments: { action: 'create_torus', actorName: 'Geo_Torus', location: { x: 1000, y: 0, z: 100 }, majorRadius: 80, minorRadius: 20 }, expected: 'success' },
  { scenario: 'Geo: create_plane', toolName: 'manage_geometry', arguments: { action: 'create_plane', actorName: 'Geo_Plane', location: { x: 0, y: 200, z: 0 }, width: 500, height: 500 }, expected: 'success' },
  
  // === Geometry Operations ===
  { scenario: 'Geo: boolean_union', toolName: 'manage_geometry', arguments: { action: 'boolean_union', actorA: 'Geo_Box', actorB: 'Geo_Sphere', resultName: 'Geo_Union' }, expected: 'success|not found' },
  { scenario: 'Geo: boolean_subtract', toolName: 'manage_geometry', arguments: { action: 'boolean_subtract', actorA: 'Geo_Cylinder', actorB: 'Geo_Sphere', resultName: 'Geo_Subtract' }, expected: 'success|not found' },
  { scenario: 'Geo: boolean_intersect', toolName: 'manage_geometry', arguments: { action: 'boolean_intersect', actorA: 'Geo_Cone', actorB: 'Geo_Capsule', resultName: 'Geo_Intersect' }, expected: 'success|not found' },
  
  // === Mesh Deformation ===
  { scenario: 'Geo: apply_displacement', toolName: 'manage_geometry', arguments: { action: 'apply_displacement', actorName: 'Geo_Box', displacementScale: 10.0 }, expected: 'success|not found' },
  { scenario: 'Geo: apply_noise', toolName: 'manage_geometry', arguments: { action: 'apply_noise', actorName: 'Geo_Sphere', noiseScale: 5.0, noiseStrength: 20.0 }, expected: 'success|not found' },
  { scenario: 'Geo: smooth_mesh', toolName: 'manage_geometry', arguments: { action: 'smooth_mesh', actorName: 'Geo_Cylinder', iterations: 3 }, expected: 'success|not found' },
  { scenario: 'Geo: subdivide_mesh', toolName: 'manage_geometry', arguments: { action: 'subdivide_mesh', actorName: 'Geo_Cone', subdivisions: 2 }, expected: 'success|not found' },
  { scenario: 'Geo: simplify_mesh', toolName: 'manage_geometry', arguments: { action: 'simplify_mesh', actorName: 'Geo_Torus', targetTriangleCount: 1000 }, expected: 'success|not found' },
  
  // === UV Operations ===
  { scenario: 'Geo: generate_uvs_box', toolName: 'manage_geometry', arguments: { action: 'generate_uvs_box', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: generate_uvs_planar', toolName: 'manage_geometry', arguments: { action: 'generate_uvs_planar', actorName: 'Geo_Plane', projectionAxis: 'Z' }, expected: 'success|not found' },
  { scenario: 'Geo: generate_uvs_cylindrical', toolName: 'manage_geometry', arguments: { action: 'generate_uvs_cylindrical', actorName: 'Geo_Cylinder' }, expected: 'success|not found' },
  { scenario: 'Geo: unwrap_uvs', toolName: 'manage_geometry', arguments: { action: 'unwrap_uvs', actorName: 'Geo_Sphere', method: 'Angle' }, expected: 'success|not found' },
  { scenario: 'Geo: pack_uvs', toolName: 'manage_geometry', arguments: { action: 'pack_uvs', actorName: 'Geo_Box' }, expected: 'success|not found' },
  
  // === Geometry Query ===
  { scenario: 'Geo: get_vertex_count', toolName: 'manage_geometry', arguments: { action: 'get_vertex_count', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: get_triangle_count', toolName: 'manage_geometry', arguments: { action: 'get_triangle_count', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  { scenario: 'Geo: get_bounds', toolName: 'manage_geometry', arguments: { action: 'get_bounds', actorName: 'Geo_Cylinder' }, expected: 'success|not found' },
  { scenario: 'Geo: get_surface_area', toolName: 'manage_geometry', arguments: { action: 'get_surface_area', actorName: 'Geo_Cone' }, expected: 'success|not found' },
  { scenario: 'Geo: get_volume', toolName: 'manage_geometry', arguments: { action: 'get_volume', actorName: 'Geo_Capsule' }, expected: 'success|not found' },
  { scenario: 'Geo: is_watertight', toolName: 'manage_geometry', arguments: { action: 'is_watertight', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: get_mesh_info', toolName: 'manage_geometry', arguments: { action: 'get_mesh_info', actorName: 'Geo_Torus' }, expected: 'success|not found' },
  
  // === Mesh Transform ===
  { scenario: 'Geo: mirror_mesh', toolName: 'manage_geometry', arguments: { action: 'mirror_mesh', actorName: 'Geo_Box', axis: 'X' }, expected: 'success|not found' },
  { scenario: 'Geo: offset_mesh', toolName: 'manage_geometry', arguments: { action: 'offset_mesh', actorName: 'Geo_Sphere', offset: 5.0 }, expected: 'success|not found' },
  { scenario: 'Geo: scale_mesh', toolName: 'manage_geometry', arguments: { action: 'scale_mesh', actorName: 'Geo_Cylinder', scale: { x: 2, y: 1, z: 1 } }, expected: 'success|not found' },
  { scenario: 'Geo: rotate_mesh', toolName: 'manage_geometry', arguments: { action: 'rotate_mesh', actorName: 'Geo_Cone', rotation: { pitch: 0, yaw: 45, roll: 0 } }, expected: 'success|not found' },
  { scenario: 'Geo: translate_mesh', toolName: 'manage_geometry', arguments: { action: 'translate_mesh', actorName: 'Geo_Capsule', translation: { x: 100, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Geo: center_pivot', toolName: 'manage_geometry', arguments: { action: 'center_pivot', actorName: 'Geo_Torus' }, expected: 'success|not found' },
  { scenario: 'Geo: align_to_ground', toolName: 'manage_geometry', arguments: { action: 'align_to_ground', actorName: 'Geo_Box' }, expected: 'success|not found' },
  
  // === Extrusion & Sweep ===
  { scenario: 'Geo: extrude_along_spline', toolName: 'manage_geometry', arguments: { action: 'extrude_along_spline', profileActor: 'Geo_Box', splinePath: `${TEST_FOLDER}/Spline1` }, expected: 'success|not found' },
  { scenario: 'Geo: revolve_profile', toolName: 'manage_geometry', arguments: { action: 'revolve_profile', profileActor: 'Geo_Plane', axis: 'Y', degrees: 360 }, expected: 'success|not found' },
  { scenario: 'Geo: loft_profiles', toolName: 'manage_geometry', arguments: { action: 'loft_profiles', profiles: ['Geo_Box', 'Geo_Sphere'], resultName: 'Geo_Loft' }, expected: 'success|not found' },
  { scenario: 'Geo: sweep_along_path', toolName: 'manage_geometry', arguments: { action: 'sweep_along_path', crossSection: 'Geo_Box', pathActor: 'Geo_Spline' }, expected: 'success|not found' },
  
  // === Normals ===
  { scenario: 'Geo: recalculate_normals', toolName: 'manage_geometry', arguments: { action: 'recalculate_normals', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: flip_normals', toolName: 'manage_geometry', arguments: { action: 'flip_normals', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  { scenario: 'Geo: set_hard_edges', toolName: 'manage_geometry', arguments: { action: 'set_hard_edges', actorName: 'Geo_Box', angle: 60.0 }, expected: 'success|not found' },
  { scenario: 'Geo: set_soft_edges', toolName: 'manage_geometry', arguments: { action: 'set_soft_edges', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  
  // === Mesh Merge/Split ===
  { scenario: 'Geo: merge_meshes', toolName: 'manage_geometry', arguments: { action: 'merge_meshes', actors: ['Geo_Box', 'Geo_Sphere'], resultName: 'Geo_Merged' }, expected: 'success|not found' },
  { scenario: 'Geo: split_by_material', toolName: 'manage_geometry', arguments: { action: 'split_by_material', actorName: 'Geo_Merged' }, expected: 'success|not found' },
  { scenario: 'Geo: separate_components', toolName: 'manage_geometry', arguments: { action: 'separate_components', actorName: 'Geo_Merged' }, expected: 'success|not found' },
  
  // === LOD Generation ===
  { scenario: 'Geo: generate_lods', toolName: 'manage_geometry', arguments: { action: 'generate_lods', actorName: 'Geo_Box', lodCount: 3 }, expected: 'success|not found' },
  { scenario: 'Geo: set_lod_distance', toolName: 'manage_geometry', arguments: { action: 'set_lod_distance', actorName: 'Geo_Box', lodIndex: 1, distance: 1000.0 }, expected: 'success|not found' },
  { scenario: 'Geo: configure_lod_screen_size', toolName: 'manage_geometry', arguments: { action: 'configure_lod_screen_size', actorName: 'Geo_Box', lodIndex: 0, screenSize: 0.5 }, expected: 'success|not found' },
  
  // === Collision ===
  { scenario: 'Geo: generate_collision_simple', toolName: 'manage_geometry', arguments: { action: 'generate_collision_simple', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: generate_collision_convex', toolName: 'manage_geometry', arguments: { action: 'generate_collision_convex', actorName: 'Geo_Sphere', hullCount: 16 }, expected: 'success|not found' },
  { scenario: 'Geo: generate_collision_complex', toolName: 'manage_geometry', arguments: { action: 'generate_collision_complex', actorName: 'Geo_Cylinder' }, expected: 'success|not found' },
  { scenario: 'Geo: remove_collision', toolName: 'manage_geometry', arguments: { action: 'remove_collision', actorName: 'Geo_Cone' }, expected: 'success|not found' },
  
  // === Static Mesh Conversion ===
  { scenario: 'Geo: convert_to_static_mesh', toolName: 'manage_geometry', arguments: { action: 'convert_to_static_mesh', actorName: 'Geo_Box', assetPath: `${TEST_FOLDER}/SM_FromGeo` }, expected: 'success|not found' },
  { scenario: 'Geo: convert_to_volume', toolName: 'manage_geometry', arguments: { action: 'convert_to_volume', actorName: 'Geo_Sphere', volumeType: 'BlockingVolume' }, expected: 'success|not found' },
  
  // === Spline Mesh ===
  { scenario: 'Geo: create_spline_mesh', toolName: 'manage_geometry', arguments: { action: 'create_spline_mesh', actorName: 'Geo_SplineMesh', meshPath: '/Engine/BasicShapes/Cylinder', splineActor: 'Geo_Spline' }, expected: 'success|not found' },
  { scenario: 'Geo: configure_spline_mesh', toolName: 'manage_geometry', arguments: { action: 'configure_spline_mesh', actorName: 'Geo_SplineMesh', forwardAxis: 'X', startScale: 1.0, endScale: 0.5 }, expected: 'success|not found' },
  
  // === HISM/ISM ===
  { scenario: 'Geo: create_instanced_static_mesh', toolName: 'manage_geometry', arguments: { action: 'create_instanced_static_mesh', actorName: 'Geo_ISM', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Geo: add_instance', toolName: 'manage_geometry', arguments: { action: 'add_instance', actorName: 'Geo_ISM', transform: { location: { x: 0, y: 0, z: 0 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, scale: { x: 1, y: 1, z: 1 } } }, expected: 'success|not found' },
  { scenario: 'Geo: batch_add_instances', toolName: 'manage_geometry', arguments: { action: 'batch_add_instances', actorName: 'Geo_ISM', transforms: [{ location: { x: 100, y: 0, z: 0 } }, { location: { x: 200, y: 0, z: 0 } }] }, expected: 'success|not found' },
  { scenario: 'Geo: remove_instance', toolName: 'manage_geometry', arguments: { action: 'remove_instance', actorName: 'Geo_ISM', instanceIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Geo: get_instance_count', toolName: 'manage_geometry', arguments: { action: 'get_instance_count', actorName: 'Geo_ISM' }, expected: 'success|not found' },
  { scenario: 'Geo: set_instance_transform', toolName: 'manage_geometry', arguments: { action: 'set_instance_transform', actorName: 'Geo_ISM', instanceIndex: 1, transform: { location: { x: 300, y: 0, z: 0 } } }, expected: 'success|not found' },
  
  // === Vertex Colors ===
  { scenario: 'Geo: paint_vertex_colors', toolName: 'manage_geometry', arguments: { action: 'paint_vertex_colors', actorName: 'Geo_Box', color: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Geo: clear_vertex_colors', toolName: 'manage_geometry', arguments: { action: 'clear_vertex_colors', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: bake_vertex_colors', toolName: 'manage_geometry', arguments: { action: 'bake_vertex_colors', actorName: 'Geo_Box' }, expected: 'success|not found' },
  
  // === Geometry Script ===
  { scenario: 'Geo: create_geometry_script', toolName: 'manage_geometry', arguments: { action: 'create_geometry_script', name: 'GS_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Geo: execute_geometry_script', toolName: 'manage_geometry', arguments: { action: 'execute_geometry_script', scriptPath: `${TEST_FOLDER}/GS_Test`, targetActor: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Transform', nodeName: 'Scale' }, expected: 'success|not found' },
  { scenario: 'Geo: connect_script_nodes', toolName: 'manage_geometry', arguments: { action: 'connect_script_nodes', scriptPath: `${TEST_FOLDER}/GS_Test`, fromNode: 'Input', toNode: 'Scale' }, expected: 'success|not found' },
  
  // === Additional Geometry Script Nodes ===
  { scenario: 'Geo: add_script_node_subdivide', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Subdivide', nodeName: 'SubdivideOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_simplify', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Simplify', nodeName: 'SimplifyOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_boolean', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Boolean', nodeName: 'BooleanOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_extrude', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Extrude', nodeName: 'ExtrudeOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_inset', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Inset', nodeName: 'InsetOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_bevel', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Bevel', nodeName: 'BevelOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_weld', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Weld', nodeName: 'WeldOp' }, expected: 'success|not found' },
  { scenario: 'Geo: add_script_node_triangulate', toolName: 'manage_geometry', arguments: { action: 'add_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeType: 'Triangulate', nodeName: 'TriangulateOp' }, expected: 'success|not found' },
  { scenario: 'Geo: remove_script_node', toolName: 'manage_geometry', arguments: { action: 'remove_script_node', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeName: 'TriangulateOp' }, expected: 'success|not found' },
  { scenario: 'Geo: disconnect_script_nodes', toolName: 'manage_geometry', arguments: { action: 'disconnect_script_nodes', scriptPath: `${TEST_FOLDER}/GS_Test`, fromNode: 'Input', toNode: 'Scale' }, expected: 'success|not found' },
  { scenario: 'Geo: set_script_node_property', toolName: 'manage_geometry', arguments: { action: 'set_script_node_property', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeName: 'Scale', propertyName: 'ScaleFactor', value: 2.0 }, expected: 'success|not found' },
  { scenario: 'Geo: get_script_node_property', toolName: 'manage_geometry', arguments: { action: 'get_script_node_property', scriptPath: `${TEST_FOLDER}/GS_Test`, nodeName: 'Scale', propertyName: 'ScaleFactor' }, expected: 'success|not found' },
  { scenario: 'Geo: list_script_nodes', toolName: 'manage_geometry', arguments: { action: 'list_script_nodes', scriptPath: `${TEST_FOLDER}/GS_Test` }, expected: 'success|not found' },
  { scenario: 'Geo: duplicate_geometry_script', toolName: 'manage_geometry', arguments: { action: 'duplicate_geometry_script', sourcePath: `${TEST_FOLDER}/GS_Test`, destPath: TEST_FOLDER, newName: 'GS_TestCopy' }, expected: 'success|not found' },
  
  // === Additional Primitive Shapes ===
  { scenario: 'Geo: create_pyramid', toolName: 'manage_geometry', arguments: { action: 'create_pyramid', actorName: 'Geo_Pyramid', location: { x: 1200, y: 0, z: 100 }, baseSize: 100, height: 150 }, expected: 'success' },
  { scenario: 'Geo: create_wedge', toolName: 'manage_geometry', arguments: { action: 'create_wedge', actorName: 'Geo_Wedge', location: { x: 1400, y: 0, z: 100 }, dimensions: { x: 100, y: 50, z: 100 } }, expected: 'success' },
  { scenario: 'Geo: create_stairs', toolName: 'manage_geometry', arguments: { action: 'create_stairs', actorName: 'Geo_Stairs', location: { x: 1600, y: 0, z: 0 }, stepCount: 10, stepWidth: 100, stepHeight: 20, stepDepth: 30 }, expected: 'success' },
  { scenario: 'Geo: create_spiral_stairs', toolName: 'manage_geometry', arguments: { action: 'create_spiral_stairs', actorName: 'Geo_SpiralStairs', location: { x: 1800, y: 0, z: 0 }, innerRadius: 50, outerRadius: 150, stepCount: 20 }, expected: 'success' },
  { scenario: 'Geo: create_arch', toolName: 'manage_geometry', arguments: { action: 'create_arch', actorName: 'Geo_Arch', location: { x: 2000, y: 0, z: 100 }, radius: 100, thickness: 20, segments: 16 }, expected: 'success' },
  { scenario: 'Geo: create_helix', toolName: 'manage_geometry', arguments: { action: 'create_helix', actorName: 'Geo_Helix', location: { x: 2200, y: 0, z: 0 }, radius: 50, height: 200, turns: 3 }, expected: 'success' },
  { scenario: 'Geo: create_disc', toolName: 'manage_geometry', arguments: { action: 'create_disc', actorName: 'Geo_Disc', location: { x: 2400, y: 0, z: 0 }, radius: 100, segments: 32 }, expected: 'success' },
  { scenario: 'Geo: create_ring', toolName: 'manage_geometry', arguments: { action: 'create_ring', actorName: 'Geo_Ring', location: { x: 2600, y: 0, z: 100 }, innerRadius: 50, outerRadius: 100 }, expected: 'success' },
  
  // === Additional Mesh Operations ===
  { scenario: 'Geo: weld_vertices', toolName: 'manage_geometry', arguments: { action: 'weld_vertices', actorName: 'Geo_Box', threshold: 0.01 }, expected: 'success|not found' },
  { scenario: 'Geo: remove_duplicate_faces', toolName: 'manage_geometry', arguments: { action: 'remove_duplicate_faces', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: remove_degenerate_faces', toolName: 'manage_geometry', arguments: { action: 'remove_degenerate_faces', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  { scenario: 'Geo: fill_holes', toolName: 'manage_geometry', arguments: { action: 'fill_holes', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: repair_mesh', toolName: 'manage_geometry', arguments: { action: 'repair_mesh', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  { scenario: 'Geo: make_solid', toolName: 'manage_geometry', arguments: { action: 'make_solid', actorName: 'Geo_Box', voxelResolution: 64 }, expected: 'success|not found' },
  { scenario: 'Geo: shell_mesh', toolName: 'manage_geometry', arguments: { action: 'shell_mesh', actorName: 'Geo_Sphere', thickness: 5 }, expected: 'success|not found' },
  { scenario: 'Geo: hollow_mesh', toolName: 'manage_geometry', arguments: { action: 'hollow_mesh', actorName: 'Geo_Box', wallThickness: 10 }, expected: 'success|not found' },
  { scenario: 'Geo: thicken_mesh', toolName: 'manage_geometry', arguments: { action: 'thicken_mesh', actorName: 'Geo_Plane', thickness: 5 }, expected: 'success|not found' },
  
  // === UV Channel Operations ===
  { scenario: 'Geo: add_uv_channel', toolName: 'manage_geometry', arguments: { action: 'add_uv_channel', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: remove_uv_channel', toolName: 'manage_geometry', arguments: { action: 'remove_uv_channel', actorName: 'Geo_Box', channelIndex: 1 }, expected: 'success|not found' },
  { scenario: 'Geo: copy_uv_channel', toolName: 'manage_geometry', arguments: { action: 'copy_uv_channel', actorName: 'Geo_Box', sourceChannel: 0, targetChannel: 1 }, expected: 'success|not found' },
  { scenario: 'Geo: transform_uvs', toolName: 'manage_geometry', arguments: { action: 'transform_uvs', actorName: 'Geo_Box', scale: { x: 2, y: 2 }, offset: { x: 0.5, y: 0 } }, expected: 'success|not found' },
  { scenario: 'Geo: get_uv_island_count', toolName: 'manage_geometry', arguments: { action: 'get_uv_island_count', actorName: 'Geo_Box' }, expected: 'success|not found' },
  
  // === Mesh Selection Operations ===
  { scenario: 'Geo: select_by_material', toolName: 'manage_geometry', arguments: { action: 'select_by_material', actorName: 'Geo_Box', materialIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Geo: select_by_normal', toolName: 'manage_geometry', arguments: { action: 'select_by_normal', actorName: 'Geo_Box', normalDirection: { x: 0, y: 0, z: 1 }, angleTolerance: 45 }, expected: 'success|not found' },
  { scenario: 'Geo: grow_selection', toolName: 'manage_geometry', arguments: { action: 'grow_selection', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: shrink_selection', toolName: 'manage_geometry', arguments: { action: 'shrink_selection', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: invert_selection', toolName: 'manage_geometry', arguments: { action: 'invert_selection', actorName: 'Geo_Box' }, expected: 'success|not found' },
  
  // === Mesh Info & Stats ===
  { scenario: 'Geo: get_edge_count', toolName: 'manage_geometry', arguments: { action: 'get_edge_count', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: get_face_count', toolName: 'manage_geometry', arguments: { action: 'get_face_count', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  { scenario: 'Geo: get_material_count', toolName: 'manage_geometry', arguments: { action: 'get_material_count', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: get_uv_channel_count', toolName: 'manage_geometry', arguments: { action: 'get_uv_channel_count', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: has_vertex_colors', toolName: 'manage_geometry', arguments: { action: 'has_vertex_colors', actorName: 'Geo_Box' }, expected: 'success|not found' },
  { scenario: 'Geo: get_centroid', toolName: 'manage_geometry', arguments: { action: 'get_centroid', actorName: 'Geo_Sphere' }, expected: 'success|not found' },
  
  // === Mesh Export/Import ===
  { scenario: 'Geo: export_mesh_obj', toolName: 'manage_geometry', arguments: { action: 'export_mesh', actorName: 'Geo_Box', exportPath: 'C:/temp/geo_box.obj', format: 'OBJ' }, expected: 'success|not found' },
  { scenario: 'Geo: export_mesh_fbx', toolName: 'manage_geometry', arguments: { action: 'export_mesh', actorName: 'Geo_Sphere', exportPath: 'C:/temp/geo_sphere.fbx', format: 'FBX' }, expected: 'success|not found' },
  { scenario: 'Geo: export_mesh_stl', toolName: 'manage_geometry', arguments: { action: 'export_mesh', actorName: 'Geo_Box', exportPath: 'C:/temp/geo_box.stl', format: 'STL' }, expected: 'success|not found' },
  { scenario: 'Geo: import_mesh_obj', toolName: 'manage_geometry', arguments: { action: 'import_mesh', sourcePath: 'C:/temp/imported.obj', actorName: 'Geo_Imported' }, expected: 'success|file not found' },
  
  // === HISM/ISM Extended ===
  { scenario: 'Geo: create_hierarchical_ism', toolName: 'manage_geometry', arguments: { action: 'create_hierarchical_ism', actorName: 'Geo_HISM', meshPath: '/Engine/BasicShapes/Cube' }, expected: 'success' },
  { scenario: 'Geo: set_ism_cull_distance', toolName: 'manage_geometry', arguments: { action: 'set_ism_cull_distance', actorName: 'Geo_ISM', minDistance: 0, maxDistance: 5000 }, expected: 'success|not found' },
  { scenario: 'Geo: set_ism_lod_distances', toolName: 'manage_geometry', arguments: { action: 'set_ism_lod_distances', actorName: 'Geo_ISM', lodDistances: [1000, 2000, 3000] }, expected: 'success|not found' },
  { scenario: 'Geo: update_instance_transform', toolName: 'manage_geometry', arguments: { action: 'update_instance_transform', actorName: 'Geo_ISM', instanceIndex: 0, worldSpace: true, transform: { location: { x: 100, y: 100, z: 0 } } }, expected: 'success|not found' },
  { scenario: 'Geo: get_instance_transform', toolName: 'manage_geometry', arguments: { action: 'get_instance_transform', actorName: 'Geo_ISM', instanceIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Geo: set_instance_custom_data', toolName: 'manage_geometry', arguments: { action: 'set_instance_custom_data', actorName: 'Geo_ISM', instanceIndex: 0, customData: [1.0, 0.5, 0.0, 1.0] }, expected: 'success|not found' },
  { scenario: 'Geo: clear_all_instances', toolName: 'manage_geometry', arguments: { action: 'clear_all_instances', actorName: 'Geo_ISM' }, expected: 'success|not found' },
  
  // === Geometry Utilities ===
  { scenario: 'Geo: list_geometry_actors', toolName: 'manage_geometry', arguments: { action: 'list_geometry_actors' }, expected: 'success' },
  { scenario: 'Geo: list_geometry_scripts', toolName: 'manage_geometry', arguments: { action: 'list_geometry_scripts', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Geo: get_geometry_info', toolName: 'manage_geometry', arguments: { action: 'get_geometry_info' }, expected: 'success' },
  
  // === Additional Geometry Operations (Expanded Coverage) ===
  // Boolean Operations Variations
  { scenario: 'Geo: boolean_union_multibox', toolName: 'manage_geometry', arguments: { action: 'boolean_union', actorA: 'Geo_Box', actorB: 'Geo_Cylinder', resultName: 'Geo_UnionBoxCyl' }, expected: 'success|not found' },
  { scenario: 'Geo: boolean_subtract_cone', toolName: 'manage_geometry', arguments: { action: 'boolean_subtract', actorA: 'Geo_Box', actorB: 'Geo_Cone', resultName: 'Geo_SubtractCone' }, expected: 'success|not found' },
  { scenario: 'Geo: boolean_intersect_torus', toolName: 'manage_geometry', arguments: { action: 'boolean_intersect', actorA: 'Geo_Sphere', actorB: 'Geo_Torus', resultName: 'Geo_IntersectTorus' }, expected: 'success|not found' },
  
  // Procedural Mesh Conversion
  { scenario: 'Geo: create_procedural_box', toolName: 'manage_geometry', arguments: { action: 'create_procedural_box', name: 'PM_Box', path: TEST_FOLDER, dimensions: { x: 100, y: 100, z: 100 } }, expected: 'success|already exists' },
  { scenario: 'Geo: boolean_mesh_operation', toolName: 'manage_geometry', arguments: { action: 'boolean_mesh_operation', meshA: `${TEST_FOLDER}/PM_Box`, meshB: `${TEST_FOLDER}/PM_Test`, operation: 'Union' }, expected: 'success|not found' },
  { scenario: 'Geo: generate_mesh_uvs', toolName: 'manage_geometry', arguments: { action: 'generate_mesh_uvs', meshPath: `${TEST_FOLDER}/PM_Box`, projectionType: 'Box' }, expected: 'success|not found' },
  { scenario: 'Geo: create_mesh_from_spline', toolName: 'manage_geometry', arguments: { action: 'create_mesh_from_spline', splinePath: `${TEST_FOLDER}/Spline1`, width: 50, depth: 10 }, expected: 'success|not found' },
  { scenario: 'Geo: configure_nanite_settings', toolName: 'manage_geometry', arguments: { action: 'configure_nanite_settings', meshPath: `${TEST_FOLDER}/PM_Box`, enabled: true, fallbackRelativeError: 0.01 }, expected: 'success|not found' },
  { scenario: 'Geo: export_geometry_to_file', toolName: 'manage_geometry', arguments: { action: 'export_geometry_to_file', actorName: 'Geo_Box', filePath: 'C:/temp/exported.obj', format: 'OBJ' }, expected: 'success|not found' },
  
  // Additional Primitive Shapes
  { scenario: 'Geo: create_prism', toolName: 'manage_geometry', arguments: { action: 'create_prism', actorName: 'Geo_Prism', location: { x: 2800, y: 0, z: 100 }, sides: 6, radius: 50, height: 100 }, expected: 'success' },
  { scenario: 'Geo: create_tube', toolName: 'manage_geometry', arguments: { action: 'create_tube', actorName: 'Geo_Tube', location: { x: 3000, y: 0, z: 100 }, innerRadius: 30, outerRadius: 50, height: 200 }, expected: 'success' },
  { scenario: 'Geo: create_icosphere', toolName: 'manage_geometry', arguments: { action: 'create_icosphere', actorName: 'Geo_Icosphere', location: { x: 3200, y: 0, z: 100 }, radius: 50, subdivisions: 3 }, expected: 'success' },
  { scenario: 'Geo: create_uvsphere', toolName: 'manage_geometry', arguments: { action: 'create_uvsphere', actorName: 'Geo_UVSphere', location: { x: 3400, y: 0, z: 100 }, radius: 50, segments: 24, rings: 12 }, expected: 'success' },
];

// ============================================================================
// MANAGE_SKELETON (54 actions)
// ============================================================================
const manageSkeletonTests = [
  // === Skeletal Mesh Basics ===
  { scenario: 'Skel: list_skeletal_meshes', toolName: 'manage_skeleton', arguments: { action: 'list_skeletal_meshes', directory: '/Game' }, expected: 'success' },
  { scenario: 'Skel: get_skeleton_info', toolName: 'manage_skeleton', arguments: { action: 'get_skeleton_info', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'Skel: get_bone_hierarchy', toolName: 'manage_skeleton', arguments: { action: 'get_bone_hierarchy', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'Skel: get_bone_count', toolName: 'manage_skeleton', arguments: { action: 'get_bone_count', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube' }, expected: 'success|not found' },
  { scenario: 'Skel: find_bone_by_name', toolName: 'manage_skeleton', arguments: { action: 'find_bone_by_name', skeletalMeshPath: '/Engine/EngineMeshes/SkeletalCube', boneName: 'root' }, expected: 'success|not found' },
  
  // === Socket Operations ===
  { scenario: 'Skel: add_socket', toolName: 'manage_skeleton', arguments: { action: 'add_socket', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'root', socketName: 'WeaponSocket' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_socket', toolName: 'manage_skeleton', arguments: { action: 'remove_socket', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, socketName: 'WeaponSocket' }, expected: 'success|not found' },
  { scenario: 'Skel: list_sockets', toolName: 'manage_skeleton', arguments: { action: 'list_sockets', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: set_socket_transform', toolName: 'manage_skeleton', arguments: { action: 'set_socket_transform', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, socketName: 'WeaponSocket', location: { x: 0, y: 0, z: 10 }, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'success|not found' },
  { scenario: 'Skel: get_socket_transform', toolName: 'manage_skeleton', arguments: { action: 'get_socket_transform', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, socketName: 'WeaponSocket' }, expected: 'success|not found' },
  
  // === Physics Asset ===
  { scenario: 'Skel: create_physics_asset', toolName: 'manage_skeleton', arguments: { action: 'create_physics_asset', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, name: 'PHYS_Test' }, expected: 'success|not found' },
  { scenario: 'Skel: add_physics_body', toolName: 'manage_skeleton', arguments: { action: 'add_physics_body', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'root', bodyType: 'Capsule' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_physics_body', toolName: 'manage_skeleton', arguments: { action: 'remove_physics_body', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'root' }, expected: 'success|not found' },
  { scenario: 'Skel: add_physics_constraint', toolName: 'manage_skeleton', arguments: { action: 'add_physics_constraint', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'Skel: configure_physics_constraint', toolName: 'manage_skeleton', arguments: { action: 'configure_physics_constraint', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01', swingLimit: 45, twistLimit: 30 }, expected: 'success|not found' },
  { scenario: 'Skel: auto_generate_physics', toolName: 'manage_skeleton', arguments: { action: 'auto_generate_physics', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, minBoneSize: 5.0 }, expected: 'success|not found' },
  { scenario: 'Skel: validate_physics_asset', toolName: 'manage_skeleton', arguments: { action: 'validate_physics_asset', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test` }, expected: 'success|not found' },
  
  // === LODs ===
  { scenario: 'Skel: generate_lods', toolName: 'manage_skeleton', arguments: { action: 'generate_lods', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, lodCount: 3 }, expected: 'success|not found' },
  { scenario: 'Skel: set_lod_settings', toolName: 'manage_skeleton', arguments: { action: 'set_lod_settings', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, lodIndex: 1, screenSize: 0.5, trianglePercent: 50 }, expected: 'success|not found' },
  { scenario: 'Skel: remove_lod', toolName: 'manage_skeleton', arguments: { action: 'remove_lod', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, lodIndex: 2 }, expected: 'success|not found' },
  
  // === Materials ===
  { scenario: 'Skel: get_materials', toolName: 'manage_skeleton', arguments: { action: 'get_materials', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: set_material', toolName: 'manage_skeleton', arguments: { action: 'set_material', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, slotIndex: 0, materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Skel: add_material_slot', toolName: 'manage_skeleton', arguments: { action: 'add_material_slot', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, slotName: 'NewSlot' }, expected: 'success|not found' },
  
  // === Morph Targets ===
  { scenario: 'Skel: list_morph_targets', toolName: 'manage_skeleton', arguments: { action: 'list_morph_targets', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: remove_morph_target', toolName: 'manage_skeleton', arguments: { action: 'remove_morph_target', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, morphTargetName: 'Smile' }, expected: 'success|not found' },
  { scenario: 'Skel: rename_morph_target', toolName: 'manage_skeleton', arguments: { action: 'rename_morph_target', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, oldName: 'Smile', newName: 'Smile_Full' }, expected: 'success|not found' },
  
  // === Cloth Simulation ===
  { scenario: 'Skel: create_cloth_config', toolName: 'manage_skeleton', arguments: { action: 'create_cloth_config', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape' }, expected: 'success|not found' },
  { scenario: 'Skel: configure_cloth', toolName: 'manage_skeleton', arguments: { action: 'configure_cloth', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', massScale: 1.0, stiffness: 0.8, damping: 0.1 }, expected: 'success|not found' },
  { scenario: 'Skel: paint_cloth_weights', toolName: 'manage_skeleton', arguments: { action: 'paint_cloth_weights', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', weight: 0.5 }, expected: 'success|not found' },
  { scenario: 'Skel: remove_cloth', toolName: 'manage_skeleton', arguments: { action: 'remove_cloth', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape' }, expected: 'success|not found' },
  
  // === Virtual Bones ===
  { scenario: 'Skel: add_virtual_bone', toolName: 'manage_skeleton', arguments: { action: 'add_virtual_bone', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, sourceBone: 'hand_r', targetBone: 'hand_l', virtualBoneName: 'VB_HandMid' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_virtual_bone', toolName: 'manage_skeleton', arguments: { action: 'remove_virtual_bone', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, virtualBoneName: 'VB_HandMid' }, expected: 'success|not found' },
  { scenario: 'Skel: list_virtual_bones', toolName: 'manage_skeleton', arguments: { action: 'list_virtual_bones', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  
  // === Retargeting ===
  { scenario: 'Skel: create_ik_rig', toolName: 'manage_skeleton', arguments: { action: 'create_ik_rig', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, name: 'IK_Test' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_chain', toolName: 'manage_skeleton', arguments: { action: 'add_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'LeftArm', startBone: 'clavicle_l', endBone: 'hand_l' }, expected: 'success|not found' },
  { scenario: 'Skel: create_ik_retargeter', toolName: 'manage_skeleton', arguments: { action: 'create_ik_retargeter', sourceIKRig: `${TEST_FOLDER}/IK_Source`, targetIKRig: `${TEST_FOLDER}/IK_Target`, name: 'RTG_Test' }, expected: 'success|not found' },
  { scenario: 'Skel: configure_retarget_chain', toolName: 'manage_skeleton', arguments: { action: 'configure_retarget_chain', retargeterPath: `${TEST_FOLDER}/RTG_Test`, chainName: 'LeftArm', translationMode: 'Additive' }, expected: 'success|not found' },
  
  // === Import/Export ===
  { scenario: 'Skel: import_skeletal_mesh', toolName: 'manage_skeleton', arguments: { action: 'import_skeletal_mesh', sourcePath: 'C:/temp/character.fbx', destPath: TEST_FOLDER, name: 'SK_Imported' }, expected: 'success|file not found' },
  { scenario: 'Skel: reimport_skeletal_mesh', toolName: 'manage_skeleton', arguments: { action: 'reimport_skeletal_mesh', skeletalMeshPath: `${TEST_FOLDER}/SK_Imported` }, expected: 'success|not found' },
  { scenario: 'Skel: export_skeletal_mesh', toolName: 'manage_skeleton', arguments: { action: 'export_skeletal_mesh', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, exportPath: 'C:/temp/exported.fbx' }, expected: 'success|not found' },
  
  // === Animation Slots ===
  { scenario: 'Skel: add_anim_slot', toolName: 'manage_skeleton', arguments: { action: 'add_anim_slot', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton`, groupName: 'UpperBody', slotName: 'Attack' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_anim_slot', toolName: 'manage_skeleton', arguments: { action: 'remove_anim_slot', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton`, slotName: 'Attack' }, expected: 'success|not found' },
  { scenario: 'Skel: list_anim_slots', toolName: 'manage_skeleton', arguments: { action: 'list_anim_slots', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton` }, expected: 'success|not found' },
  
  // === Bone Curves ===
  { scenario: 'Skel: add_bone_curve', toolName: 'manage_skeleton', arguments: { action: 'add_bone_curve', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton`, curveName: 'BreathingScale' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_bone_curve', toolName: 'manage_skeleton', arguments: { action: 'remove_bone_curve', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton`, curveName: 'BreathingScale' }, expected: 'success|not found' },
  { scenario: 'Skel: list_bone_curves', toolName: 'manage_skeleton', arguments: { action: 'list_bone_curves', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton` }, expected: 'success|not found' },
  
  // === Pose Assets ===
  { scenario: 'Skel: create_pose_asset', toolName: 'manage_skeleton', arguments: { action: 'create_pose_asset', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, name: 'Pose_TPose', path: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Skel: add_pose_to_asset', toolName: 'manage_skeleton', arguments: { action: 'add_pose_to_asset', poseAssetPath: `${TEST_FOLDER}/Pose_TPose`, poseName: 'Idle' }, expected: 'success|not found' },
  { scenario: 'Skel: add_pose_to_asset_combat', toolName: 'manage_skeleton', arguments: { action: 'add_pose_to_asset', poseAssetPath: `${TEST_FOLDER}/Pose_TPose`, poseName: 'Combat' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_pose_from_asset', toolName: 'manage_skeleton', arguments: { action: 'remove_pose_from_asset', poseAssetPath: `${TEST_FOLDER}/Pose_TPose`, poseName: 'Combat' }, expected: 'success|not found' },
  { scenario: 'Skel: list_poses_in_asset', toolName: 'manage_skeleton', arguments: { action: 'list_poses_in_asset', poseAssetPath: `${TEST_FOLDER}/Pose_TPose` }, expected: 'success|not found' },
  { scenario: 'Skel: rename_pose_in_asset', toolName: 'manage_skeleton', arguments: { action: 'rename_pose_in_asset', poseAssetPath: `${TEST_FOLDER}/Pose_TPose`, oldName: 'Idle', newName: 'IdlePose' }, expected: 'success|not found' },
  
  // === Media Textures ===
  { scenario: 'Skel: create_media_texture', toolName: 'manage_skeleton', arguments: { action: 'create_media_texture', name: 'MT_Video', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Skel: create_media_player', toolName: 'manage_skeleton', arguments: { action: 'create_media_player', name: 'MP_Video', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Skel: configure_media_source', toolName: 'manage_skeleton', arguments: { action: 'configure_media_source', mediaPlayerPath: `${TEST_FOLDER}/MP_Video`, sourceUrl: 'C:/temp/video.mp4' }, expected: 'success|not found' },
  { scenario: 'Skel: configure_media_source_stream', toolName: 'manage_skeleton', arguments: { action: 'configure_media_source', mediaPlayerPath: `${TEST_FOLDER}/MP_Video`, sourceUrl: 'rtsp://localhost:8554/stream' }, expected: 'success|not found' },
  { scenario: 'Skel: link_media_texture_to_player', toolName: 'manage_skeleton', arguments: { action: 'link_media_texture_to_player', texturePath: `${TEST_FOLDER}/MT_Video`, playerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  { scenario: 'Skel: play_media', toolName: 'manage_skeleton', arguments: { action: 'play_media', mediaPlayerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  { scenario: 'Skel: pause_media', toolName: 'manage_skeleton', arguments: { action: 'pause_media', mediaPlayerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  { scenario: 'Skel: stop_media', toolName: 'manage_skeleton', arguments: { action: 'stop_media', mediaPlayerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  { scenario: 'Skel: seek_media', toolName: 'manage_skeleton', arguments: { action: 'seek_media', mediaPlayerPath: `${TEST_FOLDER}/MP_Video`, time: 10.0 }, expected: 'success|not found' },
  { scenario: 'Skel: set_media_looping', toolName: 'manage_skeleton', arguments: { action: 'set_media_looping', mediaPlayerPath: `${TEST_FOLDER}/MP_Video`, looping: true }, expected: 'success|not found' },
  { scenario: 'Skel: get_media_duration', toolName: 'manage_skeleton', arguments: { action: 'get_media_duration', mediaPlayerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  { scenario: 'Skel: get_media_time', toolName: 'manage_skeleton', arguments: { action: 'get_media_time', mediaPlayerPath: `${TEST_FOLDER}/MP_Video` }, expected: 'success|not found' },
  
  // === Additional Bone Operations ===
  { scenario: 'Skel: get_bone_transform', toolName: 'manage_skeleton', arguments: { action: 'get_bone_transform', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'root' }, expected: 'success|not found' },
  { scenario: 'Skel: get_bone_index', toolName: 'manage_skeleton', arguments: { action: 'get_bone_index', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'pelvis' }, expected: 'success|not found' },
  { scenario: 'Skel: get_parent_bone', toolName: 'manage_skeleton', arguments: { action: 'get_parent_bone', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'Skel: get_child_bones', toolName: 'manage_skeleton', arguments: { action: 'get_child_bones', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'pelvis' }, expected: 'success|not found' },
  { scenario: 'Skel: is_bone_weighted', toolName: 'manage_skeleton', arguments: { action: 'is_bone_weighted', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'hand_r' }, expected: 'success|not found' },
  { scenario: 'Skel: get_bone_length', toolName: 'manage_skeleton', arguments: { action: 'get_bone_length', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, boneName: 'upperarm_r' }, expected: 'success|not found' },
  
  // === Additional Physics Asset Operations ===
  { scenario: 'Skel: get_physics_body_info', toolName: 'manage_skeleton', arguments: { action: 'get_physics_body_info', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'pelvis' }, expected: 'success|not found' },
  { scenario: 'Skel: set_physics_body_type', toolName: 'manage_skeleton', arguments: { action: 'set_physics_body_type', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'pelvis', bodyType: 'Sphere' }, expected: 'success|not found' },
  { scenario: 'Skel: set_physics_body_mass', toolName: 'manage_skeleton', arguments: { action: 'set_physics_body_mass', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'pelvis', mass: 10.0 }, expected: 'success|not found' },
  { scenario: 'Skel: set_physics_body_collision', toolName: 'manage_skeleton', arguments: { action: 'set_physics_body_collision', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, boneName: 'pelvis', collisionEnabled: true }, expected: 'success|not found' },
  { scenario: 'Skel: copy_physics_body', toolName: 'manage_skeleton', arguments: { action: 'copy_physics_body', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, sourceBone: 'pelvis', targetBone: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_physics_constraint', toolName: 'manage_skeleton', arguments: { action: 'remove_physics_constraint', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'Skel: get_physics_constraint_info', toolName: 'manage_skeleton', arguments: { action: 'get_physics_constraint_info', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01' }, expected: 'success|not found' },
  { scenario: 'Skel: set_constraint_swing_limit', toolName: 'manage_skeleton', arguments: { action: 'set_constraint_swing_limit', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01', swing1Limit: 45, swing2Limit: 45 }, expected: 'success|not found' },
  { scenario: 'Skel: set_constraint_twist_limit', toolName: 'manage_skeleton', arguments: { action: 'set_constraint_twist_limit', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01', twistLimit: 30 }, expected: 'success|not found' },
  { scenario: 'Skel: set_constraint_drive', toolName: 'manage_skeleton', arguments: { action: 'set_constraint_drive', physicsAssetPath: `${TEST_FOLDER}/PHYS_Test`, bone1: 'pelvis', bone2: 'spine_01', driveType: 'Angular', spring: 100, damping: 10 }, expected: 'success|not found' },
  
  // === Additional IK Rig Operations ===
  { scenario: 'Skel: add_ik_chain_rightarm', toolName: 'manage_skeleton', arguments: { action: 'add_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'RightArm', startBone: 'clavicle_r', endBone: 'hand_r' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_chain_leftleg', toolName: 'manage_skeleton', arguments: { action: 'add_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'LeftLeg', startBone: 'thigh_l', endBone: 'foot_l' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_chain_rightleg', toolName: 'manage_skeleton', arguments: { action: 'add_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'RightLeg', startBone: 'thigh_r', endBone: 'foot_r' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_chain_spine', toolName: 'manage_skeleton', arguments: { action: 'add_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'Spine', startBone: 'pelvis', endBone: 'head' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_ik_chain', toolName: 'manage_skeleton', arguments: { action: 'remove_ik_chain', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'Spine' }, expected: 'success|not found' },
  { scenario: 'Skel: list_ik_chains', toolName: 'manage_skeleton', arguments: { action: 'list_ik_chains', ikRigPath: `${TEST_FOLDER}/IK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: set_ik_chain_settings', toolName: 'manage_skeleton', arguments: { action: 'set_ik_chain_settings', ikRigPath: `${TEST_FOLDER}/IK_Test`, chainName: 'LeftArm', settings: { iterations: 15, tolerance: 0.01 } }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_goal', toolName: 'manage_skeleton', arguments: { action: 'add_ik_goal', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'LeftHand_Goal', chainName: 'LeftArm', boneName: 'hand_l' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_goal_righthand', toolName: 'manage_skeleton', arguments: { action: 'add_ik_goal', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'RightHand_Goal', chainName: 'RightArm', boneName: 'hand_r' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_goal_leftfoot', toolName: 'manage_skeleton', arguments: { action: 'add_ik_goal', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'LeftFoot_Goal', chainName: 'LeftLeg', boneName: 'foot_l' }, expected: 'success|not found' },
  { scenario: 'Skel: add_ik_goal_rightfoot', toolName: 'manage_skeleton', arguments: { action: 'add_ik_goal', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'RightFoot_Goal', chainName: 'RightLeg', boneName: 'foot_r' }, expected: 'success|not found' },
  { scenario: 'Skel: remove_ik_goal', toolName: 'manage_skeleton', arguments: { action: 'remove_ik_goal', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'LeftFoot_Goal' }, expected: 'success|not found' },
  { scenario: 'Skel: set_ik_goal_settings', toolName: 'manage_skeleton', arguments: { action: 'set_ik_goal_settings', ikRigPath: `${TEST_FOLDER}/IK_Test`, goalName: 'LeftHand_Goal', positionAlpha: 1.0, rotationAlpha: 0.5 }, expected: 'success|not found' },
  
  // === Additional Retargeter Operations ===
  { scenario: 'Skel: set_retarget_root', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_root', ikRigPath: `${TEST_FOLDER}/IK_Test`, rootBone: 'pelvis' }, expected: 'success|not found' },
  { scenario: 'Skel: set_retarget_pose', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_pose', retargeterPath: `${TEST_FOLDER}/RTG_Test`, poseType: 'MeshPose' }, expected: 'success|not found' },
  { scenario: 'Skel: set_chain_mapping_spine', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_chain_mapping', retargeterPath: `${TEST_FOLDER}/RTG_Test`, sourceChain: 'Spine', targetChain: 'Spine' }, expected: 'success|not found' },
  { scenario: 'Skel: set_chain_mapping_leftleg', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_chain_mapping', retargeterPath: `${TEST_FOLDER}/RTG_Test`, sourceChain: 'LeftLeg', targetChain: 'LeftLeg' }, expected: 'success|not found' },
  { scenario: 'Skel: set_chain_mapping_rightleg', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_chain_mapping', retargeterPath: `${TEST_FOLDER}/RTG_Test`, sourceChain: 'RightLeg', targetChain: 'RightLeg' }, expected: 'success|not found' },
  { scenario: 'Skel: set_retarget_global_settings', toolName: 'manage_skeleton', arguments: { action: 'set_retarget_global_settings', retargeterPath: `${TEST_FOLDER}/RTG_Test`, targetMeshOffset: { x: 0, y: 0, z: 0 }, targetMeshScale: 1.0 }, expected: 'success|not found' },
  { scenario: 'Skel: retarget_animation', toolName: 'manage_skeleton', arguments: { action: 'retarget_animation', retargeterPath: `${TEST_FOLDER}/RTG_Test`, sourceAnimPath: `${TEST_FOLDER}/Anim_Walk`, destPath: TEST_FOLDER }, expected: 'success|not found' },
  { scenario: 'Skel: batch_retarget_animations', toolName: 'manage_skeleton', arguments: { action: 'batch_retarget_animations', retargeterPath: `${TEST_FOLDER}/RTG_Test`, sourceDirectory: `${TEST_FOLDER}/Animations`, destPath: `${TEST_FOLDER}/RetargetedAnims` }, expected: 'success|not found' },
  
  // === Additional Cloth Operations ===
  { scenario: 'Skel: create_cloth_config_hair', toolName: 'manage_skeleton', arguments: { action: 'create_cloth_config', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Hair' }, expected: 'success|not found' },
  { scenario: 'Skel: create_cloth_config_skirt', toolName: 'manage_skeleton', arguments: { action: 'create_cloth_config', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Skirt' }, expected: 'success|not found' },
  { scenario: 'Skel: set_cloth_gravity_scale', toolName: 'manage_skeleton', arguments: { action: 'set_cloth_gravity_scale', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', gravityScale: 1.0 }, expected: 'success|not found' },
  { scenario: 'Skel: set_cloth_wind_settings', toolName: 'manage_skeleton', arguments: { action: 'set_cloth_wind_settings', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', windScale: 1.0, liftCoeff: 0.5, dragCoeff: 0.5 }, expected: 'success|not found' },
  { scenario: 'Skel: set_cloth_collision_thickness', toolName: 'manage_skeleton', arguments: { action: 'set_cloth_collision_thickness', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', thickness: 1.0 }, expected: 'success|not found' },
  { scenario: 'Skel: enable_cloth_self_collision', toolName: 'manage_skeleton', arguments: { action: 'enable_cloth_self_collision', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape', enabled: true }, expected: 'success|not found' },
  { scenario: 'Skel: list_cloth_configs', toolName: 'manage_skeleton', arguments: { action: 'list_cloth_configs', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: get_cloth_config', toolName: 'manage_skeleton', arguments: { action: 'get_cloth_config', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, clothName: 'Cape' }, expected: 'success|not found' },
  
  // === Skeleton Asset Operations ===
  { scenario: 'Skel: create_skeleton', toolName: 'manage_skeleton', arguments: { action: 'create_skeleton', name: 'Skeleton_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Skel: duplicate_skeleton', toolName: 'manage_skeleton', arguments: { action: 'duplicate_skeleton', sourcePath: `${TEST_FOLDER}/SK_Test_Skeleton`, destPath: TEST_FOLDER, newName: 'SK_TestCopy_Skeleton' }, expected: 'success|not found' },
  { scenario: 'Skel: merge_skeletons', toolName: 'manage_skeleton', arguments: { action: 'merge_skeletons', skeletonPaths: [`${TEST_FOLDER}/Skeleton_A`, `${TEST_FOLDER}/Skeleton_B`], resultPath: TEST_FOLDER, resultName: 'Skeleton_Merged' }, expected: 'success|not found' },
  { scenario: 'Skel: get_skeleton_compatible_meshes', toolName: 'manage_skeleton', arguments: { action: 'get_skeleton_compatible_meshes', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton` }, expected: 'success|not found' },
  { scenario: 'Skel: add_compatible_skeleton', toolName: 'manage_skeleton', arguments: { action: 'add_compatible_skeleton', skeletonPath: `${TEST_FOLDER}/SK_Test_Skeleton`, compatibleSkeletonPath: `${TEST_FOLDER}/SK_Other_Skeleton` }, expected: 'success|not found' },
  
  // === Skeletal Mesh Import Options ===
  { scenario: 'Skel: set_import_options', toolName: 'manage_skeleton', arguments: { action: 'set_import_options', importNormals: true, importTangents: true, reorderMaterials: false }, expected: 'success' },
  { scenario: 'Skel: set_import_morph_targets', toolName: 'manage_skeleton', arguments: { action: 'set_import_morph_targets', enabled: true, thresholdPosition: 0.1 }, expected: 'success' },
  { scenario: 'Skel: set_import_animations', toolName: 'manage_skeleton', arguments: { action: 'set_import_animations', enabled: true, importBoneTracks: true, animationLength: 'ExportedTime' }, expected: 'success' },
  
  // === Skeleton Utility ===
  { scenario: 'Skel: validate_skeleton', toolName: 'manage_skeleton', arguments: { action: 'validate_skeleton', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: get_skeleton_stats', toolName: 'manage_skeleton', arguments: { action: 'get_skeleton_stats', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: cleanup_unused_bones', toolName: 'manage_skeleton', arguments: { action: 'cleanup_unused_bones', skeletalMeshPath: `${TEST_FOLDER}/SK_Test` }, expected: 'success|not found' },
  { scenario: 'Skel: list_skeletons', toolName: 'manage_skeleton', arguments: { action: 'list_skeletons', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Skel: list_physics_assets', toolName: 'manage_skeleton', arguments: { action: 'list_physics_assets', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Skel: list_ik_rigs', toolName: 'manage_skeleton', arguments: { action: 'list_ik_rigs', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Skel: list_ik_retargeters', toolName: 'manage_skeleton', arguments: { action: 'list_ik_retargeters', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Skel: get_skeleton_info_extended', toolName: 'manage_skeleton', arguments: { action: 'get_skeleton_info', skeletalMeshPath: `${TEST_FOLDER}/SK_Test`, includePhysics: true, includeCloth: true }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_AUDIO (134 actions) - Top 50 most common
// ============================================================================
const manageAudioTests = [
  // === Sound Assets ===
  { scenario: 'Audio: list_sounds', toolName: 'manage_audio', arguments: { action: 'list_sounds', directory: '/Game' }, expected: 'success' },
  { scenario: 'Audio: import_sound', toolName: 'manage_audio', arguments: { action: 'import_sound', sourcePath: 'C:/temp/sound.wav', destPath: TEST_FOLDER, name: 'SFX_Test' }, expected: 'success|file not found' },
  { scenario: 'Audio: create_sound_cue', toolName: 'manage_audio', arguments: { action: 'create_sound_cue', name: 'SC_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: add_sound_to_cue', toolName: 'manage_audio', arguments: { action: 'add_sound_to_cue', cuePath: `${TEST_FOLDER}/SC_Test`, soundWavePath: '/Engine/EngineSounds/WhiteNoise' }, expected: 'success|not found' },
  
  // === Sound Cue Nodes ===
  { scenario: 'Audio: add_cue_node_random', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Random' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_mixer', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Mixer' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_delay', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Delay' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_looping', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Looping' }, expected: 'success|not found' },
  { scenario: 'Audio: connect_cue_nodes', toolName: 'manage_audio', arguments: { action: 'connect_cue_nodes', cuePath: `${TEST_FOLDER}/SC_Test`, fromNode: 'Wave1', toNode: 'Output' }, expected: 'success|not found' },
  
  // === Sound Classes ===
  { scenario: 'Audio: create_sound_class', toolName: 'manage_audio', arguments: { action: 'create_sound_class', name: 'SFX', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: set_sound_class_volume', toolName: 'manage_audio', arguments: { action: 'set_sound_class_volume', classPath: `${TEST_FOLDER}/SFX`, volume: 0.8 }, expected: 'success|not found' },
  { scenario: 'Audio: set_sound_class_pitch', toolName: 'manage_audio', arguments: { action: 'set_sound_class_pitch', classPath: `${TEST_FOLDER}/SFX`, pitch: 1.0 }, expected: 'success|not found' },
  { scenario: 'Audio: assign_sound_class', toolName: 'manage_audio', arguments: { action: 'assign_sound_class', soundPath: `${TEST_FOLDER}/SC_Test`, classPath: `${TEST_FOLDER}/SFX` }, expected: 'success|not found' },
  
  // === Sound Mix ===
  { scenario: 'Audio: create_sound_mix', toolName: 'manage_audio', arguments: { action: 'create_sound_mix', name: 'Mix_Combat', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: add_class_to_mix', toolName: 'manage_audio', arguments: { action: 'add_class_to_mix', mixPath: `${TEST_FOLDER}/Mix_Combat`, classPath: `${TEST_FOLDER}/SFX`, volumeAdjust: 1.2 }, expected: 'success|not found' },
  { scenario: 'Audio: push_sound_mix', toolName: 'manage_audio', arguments: { action: 'push_sound_mix', mixPath: `${TEST_FOLDER}/Mix_Combat` }, expected: 'success|not found' },
  { scenario: 'Audio: pop_sound_mix', toolName: 'manage_audio', arguments: { action: 'pop_sound_mix', mixPath: `${TEST_FOLDER}/Mix_Combat` }, expected: 'success|not found' },
  
  // === Attenuation ===
  { scenario: 'Audio: create_attenuation', toolName: 'manage_audio', arguments: { action: 'create_attenuation', name: 'Atten_3D', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: configure_attenuation', toolName: 'manage_audio', arguments: { action: 'configure_attenuation', attenuationPath: `${TEST_FOLDER}/Atten_3D`, innerRadius: 100, falloffDistance: 2000, attenuationShape: 'Sphere' }, expected: 'success|not found' },
  { scenario: 'Audio: assign_attenuation', toolName: 'manage_audio', arguments: { action: 'assign_attenuation', soundPath: `${TEST_FOLDER}/SC_Test`, attenuationPath: `${TEST_FOLDER}/Atten_3D` }, expected: 'success|not found' },
  
  // === Concurrency ===
  { scenario: 'Audio: create_concurrency', toolName: 'manage_audio', arguments: { action: 'create_concurrency', name: 'Conc_Footsteps', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: configure_concurrency', toolName: 'manage_audio', arguments: { action: 'configure_concurrency', concurrencyPath: `${TEST_FOLDER}/Conc_Footsteps`, maxCount: 4, resolutionRule: 'StopOldest' }, expected: 'success|not found' },
  
  // === Ambient Sounds ===
  { scenario: 'Audio: spawn_ambient_sound', toolName: 'manage_audio', arguments: { action: 'spawn_ambient_sound', actorName: 'AmbientWind', soundPath: `${TEST_FOLDER}/SC_Test`, location: { x: 0, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Audio: configure_ambient_sound', toolName: 'manage_audio', arguments: { action: 'configure_ambient_sound', actorName: 'AmbientWind', autoActivate: true, isLooping: true }, expected: 'success|not found' },
  
  // === Audio Component ===
  { scenario: 'Audio: add_audio_component', toolName: 'manage_audio', arguments: { action: 'add_audio_component', actorName: 'Player', componentName: 'FootstepAudio' }, expected: 'success|not found' },
  { scenario: 'Audio: set_audio_component_sound', toolName: 'manage_audio', arguments: { action: 'set_audio_component_sound', actorName: 'Player', componentName: 'FootstepAudio', soundPath: `${TEST_FOLDER}/SC_Test` }, expected: 'success|not found' },
  { scenario: 'Audio: play_audio_component', toolName: 'manage_audio', arguments: { action: 'play_audio_component', actorName: 'Player', componentName: 'FootstepAudio' }, expected: 'success|not found' },
  { scenario: 'Audio: stop_audio_component', toolName: 'manage_audio', arguments: { action: 'stop_audio_component', actorName: 'Player', componentName: 'FootstepAudio' }, expected: 'success|not found' },
  
  // === Audio Effects ===
  { scenario: 'Audio: create_reverb_effect', toolName: 'manage_audio', arguments: { action: 'create_reverb_effect', name: 'Reverb_Cave', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: configure_reverb', toolName: 'manage_audio', arguments: { action: 'configure_reverb', reverbPath: `${TEST_FOLDER}/Reverb_Cave`, decayTime: 3.0, density: 0.8, diffusion: 0.9 }, expected: 'success|not found' },
  { scenario: 'Audio: create_submix', toolName: 'manage_audio', arguments: { action: 'create_submix', name: 'Submix_Master', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: add_submix_effect', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'Compressor' }, expected: 'success|not found' },
  
  // === MetaSounds ===
  { scenario: 'Audio: create_metasound', toolName: 'manage_audio', arguments: { action: 'create_metasound', name: 'MS_Synth', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: add_metasound_input', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'Frequency', inputType: 'Float' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_output', toolName: 'manage_audio', arguments: { action: 'add_metasound_output', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, outputName: 'AudioOut', outputType: 'Audio' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'SineOscillator' }, expected: 'success|not found' },
  { scenario: 'Audio: connect_metasound_nodes', toolName: 'manage_audio', arguments: { action: 'connect_metasound_nodes', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, fromNode: 'SineOsc', fromPin: 'Audio', toNode: 'Output', toPin: 'AudioOut' }, expected: 'success|not found' },
  { scenario: 'Audio: set_metasound_variable', toolName: 'manage_audio', arguments: { action: 'set_metasound_variable', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, variableName: 'Frequency', value: 440.0 }, expected: 'success|not found' },
  
  // === Audio Modulation ===
  { scenario: 'Audio: create_modulation_control_bus', toolName: 'manage_audio', arguments: { action: 'create_modulation_control_bus', name: 'Bus_MasterVolume', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: create_modulation_generator', toolName: 'manage_audio', arguments: { action: 'create_modulation_generator', name: 'Gen_LFO', path: TEST_FOLDER, generatorType: 'LFO' }, expected: 'success|already exists' },
  { scenario: 'Audio: bind_bus_to_parameter', toolName: 'manage_audio', arguments: { action: 'bind_bus_to_parameter', busPath: `${TEST_FOLDER}/Bus_MasterVolume`, soundPath: `${TEST_FOLDER}/SC_Test`, parameterName: 'Volume' }, expected: 'success|not found' },
  
  // === Sound Wave Properties ===
  { scenario: 'Audio: get_sound_wave_info', toolName: 'manage_audio', arguments: { action: 'get_sound_wave_info', soundPath: '/Engine/EngineSounds/WhiteNoise' }, expected: 'success|not found' },
  { scenario: 'Audio: set_sound_wave_looping', toolName: 'manage_audio', arguments: { action: 'set_sound_wave_looping', soundPath: `${TEST_FOLDER}/SFX_Test`, looping: true }, expected: 'success|not found' },
  { scenario: 'Audio: set_sound_wave_streaming', toolName: 'manage_audio', arguments: { action: 'set_sound_wave_streaming', soundPath: `${TEST_FOLDER}/SFX_Test`, streaming: true }, expected: 'success|not found' },
  
  // === Audio Playback ===
  { scenario: 'Audio: play_sound_2d', toolName: 'manage_audio', arguments: { action: 'play_sound_2d', soundPath: '/Engine/EngineSounds/WhiteNoise' }, expected: 'success|not found' },
  { scenario: 'Audio: play_sound_at_location', toolName: 'manage_audio', arguments: { action: 'play_sound_at_location', soundPath: '/Engine/EngineSounds/WhiteNoise', location: { x: 0, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Audio: stop_all_sounds', toolName: 'manage_audio', arguments: { action: 'stop_all_sounds' }, expected: 'success' },
  
  // === Dialogue ===
  { scenario: 'Audio: create_dialogue_voice', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: 'Voice_Hero', path: TEST_FOLDER, gender: 'Male', plurality: 'Singular' }, expected: 'success|already exists' },
  { scenario: 'Audio: create_dialogue_wave', toolName: 'manage_audio', arguments: { action: 'create_dialogue_wave', name: 'DW_Greeting', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: set_dialogue_context', toolName: 'manage_audio', arguments: { action: 'set_dialogue_context', dialoguePath: `${TEST_FOLDER}/DW_Greeting`, speakerVoice: `${TEST_FOLDER}/Voice_Hero` }, expected: 'success|not found' },
  
  // === More Sound Cue Operations ===
  { scenario: 'Audio: set_cue_attenuation', toolName: 'manage_audio', arguments: { action: 'set_cue_attenuation', assetPath: `${TEST_FOLDER}/SC_Test`, attenuationPath: `${TEST_FOLDER}/Atten_3D` }, expected: 'success|not found' },
  { scenario: 'Audio: set_cue_concurrency', toolName: 'manage_audio', arguments: { action: 'set_cue_concurrency', assetPath: `${TEST_FOLDER}/SC_Test`, concurrencyPath: `${TEST_FOLDER}/Conc_Footsteps` }, expected: 'success|not found' },
  
  // === More Effects ===
  { scenario: 'Audio: create_source_effect_chain', toolName: 'manage_audio', arguments: { action: 'create_source_effect_chain', name: 'SEC_Master', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: add_source_effect', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'LowPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: create_submix_effect', toolName: 'manage_audio', arguments: { action: 'create_submix_effect', name: 'SME_Delay', path: TEST_FOLDER, effectType: 'Delay' }, expected: 'success|already exists' },
  
  // === Attenuation Settings ===
  { scenario: 'Audio: create_attenuation_settings', toolName: 'manage_audio', arguments: { action: 'create_attenuation_settings', name: 'AS_Custom', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: configure_distance_attenuation', toolName: 'manage_audio', arguments: { action: 'configure_distance_attenuation', attenuationPath: `${TEST_FOLDER}/AS_Custom`, innerRadius: 200, falloffDistance: 3000 }, expected: 'success|not found' },
  { scenario: 'Audio: configure_spatialization', toolName: 'manage_audio', arguments: { action: 'configure_spatialization', attenuationPath: `${TEST_FOLDER}/AS_Custom`, spatializationMode: 'Binaural' }, expected: 'success|not found' },
  { scenario: 'Audio: configure_occlusion', toolName: 'manage_audio', arguments: { action: 'configure_occlusion', attenuationPath: `${TEST_FOLDER}/AS_Custom`, enabled: true, occlusionTraceChannel: 'Visibility' }, expected: 'success|not found' },
  { scenario: 'Audio: configure_reverb_send', toolName: 'manage_audio', arguments: { action: 'configure_reverb_send', attenuationPath: `${TEST_FOLDER}/AS_Custom`, sendLevel: 0.5 }, expected: 'success|not found' },
  
  // === Sound Class/Mix Authoring ===
  { scenario: 'Audio: set_class_properties', toolName: 'manage_audio', arguments: { action: 'set_class_properties', soundClassPath: `${TEST_FOLDER}/SFX`, properties: { Volume: 0.9, Pitch: 1.0 } }, expected: 'success|not found' },
  { scenario: 'Audio: set_class_parent', toolName: 'manage_audio', arguments: { action: 'set_class_parent', soundClassPath: `${TEST_FOLDER}/SFX`, parentClassPath: '/Engine/EngineSounds/Master' }, expected: 'success|not found' },
  { scenario: 'Audio: add_mix_modifier', toolName: 'manage_audio', arguments: { action: 'add_mix_modifier', mixPath: `${TEST_FOLDER}/Mix_Combat`, modifierType: 'EQ' }, expected: 'success|not found' },
  { scenario: 'Audio: configure_mix_eq', toolName: 'manage_audio', arguments: { action: 'configure_mix_eq', mixPath: `${TEST_FOLDER}/Mix_Combat`, lowFrequencyGain: 1.2, highFrequencyGain: 0.8 }, expected: 'success|not found' },
  
  // === Audio Playback Control ===
  { scenario: 'Audio: prime_sound', toolName: 'manage_audio', arguments: { action: 'prime_sound', soundPath: `${TEST_FOLDER}/SC_Test` }, expected: 'success|not found' },
  { scenario: 'Audio: play_sound_attached', toolName: 'manage_audio', arguments: { action: 'play_sound_attached', soundPath: `${TEST_FOLDER}/SC_Test`, actorName: 'Player', attachPointName: 'head' }, expected: 'success|not found' },
  { scenario: 'Audio: spawn_sound_at_location', toolName: 'manage_audio', arguments: { action: 'spawn_sound_at_location', soundPath: `${TEST_FOLDER}/SC_Test`, location: { x: 100, y: 200, z: 50 } }, expected: 'success|not found' },
  { scenario: 'Audio: fade_sound_in', toolName: 'manage_audio', arguments: { action: 'fade_sound_in', soundPath: `${TEST_FOLDER}/SC_Test`, fadeInTime: 1.5 }, expected: 'success|not found' },
  { scenario: 'Audio: fade_sound_out', toolName: 'manage_audio', arguments: { action: 'fade_sound_out', soundPath: `${TEST_FOLDER}/SC_Test`, fadeOutTime: 2.0 }, expected: 'success|not found' },
  { scenario: 'Audio: fade_sound', toolName: 'manage_audio', arguments: { action: 'fade_sound', soundPath: `${TEST_FOLDER}/SC_Test`, targetVolume: 0.5, fadeTime: 1.0 }, expected: 'success|not found' },
  { scenario: 'Audio: set_doppler_effect', toolName: 'manage_audio', arguments: { action: 'set_doppler_effect', actorName: 'Player', componentName: 'FootstepAudio', scale: 1.5 }, expected: 'success|not found' },
  { scenario: 'Audio: set_audio_occlusion', toolName: 'manage_audio', arguments: { action: 'set_audio_occlusion', actorName: 'Player', componentName: 'FootstepAudio', enabled: true }, expected: 'success|not found' },
  
  // === Environment ===
  { scenario: 'Audio: create_ambient_sound', toolName: 'manage_audio', arguments: { action: 'create_ambient_sound', name: 'AS_Wind', soundPath: `${TEST_FOLDER}/SC_Test`, location: { x: 0, y: 0, z: 200 } }, expected: 'success|not found' },
  { scenario: 'Audio: create_reverb_zone', toolName: 'manage_audio', arguments: { action: 'create_reverb_zone', name: 'RZ_Cave', location: { x: 0, y: 0, z: 0 }, reverbEffect: 'LargeCave' }, expected: 'success' },
  { scenario: 'Audio: enable_audio_analysis', toolName: 'manage_audio', arguments: { action: 'enable_audio_analysis', actorName: 'Player', componentName: 'FootstepAudio', fftSize: 1024 }, expected: 'success|not found' },
  
  // === MetaSounds Queries ===
  { scenario: 'Audio: list_metasound_assets', toolName: 'manage_audio', arguments: { action: 'list_metasound_assets', path: '/Game' }, expected: 'success' },
  { scenario: 'Audio: get_metasound_inputs', toolName: 'manage_audio', arguments: { action: 'get_metasound_inputs', metaSoundPath: `${TEST_FOLDER}/MS_Synth` }, expected: 'success|not found' },
  { scenario: 'Audio: trigger_metasound', toolName: 'manage_audio', arguments: { action: 'trigger_metasound', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'Trigger' }, expected: 'success|not found' },
  { scenario: 'Audio: set_metasound_default', toolName: 'manage_audio', arguments: { action: 'set_metasound_default', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'Frequency', defaultValue: 880.0 }, expected: 'success|not found' },
  
  // === Audio Info ===
  { scenario: 'Audio: get_audio_info', toolName: 'manage_audio', arguments: { action: 'get_audio_info' }, expected: 'success' },
  
  // === Wwise Middleware ===
  { scenario: 'Audio: mw_connect_wwise_project', toolName: 'manage_audio', arguments: { action: 'mw_connect_wwise_project', projectPath: 'C:/WwiseProjects/Game.wproj' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_post_wwise_event', toolName: 'manage_audio', arguments: { action: 'mw_post_wwise_event', eventName: 'Play_Footstep', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_post_wwise_event_at_location', toolName: 'manage_audio', arguments: { action: 'mw_post_wwise_event_at_location', eventName: 'Play_Explosion', location: { x: 100, y: 0, z: 50 } }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_stop_wwise_event', toolName: 'manage_audio', arguments: { action: 'mw_stop_wwise_event', eventName: 'Play_Footstep', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_rtpc_value', toolName: 'manage_audio', arguments: { action: 'mw_set_rtpc_value', rtpcName: 'Player_Health', value: 100.0 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_rtpc_value_on_actor', toolName: 'manage_audio', arguments: { action: 'mw_set_rtpc_value_on_actor', rtpcName: 'Speed', value: 50.0, actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_get_rtpc_value', toolName: 'manage_audio', arguments: { action: 'mw_get_rtpc_value', rtpcName: 'Player_Health' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_wwise_switch', toolName: 'manage_audio', arguments: { action: 'mw_set_wwise_switch', switchGroup: 'Surface', switchValue: 'Grass' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_wwise_switch_on_actor', toolName: 'manage_audio', arguments: { action: 'mw_set_wwise_switch_on_actor', switchGroup: 'Surface', switchValue: 'Metal', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_wwise_state', toolName: 'manage_audio', arguments: { action: 'mw_set_wwise_state', stateGroup: 'GameState', stateValue: 'Combat' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_load_wwise_bank', toolName: 'manage_audio', arguments: { action: 'mw_load_wwise_bank', bankName: 'Player_SFX' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_unload_wwise_bank', toolName: 'manage_audio', arguments: { action: 'mw_unload_wwise_bank', bankName: 'Player_SFX' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_get_loaded_banks', toolName: 'manage_audio', arguments: { action: 'mw_get_loaded_banks' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_create_wwise_component', toolName: 'manage_audio', arguments: { action: 'mw_create_wwise_component', actorName: 'Player', componentName: 'WwiseAudio' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_wwise_component', toolName: 'manage_audio', arguments: { action: 'mw_configure_wwise_component', actorName: 'Player', componentName: 'WwiseAudio', properties: { AttenuationScalingFactor: 1.0 } }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_spatial_audio', toolName: 'manage_audio', arguments: { action: 'mw_configure_spatial_audio', enabled: true, diffractionEnabled: true }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_configure_room', toolName: 'manage_audio', arguments: { action: 'mw_configure_room', actorName: 'Room1', reverbAuxBus: 'Room_Reverb' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_portal', toolName: 'manage_audio', arguments: { action: 'mw_configure_portal', actorName: 'Portal1', enabled: true }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_listener_position', toolName: 'manage_audio', arguments: { action: 'mw_set_listener_position', location: { x: 0, y: 0, z: 100 }, rotation: { pitch: 0, yaw: 0, roll: 0 } }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_get_wwise_event_duration', toolName: 'manage_audio', arguments: { action: 'mw_get_wwise_event_duration', eventName: 'Play_Music' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_create_wwise_trigger', toolName: 'manage_audio', arguments: { action: 'mw_create_wwise_trigger', triggerName: 'OnFootstep' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_post_wwise_trigger', toolName: 'manage_audio', arguments: { action: 'mw_post_wwise_trigger', triggerName: 'OnFootstep', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_aux_send', toolName: 'manage_audio', arguments: { action: 'mw_set_aux_send', actorName: 'Player', auxBusName: 'Reverb', sendLevel: 0.5 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_wwise_occlusion', toolName: 'manage_audio', arguments: { action: 'mw_configure_wwise_occlusion', actorName: 'Player', enabled: true, refreshInterval: 0.1 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_get_wwise_status', toolName: 'manage_audio', arguments: { action: 'mw_get_wwise_status' }, expected: 'success' },
  
  // === FMOD Middleware ===
  { scenario: 'Audio: mw_connect_fmod_project', toolName: 'manage_audio', arguments: { action: 'mw_connect_fmod_project', projectPath: 'C:/FMODProjects/Game.fspro' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_play_fmod_event', toolName: 'manage_audio', arguments: { action: 'mw_play_fmod_event', eventPath: 'event:/SFX/Footstep', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_play_fmod_event_at_location', toolName: 'manage_audio', arguments: { action: 'mw_play_fmod_event_at_location', eventPath: 'event:/SFX/Explosion', location: { x: 100, y: 0, z: 50 } }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_stop_fmod_event', toolName: 'manage_audio', arguments: { action: 'mw_stop_fmod_event', eventPath: 'event:/SFX/Footstep', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_parameter', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_parameter', actorName: 'Player', parameterName: 'Speed', value: 1.0 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_global_parameter', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_global_parameter', parameterName: 'MasterVolume', value: 0.8 }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_get_fmod_parameter', toolName: 'manage_audio', arguments: { action: 'mw_get_fmod_parameter', actorName: 'Player', parameterName: 'Speed' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_load_fmod_bank', toolName: 'manage_audio', arguments: { action: 'mw_load_fmod_bank', bankPath: 'bank:/Master' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_unload_fmod_bank', toolName: 'manage_audio', arguments: { action: 'mw_unload_fmod_bank', bankPath: 'bank:/Master' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_get_fmod_loaded_banks', toolName: 'manage_audio', arguments: { action: 'mw_get_fmod_loaded_banks' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_create_fmod_component', toolName: 'manage_audio', arguments: { action: 'mw_create_fmod_component', actorName: 'Player', componentName: 'FMODAudio' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_fmod_component', toolName: 'manage_audio', arguments: { action: 'mw_configure_fmod_component', actorName: 'Player', componentName: 'FMODAudio', eventPath: 'event:/SFX/Ambient' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_bus_volume', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_bus_volume', busPath: 'bus:/Master', volume: 0.9 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_bus_paused', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_bus_paused', busPath: 'bus:/SFX', paused: true }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_bus_mute', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_bus_mute', busPath: 'bus:/Music', muted: true }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_vca_volume', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_vca_volume', vcaPath: 'vca:/Master', volume: 0.8 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_apply_fmod_snapshot', toolName: 'manage_audio', arguments: { action: 'mw_apply_fmod_snapshot', snapshotPath: 'snapshot:/Underwater' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_release_fmod_snapshot', toolName: 'manage_audio', arguments: { action: 'mw_release_fmod_snapshot', snapshotPath: 'snapshot:/Underwater' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_get_fmod_event_info', toolName: 'manage_audio', arguments: { action: 'mw_get_fmod_event_info', eventPath: 'event:/SFX/Footstep' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_configure_fmod_occlusion', toolName: 'manage_audio', arguments: { action: 'mw_configure_fmod_occlusion', enabled: true, directOcclusion: 0.8, reverbOcclusion: 0.5 }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_get_fmod_status', toolName: 'manage_audio', arguments: { action: 'mw_get_fmod_status' }, expected: 'success' },
  { scenario: 'Audio: mw_get_fmod_memory_usage', toolName: 'manage_audio', arguments: { action: 'mw_get_fmod_memory_usage' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_pause_all_fmod_events', toolName: 'manage_audio', arguments: { action: 'mw_pause_all_fmod_events' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_resume_all_fmod_events', toolName: 'manage_audio', arguments: { action: 'mw_resume_all_fmod_events' }, expected: 'success|not installed' },
  
  // === Bink Video Middleware ===
  { scenario: 'Audio: mw_create_bink_media_player', toolName: 'manage_audio', arguments: { action: 'mw_create_bink_media_player', name: 'BinkPlayer', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: mw_open_bink_video', toolName: 'manage_audio', arguments: { action: 'mw_open_bink_video', playerPath: `${TEST_FOLDER}/BinkPlayer`, videoPath: 'C:/Videos/intro.bk2' }, expected: 'success|not found' },
  { scenario: 'Audio: mw_play_bink', toolName: 'manage_audio', arguments: { action: 'mw_play_bink', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_pause_bink', toolName: 'manage_audio', arguments: { action: 'mw_pause_bink', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_stop_bink', toolName: 'manage_audio', arguments: { action: 'mw_stop_bink', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_seek_bink', toolName: 'manage_audio', arguments: { action: 'mw_seek_bink', playerPath: `${TEST_FOLDER}/BinkPlayer`, time: 5.0 }, expected: 'success|not found' },
  { scenario: 'Audio: mw_set_bink_looping', toolName: 'manage_audio', arguments: { action: 'mw_set_bink_looping', playerPath: `${TEST_FOLDER}/BinkPlayer`, looping: true }, expected: 'success|not found' },
  { scenario: 'Audio: mw_set_bink_rate', toolName: 'manage_audio', arguments: { action: 'mw_set_bink_rate', playerPath: `${TEST_FOLDER}/BinkPlayer`, rate: 1.0 }, expected: 'success|not found' },
  { scenario: 'Audio: mw_set_bink_volume', toolName: 'manage_audio', arguments: { action: 'mw_set_bink_volume', playerPath: `${TEST_FOLDER}/BinkPlayer`, volume: 0.8 }, expected: 'success|not found' },
  { scenario: 'Audio: mw_get_bink_duration', toolName: 'manage_audio', arguments: { action: 'mw_get_bink_duration', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_get_bink_time', toolName: 'manage_audio', arguments: { action: 'mw_get_bink_time', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_get_bink_status', toolName: 'manage_audio', arguments: { action: 'mw_get_bink_status', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_create_bink_texture', toolName: 'manage_audio', arguments: { action: 'mw_create_bink_texture', name: 'BinkTex', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Audio: mw_configure_bink_texture', toolName: 'manage_audio', arguments: { action: 'mw_configure_bink_texture', texturePath: `${TEST_FOLDER}/BinkTex`, width: 1920, height: 1080 }, expected: 'success|not found' },
  { scenario: 'Audio: mw_set_bink_texture_player', toolName: 'manage_audio', arguments: { action: 'mw_set_bink_texture_player', texturePath: `${TEST_FOLDER}/BinkTex`, playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_get_bink_dimensions', toolName: 'manage_audio', arguments: { action: 'mw_get_bink_dimensions', playerPath: `${TEST_FOLDER}/BinkPlayer` }, expected: 'success|not found' },
  
  // === Middleware Info ===
  { scenario: 'Audio: mw_get_audio_middleware_info', toolName: 'manage_audio', arguments: { action: 'mw_get_audio_middleware_info' }, expected: 'success' },
  
  // === Additional Audio Actions (Expanded Coverage) ===
  // More Sound Cue Nodes
  { scenario: 'Audio: add_cue_node_modulator', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Modulator' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_concatenator', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Concatenator' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_crossfade', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Crossfade' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_doppler', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Doppler' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_enveloper', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Enveloper' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_switch', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Switch' }, expected: 'success|not found' },
  { scenario: 'Audio: add_cue_node_branch', toolName: 'manage_audio', arguments: { action: 'add_cue_node', cuePath: `${TEST_FOLDER}/SC_Test`, nodeType: 'Branch' }, expected: 'success|not found' },
  
  // Sound Mix Overrides
  { scenario: 'Audio: set_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'set_sound_mix_class_override', mixPath: `${TEST_FOLDER}/Mix_Combat`, classPath: `${TEST_FOLDER}/SFX`, volume: 1.5, pitch: 1.0, fadeInTime: 0.5, fadeOutTime: 0.5 }, expected: 'success|not found' },
  { scenario: 'Audio: clear_sound_mix_class_override', toolName: 'manage_audio', arguments: { action: 'clear_sound_mix_class_override', mixPath: `${TEST_FOLDER}/Mix_Combat`, classPath: `${TEST_FOLDER}/SFX` }, expected: 'success|not found' },
  { scenario: 'Audio: set_base_sound_mix', toolName: 'manage_audio', arguments: { action: 'set_base_sound_mix', mixPath: `${TEST_FOLDER}/Mix_Combat` }, expected: 'success|not found' },
  
  // Audio Component Operations
  { scenario: 'Audio: create_audio_component', toolName: 'manage_audio', arguments: { action: 'create_audio_component', actorName: 'Player', componentName: 'MusicAudio' }, expected: 'success|not found' },
  { scenario: 'Audio: set_sound_attenuation', toolName: 'manage_audio', arguments: { action: 'set_sound_attenuation', actorName: 'Player', componentName: 'FootstepAudio', attenuationPath: `${TEST_FOLDER}/Atten_3D` }, expected: 'success|not found' },
  
  // MetaSound Extended
  { scenario: 'Audio: add_metasound_node_sawtosc', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'SawtoothOscillator' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_squareosc', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'SquareOscillator' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_noise', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'NoiseGenerator' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_filter', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'LowPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_highpass', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'HighPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_bandpass', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'BandPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_adsr', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'ADSR' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_delay', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Delay' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_reverb', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Reverb' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_chorus', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Chorus' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_phaser', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Phaser' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_flanger', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Flanger' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_compressor', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Compressor' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_node_limiter', toolName: 'manage_audio', arguments: { action: 'add_metasound_node', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, nodeType: 'Limiter' }, expected: 'success|not found' },
  
  // Additional MetaSound I/O
  { scenario: 'Audio: add_metasound_input_bool', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'IsPlaying', inputType: 'Bool' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_input_int', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'NoteNumber', inputType: 'Int32' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_input_trigger', toolName: 'manage_audio', arguments: { action: 'add_metasound_input', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'OnPlay', inputType: 'Trigger' }, expected: 'success|not found' },
  { scenario: 'Audio: add_metasound_output_trigger', toolName: 'manage_audio', arguments: { action: 'add_metasound_output', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, outputName: 'OnFinished', outputType: 'Trigger' }, expected: 'success|not found' },
  
  // Additional Wwise Operations
  { scenario: 'Audio: mw_set_wwise_game_object', toolName: 'manage_audio', arguments: { action: 'mw_set_wwise_game_object', actorName: 'Player', gameObjectId: 100 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_unset_wwise_game_object', toolName: 'manage_audio', arguments: { action: 'mw_unset_wwise_game_object', actorName: 'Player' }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_wwise_project_path', toolName: 'manage_audio', arguments: { action: 'mw_set_wwise_project_path', projectPath: 'C:/WwiseProjects/Game.wproj' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_configure_wwise_init', toolName: 'manage_audio', arguments: { action: 'mw_configure_wwise_init', settings: { maxVoices: 64, poolSize: 16 } }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_restart_wwise_engine', toolName: 'manage_audio', arguments: { action: 'mw_restart_wwise_engine' }, expected: 'success|not installed' },
  
  // Additional FMOD Operations
  { scenario: 'Audio: mw_set_fmod_listener_attributes', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_listener_attributes', listenerIndex: 0, location: { x: 0, y: 0, z: 100 } }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_configure_fmod_attenuation', toolName: 'manage_audio', arguments: { action: 'mw_configure_fmod_attenuation', eventPath: 'event:/SFX/Footstep', minDistance: 1.0, maxDistance: 100.0 }, expected: 'success|not found|not installed' },
  { scenario: 'Audio: mw_set_fmod_studio_path', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_studio_path', studioPath: 'C:/FMODProjects/Game.fspro' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_configure_fmod_init', toolName: 'manage_audio', arguments: { action: 'mw_configure_fmod_init', settings: { maxChannels: 128, studioFlags: 0 } }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_restart_fmod_engine', toolName: 'manage_audio', arguments: { action: 'mw_restart_fmod_engine' }, expected: 'success|not installed' },
  { scenario: 'Audio: mw_set_fmod_3d_attributes', toolName: 'manage_audio', arguments: { action: 'mw_set_fmod_3d_attributes', actorName: 'Player', position: { x: 0, y: 0, z: 100 }, velocity: { x: 0, y: 0, z: 0 } }, expected: 'success|not found|not installed' },
  
  // Additional Bink Operations
  { scenario: 'Audio: mw_draw_bink_to_texture', toolName: 'manage_audio', arguments: { action: 'mw_draw_bink_to_texture', playerPath: `${TEST_FOLDER}/BinkPlayer`, texturePath: `${TEST_FOLDER}/BinkTex` }, expected: 'success|not found' },
  { scenario: 'Audio: mw_configure_bink_buffer_mode', toolName: 'manage_audio', arguments: { action: 'mw_configure_bink_buffer_mode', playerPath: `${TEST_FOLDER}/BinkPlayer`, bufferMode: 'Stream' }, expected: 'success|not found' },
  { scenario: 'Audio: mw_configure_bink_sound_track', toolName: 'manage_audio', arguments: { action: 'mw_configure_bink_sound_track', playerPath: `${TEST_FOLDER}/BinkPlayer`, soundTrackIndex: 0, enabled: true }, expected: 'success|not found' },
  { scenario: 'Audio: mw_configure_bink_draw_style', toolName: 'manage_audio', arguments: { action: 'mw_configure_bink_draw_style', playerPath: `${TEST_FOLDER}/BinkPlayer`, drawStyle: 'Stretched' }, expected: 'success|not found' },
  
  // Additional Modulation
  { scenario: 'Audio: create_modulation_generator_envelope', toolName: 'manage_audio', arguments: { action: 'create_modulation_generator', name: 'Gen_Envelope', path: TEST_FOLDER, generatorType: 'Envelope' }, expected: 'success|already exists' },
  { scenario: 'Audio: create_modulation_generator_random', toolName: 'manage_audio', arguments: { action: 'create_modulation_generator', name: 'Gen_Random', path: TEST_FOLDER, generatorType: 'Random' }, expected: 'success|already exists' },
  
  // Additional Submix Effects
  { scenario: 'Audio: add_submix_effect_eq', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'EQ' }, expected: 'success|not found' },
  { scenario: 'Audio: add_submix_effect_delay', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'Delay' }, expected: 'success|not found' },
  { scenario: 'Audio: add_submix_effect_chorus', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'Chorus' }, expected: 'success|not found' },
  { scenario: 'Audio: add_submix_effect_reverb', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'Reverb' }, expected: 'success|not found' },
  { scenario: 'Audio: add_submix_effect_limiter', toolName: 'manage_audio', arguments: { action: 'add_submix_effect', submixPath: `${TEST_FOLDER}/Submix_Master`, effectType: 'Limiter' }, expected: 'success|not found' },
  
  // Additional Source Effects
  { scenario: 'Audio: add_source_effect_highpass', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'HighPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: add_source_effect_bandpass', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'BandPassFilter' }, expected: 'success|not found' },
  { scenario: 'Audio: add_source_effect_eq', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'EQ' }, expected: 'success|not found' },
  { scenario: 'Audio: add_source_effect_distortion', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'Distortion' }, expected: 'success|not found' },
  { scenario: 'Audio: add_source_effect_bitcrusher', toolName: 'manage_audio', arguments: { action: 'add_source_effect', chainPath: `${TEST_FOLDER}/SEC_Master`, effectType: 'BitCrusher' }, expected: 'success|not found' },
  
  // Reverb Configuration
  { scenario: 'Audio: configure_reverb_preset', toolName: 'manage_audio', arguments: { action: 'configure_reverb', reverbPath: `${TEST_FOLDER}/Reverb_Cave`, decayTime: 5.0, density: 1.0, diffusion: 1.0, wetLevel: 0.8, dryLevel: 0.2 }, expected: 'success|not found' },
  
  // Dialogue Extended
  { scenario: 'Audio: create_dialogue_voice_female', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: 'Voice_Heroine', path: TEST_FOLDER, gender: 'Female', plurality: 'Singular' }, expected: 'success|already exists' },
  { scenario: 'Audio: create_dialogue_voice_plural', toolName: 'manage_audio', arguments: { action: 'create_dialogue_voice', name: 'Voice_Crowd', path: TEST_FOLDER, gender: 'Neuter', plurality: 'Plural' }, expected: 'success|already exists' },
  { scenario: 'Audio: create_dialogue_wave_response', toolName: 'manage_audio', arguments: { action: 'create_dialogue_wave', name: 'DW_Response', path: TEST_FOLDER }, expected: 'success|already exists' },
  
  // List/Query Operations
  { scenario: 'Audio: list_sounds_folder', toolName: 'manage_audio', arguments: { action: 'list_sounds', directory: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Audio: list_metasound_assets_folder', toolName: 'manage_audio', arguments: { action: 'list_metasound_assets', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Audio: get_metasound_inputs_synth', toolName: 'manage_audio', arguments: { action: 'get_metasound_inputs', metaSoundPath: `${TEST_FOLDER}/MS_Synth` }, expected: 'success|not found' },
  { scenario: 'Audio: trigger_metasound_play', toolName: 'manage_audio', arguments: { action: 'trigger_metasound', metaSoundPath: `${TEST_FOLDER}/MS_Synth`, inputName: 'OnPlay' }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_SEQUENCE (100 actions) - Top 50 most common
// ============================================================================
const manageSequenceTests = [
  // === Sequence Creation ===
  { scenario: 'Seq: create_sequence', toolName: 'manage_sequence', arguments: { action: 'create_sequence', name: 'LS_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Seq: open_sequence', toolName: 'manage_sequence', arguments: { action: 'open_sequence', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: close_sequence', toolName: 'manage_sequence', arguments: { action: 'close_sequence', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: duplicate_sequence', toolName: 'manage_sequence', arguments: { action: 'duplicate_sequence', sourcePath: `${TEST_FOLDER}/LS_Test`, destPath: TEST_FOLDER, name: 'LS_Copy' }, expected: 'success|not found' },
  { scenario: 'Seq: delete_sequence', toolName: 'manage_sequence', arguments: { action: 'delete_sequence', sequencePath: `${TEST_FOLDER}/LS_Copy` }, expected: 'success|not found' },
  
  // === Sequence Properties ===
  { scenario: 'Seq: get_sequence_info', toolName: 'manage_sequence', arguments: { action: 'get_sequence_info', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_sequence_length', toolName: 'manage_sequence', arguments: { action: 'set_sequence_length', sequencePath: `${TEST_FOLDER}/LS_Test`, startFrame: 0, endFrame: 300 }, expected: 'success|not found' },
  { scenario: 'Seq: set_frame_rate', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 30 }, expected: 'success|not found' },
  { scenario: 'Seq: set_playback_range', toolName: 'manage_sequence', arguments: { action: 'set_playback_range', sequencePath: `${TEST_FOLDER}/LS_Test`, startFrame: 0, endFrame: 150 }, expected: 'success|not found' },
  
  // === Tracks ===
  { scenario: 'Seq: add_camera_cut_track', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: add_audio_track', toolName: 'manage_sequence', arguments: { action: 'add_audio_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: add_fade_track', toolName: 'manage_sequence', arguments: { action: 'add_fade_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: add_event_track', toolName: 'manage_sequence', arguments: { action: 'add_event_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: add_shot_track', toolName: 'manage_sequence', arguments: { action: 'add_shot_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  
  // === Bindings ===
  { scenario: 'Seq: add_possessable', toolName: 'manage_sequence', arguments: { action: 'add_possessable', sequencePath: `${TEST_FOLDER}/LS_Test`, actorPath: '/Game/MainLevel.MainLevel:PersistentLevel.Player_0' }, expected: 'success|not found' },
  { scenario: 'Seq: add_spawnable', toolName: 'manage_sequence', arguments: { action: 'add_spawnable', sequencePath: `${TEST_FOLDER}/LS_Test`, actorClass: '/Script/Engine.StaticMeshActor' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_binding', toolName: 'manage_sequence', arguments: { action: 'remove_binding', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'Player_0' }, expected: 'success|not found' },
  { scenario: 'Seq: get_bindings', toolName: 'manage_sequence', arguments: { action: 'get_bindings', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  
  // === Property Tracks ===
  { scenario: 'Seq: add_property_track', toolName: 'manage_sequence', arguments: { action: 'add_property_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'Player_0', propertyPath: 'RelativeLocation' }, expected: 'success|not found' },
  { scenario: 'Seq: add_transform_track', toolName: 'manage_sequence', arguments: { action: 'add_transform_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'Player_0' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_track', toolName: 'manage_sequence', arguments: { action: 'remove_track', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform' }, expected: 'success|not found' },
  
  // === Keyframes ===
  { scenario: 'Seq: add_keyframe', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 0, value: { x: 0, y: 0, z: 0 } }, expected: 'success|not found' },
  { scenario: 'Seq: add_keyframe_end', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 100, value: { x: 500, y: 0, z: 100 } }, expected: 'success|not found' },
  { scenario: 'Seq: remove_keyframe', toolName: 'manage_sequence', arguments: { action: 'remove_keyframe', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: get_keyframes', toolName: 'manage_sequence', arguments: { action: 'get_keyframes', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform' }, expected: 'success|not found' },
  { scenario: 'Seq: set_keyframe_interpolation', toolName: 'manage_sequence', arguments: { action: 'set_keyframe_interpolation', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 0, interpolation: 'Cubic' }, expected: 'success|not found' },
  
  // === Camera ===
  { scenario: 'Seq: create_cine_camera', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraName: 'MainCam' }, expected: 'success|not found' },
  { scenario: 'Seq: add_camera_to_sequence', toolName: 'manage_sequence', arguments: { action: 'add_camera_to_sequence', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActor: 'MainCam' }, expected: 'success|not found' },
  { scenario: 'Seq: set_camera_cut', toolName: 'manage_sequence', arguments: { action: 'set_camera_cut', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraName: 'MainCam', frame: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: set_camera_lens', toolName: 'manage_sequence', arguments: { action: 'set_camera_lens', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraName: 'MainCam', focalLength: 35.0 }, expected: 'success|not found' },
  { scenario: 'Seq: set_camera_focus', toolName: 'manage_sequence', arguments: { action: 'set_camera_focus', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraName: 'MainCam', focusDistance: 500.0 }, expected: 'success|not found' },
  
  // === Playback ===
  { scenario: 'Seq: play_sequence', toolName: 'manage_sequence', arguments: { action: 'play_sequence', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: pause_sequence', toolName: 'manage_sequence', arguments: { action: 'pause_sequence', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: stop_sequence', toolName: 'manage_sequence', arguments: { action: 'stop_sequence', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_playhead', toolName: 'manage_sequence', arguments: { action: 'set_playhead', sequencePath: `${TEST_FOLDER}/LS_Test`, frame: 50 }, expected: 'success|not found' },
  { scenario: 'Seq: get_playhead', toolName: 'manage_sequence', arguments: { action: 'get_playhead', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  
  // === Shots & Subsequences ===
  { scenario: 'Seq: add_shot', toolName: 'manage_sequence', arguments: { action: 'add_shot', masterSequencePath: `${TEST_FOLDER}/LS_Test`, shotSequencePath: `${TEST_FOLDER}/LS_Shot01`, startFrame: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: create_shot', toolName: 'manage_sequence', arguments: { action: 'create_shot', masterSequencePath: `${TEST_FOLDER}/LS_Test`, shotName: 'Shot01', startFrame: 0, endFrame: 100 }, expected: 'success|not found' },
  { scenario: 'Seq: add_subsequence', toolName: 'manage_sequence', arguments: { action: 'add_subsequence', parentSequencePath: `${TEST_FOLDER}/LS_Test`, childSequencePath: `${TEST_FOLDER}/LS_Child`, startFrame: 100 }, expected: 'success|not found' },
  
  // === Movie Render Queue ===
  { scenario: 'Seq: create_mrq_preset', toolName: 'manage_sequence', arguments: { action: 'create_mrq_preset', name: 'MRQ_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Seq: configure_mrq_output', toolName: 'manage_sequence', arguments: { action: 'configure_mrq_output', presetPath: `${TEST_FOLDER}/MRQ_Test`, outputDirectory: 'C:/Renders', fileNameFormat: '{sequence_name}_{frame_number}' }, expected: 'success|not found' },
  { scenario: 'Seq: add_mrq_image_output', toolName: 'manage_sequence', arguments: { action: 'add_mrq_image_output', presetPath: `${TEST_FOLDER}/MRQ_Test`, format: 'PNG', resolution: { x: 1920, y: 1080 } }, expected: 'success|not found' },
  { scenario: 'Seq: add_mrq_video_output', toolName: 'manage_sequence', arguments: { action: 'add_mrq_video_output', presetPath: `${TEST_FOLDER}/MRQ_Test`, format: 'ProRes', codec: '422' }, expected: 'success|not found' },
  { scenario: 'Seq: render_sequence', toolName: 'manage_sequence', arguments: { action: 'render_sequence', sequencePath: `${TEST_FOLDER}/LS_Test`, presetPath: `${TEST_FOLDER}/MRQ_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: get_render_status', toolName: 'manage_sequence', arguments: { action: 'get_render_status' }, expected: 'success' },
  
  // === Sections ===
  { scenario: 'Seq: add_section', toolName: 'manage_sequence', arguments: { action: 'add_section', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', startFrame: 0, endFrame: 100 }, expected: 'success|not found' },
  { scenario: 'Seq: trim_section', toolName: 'manage_sequence', arguments: { action: 'trim_section', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', newStart: 10, newEnd: 90 }, expected: 'success|not found' },
  { scenario: 'Seq: delete_section', toolName: 'manage_sequence', arguments: { action: 'delete_section', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', sectionIndex: 0 }, expected: 'success|not found' },
  
  // === Core Sequence Management ===
  { scenario: 'Seq: create', toolName: 'manage_sequence', arguments: { action: 'create', name: 'LS_Core', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Seq: open', toolName: 'manage_sequence', arguments: { action: 'open', sequencePath: `${TEST_FOLDER}/LS_Core` }, expected: 'success|not found' },
  { scenario: 'Seq: duplicate', toolName: 'manage_sequence', arguments: { action: 'duplicate', sequencePath: `${TEST_FOLDER}/LS_Core`, destinationPath: TEST_FOLDER, newName: 'LS_CoreCopy' }, expected: 'success|not found' },
  { scenario: 'Seq: rename', toolName: 'manage_sequence', arguments: { action: 'rename', sequencePath: `${TEST_FOLDER}/LS_CoreCopy`, newName: 'LS_Renamed' }, expected: 'success|not found' },
  { scenario: 'Seq: delete', toolName: 'manage_sequence', arguments: { action: 'delete', sequencePath: `${TEST_FOLDER}/LS_Renamed` }, expected: 'success|not found' },
  { scenario: 'Seq: list', toolName: 'manage_sequence', arguments: { action: 'list', path: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Seq: get_metadata', toolName: 'manage_sequence', arguments: { action: 'get_metadata', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_metadata', toolName: 'manage_sequence', arguments: { action: 'set_metadata', sequencePath: `${TEST_FOLDER}/LS_Test`, metadata: { author: 'TestUser', notes: 'Test sequence' } }, expected: 'success|not found' },
  { scenario: 'Seq: create_master_sequence', toolName: 'manage_sequence', arguments: { action: 'create_master_sequence', name: 'LS_Master', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Seq: export_sequence', toolName: 'manage_sequence', arguments: { action: 'export_sequence', sequencePath: `${TEST_FOLDER}/LS_Test`, exportPath: 'C:/Exports/LS_Test.fbx', exportFormat: 'FBX' }, expected: 'success|not found' },
  
  // === Actor Binding ===
  { scenario: 'Seq: add_actor', toolName: 'manage_sequence', arguments: { action: 'add_actor', sequencePath: `${TEST_FOLDER}/LS_Test`, actorName: 'TestActor' }, expected: 'success|not found' },
  { scenario: 'Seq: add_actors', toolName: 'manage_sequence', arguments: { action: 'add_actors', sequencePath: `${TEST_FOLDER}/LS_Test`, actorNames: ['Actor1', 'Actor2'] }, expected: 'success|not found' },
  { scenario: 'Seq: remove_actors', toolName: 'manage_sequence', arguments: { action: 'remove_actors', sequencePath: `${TEST_FOLDER}/LS_Test`, actorNames: ['Actor1'] }, expected: 'success|not found' },
  { scenario: 'Seq: bind_actor', toolName: 'manage_sequence', arguments: { action: 'bind_actor', sequencePath: `${TEST_FOLDER}/LS_Test`, actorName: 'TestActor' }, expected: 'success|not found' },
  { scenario: 'Seq: unbind_actor', toolName: 'manage_sequence', arguments: { action: 'unbind_actor', sequencePath: `${TEST_FOLDER}/LS_Test`, actorName: 'TestActor' }, expected: 'success|not found' },
  { scenario: 'Seq: add_spawnable_from_class', toolName: 'manage_sequence', arguments: { action: 'add_spawnable_from_class', sequencePath: `${TEST_FOLDER}/LS_Test`, className: '/Script/Engine.PointLight' }, expected: 'success|not found' },
  
  // === Tracks & Sections ===
  { scenario: 'Seq: add_track', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Transform' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_track', toolName: 'manage_sequence', arguments: { action: 'remove_track', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform' }, expected: 'success|not found' },
  { scenario: 'Seq: list_tracks', toolName: 'manage_sequence', arguments: { action: 'list_tracks', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: list_track_types', toolName: 'manage_sequence', arguments: { action: 'list_track_types' }, expected: 'success' },
  { scenario: 'Seq: remove_section', toolName: 'manage_sequence', arguments: { action: 'remove_section', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', sectionIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: get_tracks', toolName: 'manage_sequence', arguments: { action: 'get_tracks', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_track_muted', toolName: 'manage_sequence', arguments: { action: 'set_track_muted', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', muted: true }, expected: 'success|not found' },
  { scenario: 'Seq: set_track_solo', toolName: 'manage_sequence', arguments: { action: 'set_track_solo', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', solo: true }, expected: 'success|not found' },
  { scenario: 'Seq: set_track_locked', toolName: 'manage_sequence', arguments: { action: 'set_track_locked', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', locked: true }, expected: 'success|not found' },
  
  // === Shot Tracks ===
  { scenario: 'Seq: remove_shot', toolName: 'manage_sequence', arguments: { action: 'remove_shot', sequencePath: `${TEST_FOLDER}/LS_Test`, shotName: 'Shot01' }, expected: 'success|not found' },
  { scenario: 'Seq: get_shots', toolName: 'manage_sequence', arguments: { action: 'get_shots', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: remove_subsequence', toolName: 'manage_sequence', arguments: { action: 'remove_subsequence', sequencePath: `${TEST_FOLDER}/LS_Test`, subsequencePath: `${TEST_FOLDER}/LS_Child` }, expected: 'success|not found' },
  { scenario: 'Seq: get_subsequences', toolName: 'manage_sequence', arguments: { action: 'get_subsequences', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  
  // === Camera ===
  { scenario: 'Seq: add_camera', toolName: 'manage_sequence', arguments: { action: 'add_camera', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1' }, expected: 'success|not found' },
  { scenario: 'Seq: create_cine_camera_actor', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', name: 'CineCamera2', location: { x: 0, y: 0, z: 200 } }, expected: 'success' },
  { scenario: 'Seq: configure_camera_settings', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', focalLength: 50.0, aperture: 2.8 }, expected: 'success|not found' },
  { scenario: 'Seq: add_camera_cut', toolName: 'manage_sequence', arguments: { action: 'add_camera_cut', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', frame: 0 }, expected: 'success|not found' },
  
  // === Properties & Timing ===
  { scenario: 'Seq: get_properties', toolName: 'manage_sequence', arguments: { action: 'get_properties', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_properties', toolName: 'manage_sequence', arguments: { action: 'set_properties', sequencePath: `${TEST_FOLDER}/LS_Test`, displayRate: 30, tickResolution: 24000 }, expected: 'success|not found' },
  { scenario: 'Seq: set_display_rate', toolName: 'manage_sequence', arguments: { action: 'set_display_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, displayRate: 24 }, expected: 'success|not found' },
  { scenario: 'Seq: set_tick_resolution', toolName: 'manage_sequence', arguments: { action: 'set_tick_resolution', sequencePath: `${TEST_FOLDER}/LS_Test`, tickResolution: 24000 }, expected: 'success|not found' },
  { scenario: 'Seq: set_work_range', toolName: 'manage_sequence', arguments: { action: 'set_work_range', sequencePath: `${TEST_FOLDER}/LS_Test`, start: 0, end: 1000 }, expected: 'success|not found' },
  { scenario: 'Seq: set_view_range', toolName: 'manage_sequence', arguments: { action: 'set_view_range', sequencePath: `${TEST_FOLDER}/LS_Test`, start: 0, end: 500 }, expected: 'success|not found' },
  { scenario: 'Seq: get_playback_range', toolName: 'manage_sequence', arguments: { action: 'get_playback_range', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  
  // === Playback Control ===
  { scenario: 'Seq: play', toolName: 'manage_sequence', arguments: { action: 'play', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: pause', toolName: 'manage_sequence', arguments: { action: 'pause', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: stop', toolName: 'manage_sequence', arguments: { action: 'stop', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_playback_speed', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', sequencePath: `${TEST_FOLDER}/LS_Test`, speed: 2.0 }, expected: 'success|not found' },
  { scenario: 'Seq: scrub_to_time', toolName: 'manage_sequence', arguments: { action: 'scrub_to_time', sequencePath: `${TEST_FOLDER}/LS_Test`, time: 5.0 }, expected: 'success|not found' },
  
  // === Movie Render Queue ===
  { scenario: 'Seq: create_queue', toolName: 'manage_sequence', arguments: { action: 'create_queue' }, expected: 'success' },
  { scenario: 'Seq: add_job', toolName: 'manage_sequence', arguments: { action: 'add_job', sequencePath: `${TEST_FOLDER}/LS_Test`, jobName: 'TestRender' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_job', toolName: 'manage_sequence', arguments: { action: 'remove_job', jobIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: clear_queue', toolName: 'manage_sequence', arguments: { action: 'clear_queue' }, expected: 'success' },
  { scenario: 'Seq: get_queue', toolName: 'manage_sequence', arguments: { action: 'get_queue' }, expected: 'success' },
  { scenario: 'Seq: configure_job', toolName: 'manage_sequence', arguments: { action: 'configure_job', jobIndex: 0, outputDirectory: 'C:/Renders' }, expected: 'success|not found' },
  { scenario: 'Seq: set_sequence', toolName: 'manage_sequence', arguments: { action: 'set_sequence', jobIndex: 0, sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: set_map', toolName: 'manage_sequence', arguments: { action: 'set_map', jobIndex: 0, mapPath: '/Game/Maps/TestMap' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_output', toolName: 'manage_sequence', arguments: { action: 'configure_output', jobIndex: 0, outputFormat: 'PNG', resolutionX: 1920, resolutionY: 1080 }, expected: 'success|not found' },
  { scenario: 'Seq: set_resolution', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 3840, resolutionY: 2160 }, expected: 'success|not found' },
  { scenario: 'Seq: set_output_directory', toolName: 'manage_sequence', arguments: { action: 'set_output_directory', jobIndex: 0, outputDirectory: 'C:/Renders/4K' }, expected: 'success|not found' },
  { scenario: 'Seq: set_file_name_format', toolName: 'manage_sequence', arguments: { action: 'set_file_name_format', jobIndex: 0, fileNameFormat: '{sequence}_{frame}' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'FinalImage' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_render_pass', toolName: 'manage_sequence', arguments: { action: 'remove_render_pass', jobIndex: 0, passName: 'FinalImage' }, expected: 'success|not found' },
  { scenario: 'Seq: get_render_passes', toolName: 'manage_sequence', arguments: { action: 'get_render_passes', jobIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: configure_render_pass', toolName: 'manage_sequence', arguments: { action: 'configure_render_pass', jobIndex: 0, passName: 'FinalImage', passEnabled: true }, expected: 'success|not found' },
  { scenario: 'Seq: configure_anti_aliasing', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', jobIndex: 0, overrideAntiAliasing: true, antiAliasingMethod: 'TAA' }, expected: 'success|not found' },
  { scenario: 'Seq: set_spatial_sample_count', toolName: 'manage_sequence', arguments: { action: 'set_spatial_sample_count', jobIndex: 0, spatialSampleCount: 8 }, expected: 'success|not found' },
  { scenario: 'Seq: set_temporal_sample_count', toolName: 'manage_sequence', arguments: { action: 'set_temporal_sample_count', jobIndex: 0, temporalSampleCount: 16 }, expected: 'success|not found' },
  { scenario: 'Seq: add_burn_in', toolName: 'manage_sequence', arguments: { action: 'add_burn_in', jobIndex: 0, burnInText: 'Test Render' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_burn_in', toolName: 'manage_sequence', arguments: { action: 'remove_burn_in', jobIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: configure_burn_in', toolName: 'manage_sequence', arguments: { action: 'configure_burn_in', jobIndex: 0, burnInPosition: 'BottomLeft' }, expected: 'success|not found' },
  { scenario: 'Seq: start_render', toolName: 'manage_sequence', arguments: { action: 'start_render' }, expected: 'success|no jobs' },
  { scenario: 'Seq: stop_render', toolName: 'manage_sequence', arguments: { action: 'stop_render' }, expected: 'success|not rendering' },
  { scenario: 'Seq: get_render_progress', toolName: 'manage_sequence', arguments: { action: 'get_render_progress' }, expected: 'success' },
  { scenario: 'Seq: add_console_variable', toolName: 'manage_sequence', arguments: { action: 'add_console_variable', jobIndex: 0, cvarName: 'r.ScreenPercentage', cvarValue: '100' }, expected: 'success|not found' },
  { scenario: 'Seq: remove_console_variable', toolName: 'manage_sequence', arguments: { action: 'remove_console_variable', jobIndex: 0, cvarName: 'r.ScreenPercentage' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_high_res_settings', toolName: 'manage_sequence', arguments: { action: 'configure_high_res_settings', jobIndex: 0, tileCountX: 4, tileCountY: 4 }, expected: 'success|not found' },
  { scenario: 'Seq: set_tile_count', toolName: 'manage_sequence', arguments: { action: 'set_tile_count', jobIndex: 0, tileCountX: 2, tileCountY: 2 }, expected: 'success|not found' },
  
  // === Wave 4.11-4.20: Sequencer Enhancement Actions ===
  { scenario: 'Seq: create_media_track', toolName: 'manage_sequence', arguments: { action: 'create_media_track', sequencePath: `${TEST_FOLDER}/LS_Test`, mediaPath: 'C:/Media/video.mp4', mediaSourceName: 'BackgroundVideo' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_sequence_streaming', toolName: 'manage_sequence', arguments: { action: 'configure_sequence_streaming', sequencePath: `${TEST_FOLDER}/LS_Test`, streamingSettings: { preloadFrames: 30 } }, expected: 'success|not found' },
  { scenario: 'Seq: create_event_trigger_track', toolName: 'manage_sequence', arguments: { action: 'create_event_trigger_track', sequencePath: `${TEST_FOLDER}/LS_Test`, eventTriggerName: 'OnExplosion' }, expected: 'success|not found' },
  { scenario: 'Seq: add_procedural_camera_shake', toolName: 'manage_sequence', arguments: { action: 'add_procedural_camera_shake', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', shakeIntensity: 0.5, shakeDuration: 2.0 }, expected: 'success|not found' },
  { scenario: 'Seq: configure_sequence_lod', toolName: 'manage_sequence', arguments: { action: 'configure_sequence_lod', sequencePath: `${TEST_FOLDER}/LS_Test`, lodBias: 0, forceLod: 0 }, expected: 'success|not found' },
  { scenario: 'Seq: create_camera_cut_track', toolName: 'manage_sequence', arguments: { action: 'create_camera_cut_track', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: configure_mrq_settings', toolName: 'manage_sequence', arguments: { action: 'configure_mrq_settings', mrqPreset: 'HighQuality', mrqSettings: { antiAliasing: 'TAA' } }, expected: 'success|not found' },
  { scenario: 'Seq: batch_render_sequences', toolName: 'manage_sequence', arguments: { action: 'batch_render_sequences', sequencePaths: [`${TEST_FOLDER}/LS_Test`], outputSettings: { format: 'PNG' } }, expected: 'success|not found' },
  { scenario: 'Seq: get_sequence_bindings', toolName: 'manage_sequence', arguments: { action: 'get_sequence_bindings', sequencePath: `${TEST_FOLDER}/LS_Test` }, expected: 'success|not found' },
  { scenario: 'Seq: configure_audio_track', toolName: 'manage_sequence', arguments: { action: 'configure_audio_track', sequencePath: `${TEST_FOLDER}/LS_Test`, audioTrackName: 'MusicTrack', audioAssetPath: '/Game/Audio/Music.uasset' }, expected: 'success|not found' },
  
  // === Additional Sequence Actions (Expanded Coverage) ===
  // More Keyframe Operations
  { scenario: 'Seq: add_keyframe_rotation', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 50, value: { roll: 0, pitch: 0, yaw: 45 } }, expected: 'success|not found' },
  { scenario: 'Seq: add_keyframe_scale', toolName: 'manage_sequence', arguments: { action: 'add_keyframe', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 75, value: { x: 1.5, y: 1.5, z: 1.5 } }, expected: 'success|not found' },
  { scenario: 'Seq: set_keyframe_interpolation_linear', toolName: 'manage_sequence', arguments: { action: 'set_keyframe_interpolation', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 50, interpolation: 'Linear' }, expected: 'success|not found' },
  { scenario: 'Seq: set_keyframe_interpolation_constant', toolName: 'manage_sequence', arguments: { action: 'set_keyframe_interpolation', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 75, interpolation: 'Constant' }, expected: 'success|not found' },
  { scenario: 'Seq: set_keyframe_interpolation_auto', toolName: 'manage_sequence', arguments: { action: 'set_keyframe_interpolation', sequencePath: `${TEST_FOLDER}/LS_Test`, trackName: 'Transform', frame: 100, interpolation: 'Auto' }, expected: 'success|not found' },
  
  // More Track Types
  { scenario: 'Seq: add_track_visibility', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Visibility' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_color', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Color' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_float', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Float' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_bool', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Bool' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_material', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Material' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_particle', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Particle' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_skeletal_animation', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'SkeletalAnimation' }, expected: 'success|not found' },
  { scenario: 'Seq: add_track_audio', toolName: 'manage_sequence', arguments: { action: 'add_track', sequencePath: `${TEST_FOLDER}/LS_Test`, bindingName: 'TestActor', trackType: 'Audio' }, expected: 'success|not found' },
  
  // More Camera Operations
  { scenario: 'Seq: configure_camera_aperture', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', aperture: 1.4 }, expected: 'success|not found' },
  { scenario: 'Seq: configure_camera_focusdistance', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', focusDistance: 1000.0 }, expected: 'success|not found' },
  { scenario: 'Seq: configure_camera_sensornm', toolName: 'manage_sequence', arguments: { action: 'configure_camera_settings', sequencePath: `${TEST_FOLDER}/LS_Test`, cameraActorName: 'CineCamera1', sensorWidth: 36.0, sensorHeight: 24.0 }, expected: 'success|not found' },
  { scenario: 'Seq: create_cine_camera_actor_rot', toolName: 'manage_sequence', arguments: { action: 'create_cine_camera_actor', name: 'CineCamera3', location: { x: 500, y: 0, z: 150 }, rotation: { pitch: -10, yaw: 180, roll: 0 } }, expected: 'success' },
  
  // More MRQ Operations
  { scenario: 'Seq: add_render_pass_objectid', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'ObjectId' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_worldnormal', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'WorldNormal' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_depth', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'SceneDepth' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_basecolor', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'BaseColor' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_roughness', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'Roughness' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_metallic', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'Metallic' }, expected: 'success|not found' },
  { scenario: 'Seq: add_render_pass_ambientocclusion', toolName: 'manage_sequence', arguments: { action: 'add_render_pass', jobIndex: 0, passType: 'AmbientOcclusion' }, expected: 'success|not found' },
  
  // MRQ Quality Settings
  { scenario: 'Seq: configure_anti_aliasing_fxaa', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', jobIndex: 0, overrideAntiAliasing: true, antiAliasingMethod: 'FXAA' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_anti_aliasing_msaa', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', jobIndex: 0, overrideAntiAliasing: true, antiAliasingMethod: 'MSAA' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_anti_aliasing_tsr', toolName: 'manage_sequence', arguments: { action: 'configure_anti_aliasing', jobIndex: 0, overrideAntiAliasing: true, antiAliasingMethod: 'TSR' }, expected: 'success|not found' },
  { scenario: 'Seq: set_spatial_sample_16', toolName: 'manage_sequence', arguments: { action: 'set_spatial_sample_count', jobIndex: 0, spatialSampleCount: 16 }, expected: 'success|not found' },
  { scenario: 'Seq: set_temporal_sample_32', toolName: 'manage_sequence', arguments: { action: 'set_temporal_sample_count', jobIndex: 0, temporalSampleCount: 32 }, expected: 'success|not found' },
  
  // Resolution Presets
  { scenario: 'Seq: set_resolution_hd', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 1280, resolutionY: 720 }, expected: 'success|not found' },
  { scenario: 'Seq: set_resolution_4k', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 3840, resolutionY: 2160 }, expected: 'success|not found' },
  { scenario: 'Seq: set_resolution_8k', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 7680, resolutionY: 4320 }, expected: 'success|not found' },
  { scenario: 'Seq: set_resolution_square', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 1080, resolutionY: 1080 }, expected: 'success|not found' },
  { scenario: 'Seq: set_resolution_ultrawide', toolName: 'manage_sequence', arguments: { action: 'set_resolution', jobIndex: 0, resolutionX: 3440, resolutionY: 1440 }, expected: 'success|not found' },
  
  // Frame Rate Presets
  { scenario: 'Seq: set_frame_rate_24', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 24 }, expected: 'success|not found' },
  { scenario: 'Seq: set_frame_rate_25', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 25 }, expected: 'success|not found' },
  { scenario: 'Seq: set_frame_rate_48', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 48 }, expected: 'success|not found' },
  { scenario: 'Seq: set_frame_rate_60', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 60 }, expected: 'success|not found' },
  { scenario: 'Seq: set_frame_rate_120', toolName: 'manage_sequence', arguments: { action: 'set_frame_rate', sequencePath: `${TEST_FOLDER}/LS_Test`, frameRate: 120 }, expected: 'success|not found' },
  
  // Tick Resolution
  { scenario: 'Seq: set_tick_resolution_48000', toolName: 'manage_sequence', arguments: { action: 'set_tick_resolution', sequencePath: `${TEST_FOLDER}/LS_Test`, tickResolution: 48000 }, expected: 'success|not found' },
  { scenario: 'Seq: set_tick_resolution_96000', toolName: 'manage_sequence', arguments: { action: 'set_tick_resolution', sequencePath: `${TEST_FOLDER}/LS_Test`, tickResolution: 96000 }, expected: 'success|not found' },
  
  // Playback Speed Variations
  { scenario: 'Seq: set_playback_speed_half', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', sequencePath: `${TEST_FOLDER}/LS_Test`, speed: 0.5 }, expected: 'success|not found' },
  { scenario: 'Seq: set_playback_speed_quarter', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', sequencePath: `${TEST_FOLDER}/LS_Test`, speed: 0.25 }, expected: 'success|not found' },
  { scenario: 'Seq: set_playback_speed_4x', toolName: 'manage_sequence', arguments: { action: 'set_playback_speed', sequencePath: `${TEST_FOLDER}/LS_Test`, speed: 4.0 }, expected: 'success|not found' },
  
  // Scrub and Playhead Operations
  { scenario: 'Seq: scrub_to_time_start', toolName: 'manage_sequence', arguments: { action: 'scrub_to_time', sequencePath: `${TEST_FOLDER}/LS_Test`, time: 0.0 }, expected: 'success|not found' },
  { scenario: 'Seq: scrub_to_time_mid', toolName: 'manage_sequence', arguments: { action: 'scrub_to_time', sequencePath: `${TEST_FOLDER}/LS_Test`, time: 2.5 }, expected: 'success|not found' },
  { scenario: 'Seq: scrub_to_time_end', toolName: 'manage_sequence', arguments: { action: 'scrub_to_time', sequencePath: `${TEST_FOLDER}/LS_Test`, time: 10.0 }, expected: 'success|not found' },
  { scenario: 'Seq: set_playhead_frame_100', toolName: 'manage_sequence', arguments: { action: 'set_playhead', sequencePath: `${TEST_FOLDER}/LS_Test`, frame: 100 }, expected: 'success|not found' },
  { scenario: 'Seq: set_playhead_frame_200', toolName: 'manage_sequence', arguments: { action: 'set_playhead', sequencePath: `${TEST_FOLDER}/LS_Test`, frame: 200 }, expected: 'success|not found' },
  
  // Multiple Shots
  { scenario: 'Seq: create_shot_02', toolName: 'manage_sequence', arguments: { action: 'create_shot', masterSequencePath: `${TEST_FOLDER}/LS_Test`, shotName: 'Shot02', startFrame: 100, endFrame: 200 }, expected: 'success|not found' },
  { scenario: 'Seq: create_shot_03', toolName: 'manage_sequence', arguments: { action: 'create_shot', masterSequencePath: `${TEST_FOLDER}/LS_Test`, shotName: 'Shot03', startFrame: 200, endFrame: 300 }, expected: 'success|not found' },
  { scenario: 'Seq: create_shot_04', toolName: 'manage_sequence', arguments: { action: 'create_shot', masterSequencePath: `${TEST_FOLDER}/LS_Test`, shotName: 'Shot04', startFrame: 300, endFrame: 400 }, expected: 'success|not found' },
  
  // Burn-In Variations
  { scenario: 'Seq: configure_burn_in_topright', toolName: 'manage_sequence', arguments: { action: 'configure_burn_in', jobIndex: 0, burnInPosition: 'TopRight' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_burn_in_bottomright', toolName: 'manage_sequence', arguments: { action: 'configure_burn_in', jobIndex: 0, burnInPosition: 'BottomRight' }, expected: 'success|not found' },
  { scenario: 'Seq: configure_burn_in_center', toolName: 'manage_sequence', arguments: { action: 'configure_burn_in', jobIndex: 0, burnInPosition: 'Center' }, expected: 'success|not found' },
  { scenario: 'Seq: add_burn_in_timecode', toolName: 'manage_sequence', arguments: { action: 'add_burn_in', jobIndex: 0, burnInText: '{timecode}' }, expected: 'success|not found' },
  { scenario: 'Seq: add_burn_in_frame', toolName: 'manage_sequence', arguments: { action: 'add_burn_in', jobIndex: 0, burnInText: 'Frame: {frame}' }, expected: 'success|not found' },
  
  // Console Variables
  { scenario: 'Seq: add_console_variable_raytracing', toolName: 'manage_sequence', arguments: { action: 'add_console_variable', jobIndex: 0, cvarName: 'r.RayTracing', cvarValue: '1' }, expected: 'success|not found' },
  { scenario: 'Seq: add_console_variable_lumen', toolName: 'manage_sequence', arguments: { action: 'add_console_variable', jobIndex: 0, cvarName: 'r.Lumen.DiffuseIndirect.Allow', cvarValue: '1' }, expected: 'success|not found' },
  { scenario: 'Seq: add_console_variable_nanite', toolName: 'manage_sequence', arguments: { action: 'add_console_variable', jobIndex: 0, cvarName: 'r.Nanite', cvarValue: '1' }, expected: 'success|not found' },
  { scenario: 'Seq: add_console_variable_vsm', toolName: 'manage_sequence', arguments: { action: 'add_console_variable', jobIndex: 0, cvarName: 'r.Shadow.Virtual.Enable', cvarValue: '1' }, expected: 'success|not found' },
  
  // Tile Rendering
  { scenario: 'Seq: configure_high_res_settings_8x8', toolName: 'manage_sequence', arguments: { action: 'configure_high_res_settings', jobIndex: 0, tileCountX: 8, tileCountY: 8 }, expected: 'success|not found' },
  { scenario: 'Seq: set_tile_count_1x1', toolName: 'manage_sequence', arguments: { action: 'set_tile_count', jobIndex: 0, tileCountX: 1, tileCountY: 1 }, expected: 'success|not found' },
  { scenario: 'Seq: set_tile_count_4x4', toolName: 'manage_sequence', arguments: { action: 'set_tile_count', jobIndex: 0, tileCountX: 4, tileCountY: 4 }, expected: 'success|not found' },
];

// ============================================================================
// MANAGE_WIDGET_AUTHORING (73 actions) - EXPANDED FULL COVERAGE
// ============================================================================
const manageWidgetAuthoringTests = [
  // === Widget Blueprint Creation ===
  { scenario: 'Widget: create_widget_blueprint', toolName: 'manage_widget_authoring', arguments: { action: 'create_widget_blueprint', name: 'WBP_Test', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: create_widget_blueprint_hud', toolName: 'manage_widget_authoring', arguments: { action: 'create_widget_blueprint', name: 'WBP_HUD', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: create_widget_blueprint_menu', toolName: 'manage_widget_authoring', arguments: { action: 'create_widget_blueprint', name: 'WBP_MainMenu', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: duplicate_widget_blueprint', toolName: 'manage_widget_authoring', arguments: { action: 'duplicate_widget_blueprint', sourcePath: `${TEST_FOLDER}/WBP_Test`, destPath: TEST_FOLDER, newName: 'WBP_TestCopy' }, expected: 'success|not found' },
  { scenario: 'Widget: open_widget_editor', toolName: 'manage_widget_authoring', arguments: { action: 'open_widget_editor', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: close_widget_editor', toolName: 'manage_widget_authoring', arguments: { action: 'close_widget_editor', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: get_widget_tree', toolName: 'manage_widget_authoring', arguments: { action: 'get_widget_tree', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: get_widget_info', toolName: 'manage_widget_authoring', arguments: { action: 'get_widget_info', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: list_widget_blueprints', toolName: 'manage_widget_authoring', arguments: { action: 'list_widget_blueprints', path: TEST_FOLDER }, expected: 'success' },
  
  // === Panel Widgets ===
  { scenario: 'Widget: add_canvas_panel', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'CanvasPanel', name: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_vertical_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'VerticalBox', name: 'MenuLayout', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_horizontal_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'HorizontalBox', name: 'ButtonRow', parentName: 'MenuLayout' }, expected: 'success|not found' },
  { scenario: 'Widget: add_grid_panel', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'GridPanel', name: 'InventoryGrid', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_scroll_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ScrollBox', name: 'MessageScroll', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_overlay', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Overlay', name: 'OverlayContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_size_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'SizeBox', name: 'SizeContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_scale_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ScaleBox', name: 'ScaleContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_border', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Border', name: 'BorderContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_uniform_grid_panel', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'UniformGridPanel', name: 'UniformGrid', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_wrap_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'WrapBox', name: 'WrapContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_widget_switcher', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'WidgetSwitcher', name: 'PageSwitcher', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_safe_zone', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'SafeZone', name: 'SafeZoneContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  
  // === Common Widgets ===
  { scenario: 'Widget: add_text_block', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'TextBlock', name: 'TitleText', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_button', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Button', name: 'PlayButton', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_image', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Image', name: 'BackgroundImage', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_progress_bar', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ProgressBar', name: 'HealthBar', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_check_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'CheckBox', name: 'SoundToggle', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_slider', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Slider', name: 'VolumeSlider', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_spinner', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'SpinBox', name: 'ValueSpinner', parentName: 'RootCanvas' }, expected: 'success|not found' },
  
  // === Input Widgets ===
  { scenario: 'Widget: add_editable_text', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'EditableText', name: 'NameInput', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_editable_text_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'EditableTextBox', name: 'PasswordInput', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_multi_line_editable_text', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'MultiLineEditableText', name: 'ChatInput', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_combo_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ComboBox', name: 'ResolutionDropdown', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_combo_box_string', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ComboBoxString', name: 'LanguageDropdown', parentName: 'RootCanvas' }, expected: 'success|not found' },
  
  // === List Widgets ===
  { scenario: 'Widget: add_list_view', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'ListView', name: 'ItemList', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_tile_view', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'TileView', name: 'InventoryTiles', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_tree_view', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'TreeView', name: 'SkillTree', parentName: 'RootCanvas' }, expected: 'success|not found' },
  
  // === Special Widgets ===
  { scenario: 'Widget: add_spacer', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Spacer', name: 'Spacer1', parentName: 'MenuLayout' }, expected: 'success|not found' },
  { scenario: 'Widget: add_named_slot', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'NamedSlot', name: 'ContentSlot', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_retainer_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'RetainerBox', name: 'CachedWidget', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_throbber', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'Throbber', name: 'LoadingIndicator', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_circular_throbber', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'CircularThrobber', name: 'CircularLoader', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_rich_text_block', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'RichTextBlock', name: 'FormattedText', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_input_key_selector', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'InputKeySelector', name: 'KeyBindingSelector', parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: add_invalidation_box', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetType: 'InvalidationBox', name: 'InvalidationContainer', parentName: 'RootCanvas' }, expected: 'success|not found' },
  
  // === Widget Position & Size ===
  { scenario: 'Widget: set_widget_position', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_position', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', position: { x: 100, y: 50 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_size', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_size', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', size: { x: 200, y: 50 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_anchor', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_anchor', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', anchor: 'TopCenter' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_anchor_topleft', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_anchor', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', anchor: 'TopLeft' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_anchor_bottomright', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_anchor', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', anchor: 'BottomRight' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_anchor_stretch', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_anchor', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', anchor: 'Stretch' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_alignment', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_alignment', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', alignment: { x: 0.5, y: 0 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_offset', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_offset', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', offset: { left: 10, top: 10, right: 10, bottom: 10 } }, expected: 'success|not found' },
  
  // === Widget Visibility & State ===
  { scenario: 'Widget: set_widget_visibility_visible', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_visibility', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', visibility: 'Visible' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_visibility_hidden', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_visibility', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', visibility: 'Hidden' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_visibility_collapsed', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_visibility', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'SoundToggle', visibility: 'Collapsed' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_visibility_hittestinvisible', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_visibility', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', visibility: 'HitTestInvisible' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_visibility_selfhittestinvisible', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_visibility', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'RootCanvas', visibility: 'SelfHitTestInvisible' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_enabled', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_enabled', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', enabled: true }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_enabled_false', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_enabled', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', enabled: false }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_render_opacity', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_render_opacity', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', opacity: 0.8 }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_render_transform', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_render_transform', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', translation: { x: 10, y: 5 }, angle: 5, scale: { x: 1.1, y: 1.1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_render_pivot', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_render_pivot', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', pivot: { x: 0.5, y: 0.5 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_clipping', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_clipping', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'MessageScroll', clipping: 'ClipToBounds' }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_navigation', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_navigation', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', navigation: { up: 'VolumeSlider', down: 'SoundToggle' } }, expected: 'success|not found' },
  
  // === Text Widget Properties ===
  { scenario: 'Widget: set_text_content', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_content', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', text: 'Game Title' }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_font', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_font', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', fontFamily: 'Roboto', fontSize: 48 }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_color', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_color', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', color: { r: 1, g: 1, b: 1, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_justification', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_justification', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', justification: 'Center' }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_justification_left', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_justification', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', justification: 'Left' }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_justification_right', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_justification', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', justification: 'Right' }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_wrapping', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_wrapping', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', autoWrap: true, wrapAt: 0 }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_shadow', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_shadow', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', shadowOffset: { x: 2, y: 2 }, shadowColor: { r: 0, g: 0, b: 0, a: 0.5 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_outline', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_outline', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', outlineSize: 2, outlineColor: { r: 0, g: 0, b: 0, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_text_strikethrough', toolName: 'manage_widget_authoring', arguments: { action: 'set_text_strikethrough', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', strikethrough: true }, expected: 'success|not found' },
  
  // === Button Widget Properties ===
  { scenario: 'Widget: set_button_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_button_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', normalColor: { r: 0.2, g: 0.4, b: 0.8, a: 1 }, hoveredColor: { r: 0.3, g: 0.5, b: 0.9, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_button_pressed_color', toolName: 'manage_widget_authoring', arguments: { action: 'set_button_pressed_color', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', pressedColor: { r: 0.1, g: 0.3, b: 0.7, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_button_disabled_color', toolName: 'manage_widget_authoring', arguments: { action: 'set_button_disabled_color', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', disabledColor: { r: 0.3, g: 0.3, b: 0.3, a: 0.5 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_button_click_method', toolName: 'manage_widget_authoring', arguments: { action: 'set_button_click_method', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', clickMethod: 'DownAndUp' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_button_click', toolName: 'manage_widget_authoring', arguments: { action: 'bind_button_click', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', functionName: 'OnPlayClicked' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_button_hover', toolName: 'manage_widget_authoring', arguments: { action: 'bind_button_hover', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', hoverFunction: 'OnPlayHovered', unhoverFunction: 'OnPlayUnhovered' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_button_pressed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_button_pressed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', pressedFunction: 'OnPlayPressed', releasedFunction: 'OnPlayReleased' }, expected: 'success|not found' },
  
  // === Image Widget Properties ===
  { scenario: 'Widget: set_image_brush', toolName: 'manage_widget_authoring', arguments: { action: 'set_image_brush', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', texturePath: '/Engine/EngineResources/DefaultTexture' }, expected: 'success|not found' },
  { scenario: 'Widget: set_image_color', toolName: 'manage_widget_authoring', arguments: { action: 'set_image_color', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', color: { r: 0.1, g: 0.1, b: 0.1, a: 0.8 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_image_size', toolName: 'manage_widget_authoring', arguments: { action: 'set_image_size', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', size: { x: 512, y: 512 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_image_uv', toolName: 'manage_widget_authoring', arguments: { action: 'set_image_uv', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', uvRegion: { min: { x: 0, y: 0 }, max: { x: 1, y: 1 } } }, expected: 'success|not found' },
  
  // === Progress Bar Properties ===
  { scenario: 'Widget: set_progress_percent', toolName: 'manage_widget_authoring', arguments: { action: 'set_progress_percent', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', percent: 0.75 }, expected: 'success|not found' },
  { scenario: 'Widget: set_progress_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_progress_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', fillColor: { r: 0, g: 1, b: 0, a: 1 }, backgroundColor: { r: 0.3, g: 0.3, b: 0.3, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: set_progress_direction', toolName: 'manage_widget_authoring', arguments: { action: 'set_progress_direction', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', direction: 'LeftToRight' }, expected: 'success|not found' },
  { scenario: 'Widget: set_progress_fill_type', toolName: 'manage_widget_authoring', arguments: { action: 'set_progress_fill_type', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', fillType: 'LeftToRight' }, expected: 'success|not found' },
  
  // === Slider Properties ===
  { scenario: 'Widget: set_slider_value', toolName: 'manage_widget_authoring', arguments: { action: 'set_slider_value', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', value: 0.5 }, expected: 'success|not found' },
  { scenario: 'Widget: set_slider_range', toolName: 'manage_widget_authoring', arguments: { action: 'set_slider_range', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', minValue: 0, maxValue: 1 }, expected: 'success|not found' },
  { scenario: 'Widget: set_slider_step_size', toolName: 'manage_widget_authoring', arguments: { action: 'set_slider_step_size', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', stepSize: 0.1 }, expected: 'success|not found' },
  { scenario: 'Widget: set_slider_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_slider_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', barColor: { r: 0.3, g: 0.3, b: 0.3, a: 1 }, handleColor: { r: 1, g: 1, b: 1, a: 1 } }, expected: 'success|not found' },
  { scenario: 'Widget: bind_slider_value_changed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_slider_value_changed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'VolumeSlider', functionName: 'OnVolumeChanged' }, expected: 'success|not found' },
  
  // === CheckBox Properties ===
  { scenario: 'Widget: set_checkbox_checked', toolName: 'manage_widget_authoring', arguments: { action: 'set_checkbox_checked', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'SoundToggle', checked: true }, expected: 'success|not found' },
  { scenario: 'Widget: set_checkbox_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_checkbox_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'SoundToggle', uncheckedImage: '/Engine/EngineResources/DefaultTexture', checkedImage: '/Engine/EngineResources/DefaultTexture' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_checkbox_changed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_checkbox_changed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'SoundToggle', functionName: 'OnSoundToggleChanged' }, expected: 'success|not found' },
  
  // === ComboBox Properties ===
  { scenario: 'Widget: set_combobox_options', toolName: 'manage_widget_authoring', arguments: { action: 'set_combobox_options', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'ResolutionDropdown', options: ['1920x1080', '2560x1440', '3840x2160'] }, expected: 'success|not found' },
  { scenario: 'Widget: set_combobox_selected', toolName: 'manage_widget_authoring', arguments: { action: 'set_combobox_selected', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'ResolutionDropdown', selectedOption: '1920x1080' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_combobox_changed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_combobox_changed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'ResolutionDropdown', functionName: 'OnResolutionChanged' }, expected: 'success|not found' },
  
  // === EditableText Properties ===
  { scenario: 'Widget: set_editable_text', toolName: 'manage_widget_authoring', arguments: { action: 'set_editable_text', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'NameInput', text: 'Player Name' }, expected: 'success|not found' },
  { scenario: 'Widget: set_editable_text_hint', toolName: 'manage_widget_authoring', arguments: { action: 'set_editable_text_hint', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'NameInput', hintText: 'Enter your name...' }, expected: 'success|not found' },
  { scenario: 'Widget: set_editable_text_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_editable_text_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'NameInput', isPassword: false, isReadOnly: false }, expected: 'success|not found' },
  { scenario: 'Widget: bind_text_changed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_text_changed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'NameInput', functionName: 'OnNameTextChanged' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_text_committed', toolName: 'manage_widget_authoring', arguments: { action: 'bind_text_committed', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'NameInput', functionName: 'OnNameTextCommitted' }, expected: 'success|not found' },
  
  // === Scroll Box Properties ===
  { scenario: 'Widget: set_scrollbox_orientation', toolName: 'manage_widget_authoring', arguments: { action: 'set_scrollbox_orientation', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'MessageScroll', orientation: 'Vertical' }, expected: 'success|not found' },
  { scenario: 'Widget: set_scrollbox_scroll_offset', toolName: 'manage_widget_authoring', arguments: { action: 'set_scrollbox_scroll_offset', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'MessageScroll', offset: 100 }, expected: 'success|not found' },
  { scenario: 'Widget: set_scrollbox_always_show_bar', toolName: 'manage_widget_authoring', arguments: { action: 'set_scrollbox_always_show_bar', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'MessageScroll', alwaysShowScrollbar: true }, expected: 'success|not found' },
  
  // === Property Bindings ===
  { scenario: 'Widget: create_property_binding', toolName: 'manage_widget_authoring', arguments: { action: 'create_property_binding', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', propertyName: 'Percent', bindingType: 'Function', functionName: 'GetHealthPercent' }, expected: 'success|not found' },
  { scenario: 'Widget: create_property_binding_attribute', toolName: 'manage_widget_authoring', arguments: { action: 'create_property_binding', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', propertyName: 'Text', bindingType: 'Attribute', attributeName: 'PlayerName' }, expected: 'success|not found' },
  { scenario: 'Widget: remove_binding', toolName: 'manage_widget_authoring', arguments: { action: 'remove_binding', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', propertyName: 'Percent' }, expected: 'success|not found' },
  { scenario: 'Widget: get_bindings', toolName: 'manage_widget_authoring', arguments: { action: 'get_bindings', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar' }, expected: 'success|not found' },
  { scenario: 'Widget: list_all_bindings', toolName: 'manage_widget_authoring', arguments: { action: 'list_all_bindings', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  
  // === Animations ===
  { scenario: 'Widget: create_animation', toolName: 'manage_widget_authoring', arguments: { action: 'create_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  { scenario: 'Widget: create_animation_fadeout', toolName: 'manage_widget_authoring', arguments: { action: 'create_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeOut' }, expected: 'success|not found' },
  { scenario: 'Widget: create_animation_slidein', toolName: 'manage_widget_authoring', arguments: { action: 'create_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'SlideIn' }, expected: 'success|not found' },
  { scenario: 'Widget: delete_animation', toolName: 'manage_widget_authoring', arguments: { action: 'delete_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeOut' }, expected: 'success|not found' },
  { scenario: 'Widget: duplicate_animation', toolName: 'manage_widget_authoring', arguments: { action: 'duplicate_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', newAnimationName: 'FadeIn_Copy' }, expected: 'success|not found' },
  { scenario: 'Widget: add_animation_track', toolName: 'manage_widget_authoring', arguments: { action: 'add_animation_track', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', widgetName: 'RootCanvas', propertyName: 'RenderOpacity' }, expected: 'success|not found' },
  { scenario: 'Widget: add_animation_track_transform', toolName: 'manage_widget_authoring', arguments: { action: 'add_animation_track', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'SlideIn', widgetName: 'RootCanvas', propertyName: 'RenderTransform' }, expected: 'success|not found' },
  { scenario: 'Widget: remove_animation_track', toolName: 'manage_widget_authoring', arguments: { action: 'remove_animation_track', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', trackName: 'RenderOpacity' }, expected: 'success|not found' },
  { scenario: 'Widget: add_animation_key', toolName: 'manage_widget_authoring', arguments: { action: 'add_animation_key', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', time: 0, value: 0 }, expected: 'success|not found' },
  { scenario: 'Widget: add_animation_key_end', toolName: 'manage_widget_authoring', arguments: { action: 'add_animation_key', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', time: 1.0, value: 1 }, expected: 'success|not found' },
  { scenario: 'Widget: remove_animation_key', toolName: 'manage_widget_authoring', arguments: { action: 'remove_animation_key', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', time: 0 }, expected: 'success|not found' },
  { scenario: 'Widget: set_animation_length', toolName: 'manage_widget_authoring', arguments: { action: 'set_animation_length', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', length: 2.0 }, expected: 'success|not found' },
  { scenario: 'Widget: set_animation_looping', toolName: 'manage_widget_authoring', arguments: { action: 'set_animation_looping', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', loop: true }, expected: 'success|not found' },
  { scenario: 'Widget: set_animation_curve', toolName: 'manage_widget_authoring', arguments: { action: 'set_animation_curve', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn', curveType: 'EaseInOut' }, expected: 'success|not found' },
  { scenario: 'Widget: play_animation', toolName: 'manage_widget_authoring', arguments: { action: 'play_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  { scenario: 'Widget: play_animation_reverse', toolName: 'manage_widget_authoring', arguments: { action: 'play_animation_reverse', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  { scenario: 'Widget: pause_animation', toolName: 'manage_widget_authoring', arguments: { action: 'pause_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  { scenario: 'Widget: stop_animation', toolName: 'manage_widget_authoring', arguments: { action: 'stop_animation', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  { scenario: 'Widget: stop_all_animations', toolName: 'manage_widget_authoring', arguments: { action: 'stop_all_animations', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: list_animations', toolName: 'manage_widget_authoring', arguments: { action: 'list_animations', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: get_animation_info', toolName: 'manage_widget_authoring', arguments: { action: 'get_animation_info', widgetPath: `${TEST_FOLDER}/WBP_Test`, animationName: 'FadeIn' }, expected: 'success|not found' },
  
  // === Widget Hierarchy ===
  { scenario: 'Widget: move_widget', toolName: 'manage_widget_authoring', arguments: { action: 'move_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', newParent: 'MenuLayout' }, expected: 'success|not found' },
  { scenario: 'Widget: reorder_widget', toolName: 'manage_widget_authoring', arguments: { action: 'reorder_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', newIndex: 0 }, expected: 'success|not found' },
  { scenario: 'Widget: duplicate_widget', toolName: 'manage_widget_authoring', arguments: { action: 'duplicate_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', newName: 'QuitButton' }, expected: 'success|not found' },
  { scenario: 'Widget: rename_widget', toolName: 'manage_widget_authoring', arguments: { action: 'rename_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', newName: 'StartButton' }, expected: 'success|not found' },
  { scenario: 'Widget: remove_widget', toolName: 'manage_widget_authoring', arguments: { action: 'remove_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'SoundToggle' }, expected: 'success|not found' },
  { scenario: 'Widget: clear_children', toolName: 'manage_widget_authoring', arguments: { action: 'clear_children', widgetPath: `${TEST_FOLDER}/WBP_Test`, parentName: 'ButtonRow' }, expected: 'success|not found' },
  { scenario: 'Widget: get_children', toolName: 'manage_widget_authoring', arguments: { action: 'get_children', widgetPath: `${TEST_FOLDER}/WBP_Test`, parentName: 'RootCanvas' }, expected: 'success|not found' },
  { scenario: 'Widget: get_parent', toolName: 'manage_widget_authoring', arguments: { action: 'get_parent', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText' }, expected: 'success|not found' },
  
  // === Widget Variables ===
  { scenario: 'Widget: expose_widget_as_variable', toolName: 'manage_widget_authoring', arguments: { action: 'expose_widget_as_variable', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar', variableName: 'HealthBarWidget' }, expected: 'success|not found' },
  { scenario: 'Widget: unexpose_widget', toolName: 'manage_widget_authoring', arguments: { action: 'unexpose_widget', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'HealthBar' }, expected: 'success|not found' },
  { scenario: 'Widget: add_widget_variable', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget_variable', widgetPath: `${TEST_FOLDER}/WBP_Test`, variableName: 'CurrentScore', variableType: 'Integer', defaultValue: 0 }, expected: 'success|not found' },
  { scenario: 'Widget: remove_widget_variable', toolName: 'manage_widget_authoring', arguments: { action: 'remove_widget_variable', widgetPath: `${TEST_FOLDER}/WBP_Test`, variableName: 'CurrentScore' }, expected: 'success|not found' },
  { scenario: 'Widget: list_widget_variables', toolName: 'manage_widget_authoring', arguments: { action: 'list_widget_variables', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  
  // === Widget Functions ===
  { scenario: 'Widget: add_widget_function', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget_function', widgetPath: `${TEST_FOLDER}/WBP_Test`, functionName: 'UpdateScore', inputs: [{ name: 'NewScore', type: 'Integer' }], outputs: [] }, expected: 'success|not found' },
  { scenario: 'Widget: remove_widget_function', toolName: 'manage_widget_authoring', arguments: { action: 'remove_widget_function', widgetPath: `${TEST_FOLDER}/WBP_Test`, functionName: 'UpdateScore' }, expected: 'success|not found' },
  { scenario: 'Widget: list_widget_functions', toolName: 'manage_widget_authoring', arguments: { action: 'list_widget_functions', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  
  // === Widget Events ===
  { scenario: 'Widget: add_widget_event', toolName: 'manage_widget_authoring', arguments: { action: 'add_widget_event', widgetPath: `${TEST_FOLDER}/WBP_Test`, eventName: 'OnScoreChanged' }, expected: 'success|not found' },
  { scenario: 'Widget: remove_widget_event', toolName: 'manage_widget_authoring', arguments: { action: 'remove_widget_event', widgetPath: `${TEST_FOLDER}/WBP_Test`, eventName: 'OnScoreChanged' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_native_event_construct', toolName: 'manage_widget_authoring', arguments: { action: 'bind_native_event', widgetPath: `${TEST_FOLDER}/WBP_Test`, eventName: 'Construct' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_native_event_destruct', toolName: 'manage_widget_authoring', arguments: { action: 'bind_native_event', widgetPath: `${TEST_FOLDER}/WBP_Test`, eventName: 'Destruct' }, expected: 'success|not found' },
  { scenario: 'Widget: bind_native_event_tick', toolName: 'manage_widget_authoring', arguments: { action: 'bind_native_event', widgetPath: `${TEST_FOLDER}/WBP_Test`, eventName: 'Tick' }, expected: 'success|not found' },
  
  // === Widget Styling ===
  { scenario: 'Widget: create_widget_style', toolName: 'manage_widget_authoring', arguments: { action: 'create_widget_style', name: 'WS_Button', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Widget: apply_widget_style', toolName: 'manage_widget_authoring', arguments: { action: 'apply_widget_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'PlayButton', stylePath: `${TEST_FOLDER}/WS_Button` }, expected: 'success|not found' },
  { scenario: 'Widget: set_widget_font_style', toolName: 'manage_widget_authoring', arguments: { action: 'set_widget_font_style', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'TitleText', fontFamily: 'Roboto', typeface: 'Bold' }, expected: 'success|not found' },
  
  // === Widget Brush ===
  { scenario: 'Widget: set_brush_from_texture', toolName: 'manage_widget_authoring', arguments: { action: 'set_brush_from_texture', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', texturePath: '/Engine/EngineResources/DefaultTexture' }, expected: 'success|not found' },
  { scenario: 'Widget: set_brush_from_material', toolName: 'manage_widget_authoring', arguments: { action: 'set_brush_from_material', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', materialPath: '/Engine/EngineMaterials/DefaultMaterial' }, expected: 'success|not found' },
  { scenario: 'Widget: set_brush_tiling', toolName: 'manage_widget_authoring', arguments: { action: 'set_brush_tiling', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', tiling: 'Horizontal' }, expected: 'success|not found' },
  { scenario: 'Widget: set_brush_draw_as', toolName: 'manage_widget_authoring', arguments: { action: 'set_brush_draw_as', widgetPath: `${TEST_FOLDER}/WBP_Test`, widgetName: 'BackgroundImage', drawAs: 'Image' }, expected: 'success|not found' },
  
  // === Widget Compilation ===
  { scenario: 'Widget: compile_widget', toolName: 'manage_widget_authoring', arguments: { action: 'compile_widget', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: validate_widget', toolName: 'manage_widget_authoring', arguments: { action: 'validate_widget', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
  { scenario: 'Widget: get_widget_errors', toolName: 'manage_widget_authoring', arguments: { action: 'get_widget_errors', widgetPath: `${TEST_FOLDER}/WBP_Test` }, expected: 'success|not found' },
];

// ============================================================================
// EXPORTS
// ============================================================================
export const authoringToolsTests = [
  ...manageMaterialAuthoringTests,
  ...manageGeometryTests,
  ...manageSkeletonTests,
  ...manageAudioTests,
  ...manageSequenceTests,
  ...manageWidgetAuthoringTests,
];

// Main execution
const main = async () => {
  console.log('='.repeat(80));
  console.log('AUTHORING TOOLS INTEGRATION TESTS');
  console.log('Tools: manage_material_authoring, manage_geometry, manage_skeleton,');
  console.log('       manage_audio, manage_sequence, manage_widget_authoring');
  console.log(`Total Test Cases: ${authoringToolsTests.length}`);
  console.log('='.repeat(80));
  
  try {
    await runToolTests('authoring-tools', authoringToolsTests);
  } catch (error) {
    console.error('Test execution failed:', error);
    process.exit(1);
  }
};

// Run if executed directly
if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  main();
}
