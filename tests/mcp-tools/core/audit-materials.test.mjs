#!/usr/bin/env node
// inspect audit_materials: the materials in view checked as a technical artist would. A row of cubes on a floor, each
// with a material of known values (a black base colour, a half metal, a mid grey, an unlit one), five cubes in the
// engine's default material and one a tenth their size (ten times the texel density), and ten spheres to instance
// behind the camera (instancing reads the whole level).
// (tests/mcp-tools/core/audit-materials.test.mjs)

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TAG = `MCPAuditTag_${ts}`;
const FOLDER = `/Game/MCPTest/Audit_${ts}`;
const PARENT = `${FOLDER}/M_AuditCal`;
const BLACK = `MCP_AuditBlack_${ts}`;
const HALF_METAL = `MCP_AuditHalfMetal_${ts}`;
const GREY = `MCP_AuditGrey_${ts}`;
const UNLIT = `MCP_AuditUnlit_${ts}`;
const SMALL = `MCP_AuditSmall_${ts}`;
// The engine's default material carries texture streaming data, so its texel density is judged, not approximate.
const GRID = '/Engine/EngineMaterials/DefaultMaterial';
const CAMERA = { location: { x: 79200, y: 80300, z: 220 }, rotation: { pitch: -8, yaw: 0, roll: 0 } };

const testCases = [
  { scenario: 'Setup: a material whose values are parameters', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_AuditCal', path: FOLDER }, expected: 'success' },
  { scenario: 'Setup: its graph', toolName: 'manage_asset', arguments: { action: 'add_material_node', materialPath: PARENT, nodeKind: 'batch', operations: [
    { edit: 'add_vector_parameter', id: 'base', parameterName: 'Base', defaultValue: [0.5, 0.5, 0.5] },
    { edit: 'add_scalar_parameter', id: 'metal', parameterName: 'Metal', defaultValue: 0 },
    { edit: 'add_scalar_parameter', id: 'rough', parameterName: 'Rough', defaultValue: 0.5 },
    { edit: 'connect_nodes', from: '$base.RGB', to: 'Main.BaseColor' },
    { edit: 'connect_nodes', from: '$metal', to: 'Main.Metallic' },
    { edit: 'connect_nodes', from: '$rough', to: 'Main.Roughness' },
  ] }, expected: 'success' },
  { scenario: 'Setup: a black and a half-metal instance', toolName: 'manage_asset', arguments: { action: 'create_material_instance', parentMaterial: PARENT, savePath: FOLDER, instances: [
    { name: 'MI_AuditBlack', parameters: [{ parameterName: 'Base', parameterType: 'vector', value: [0, 0, 0, 1] }] },
    { name: 'MI_AuditHalfMetal', parameters: [{ parameterName: 'Metal', parameterType: 'scalar', value: 0.5 }] },
  ] }, expected: 'success' },
  { scenario: 'Setup: an unlit material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_AuditUnlit', path: FOLDER, shadingModel: 'Unlit' }, expected: 'success' },
  { scenario: 'Setup: the row of cubes on a floor', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', tags: [TAG] }, actors: [
    { actorName: `MCP_AuditFloor_${ts}`, location: [80600, 80600, 0], scale: [24, 24, 0.2] },
    { actorName: BLACK, location: [80000, 80000, 60], materialPath: `${FOLDER}/MI_AuditBlack` },
    { actorName: HALF_METAL, location: [80000, 80200, 60], materialPath: `${FOLDER}/MI_AuditHalfMetal` },
    { actorName: GREY, location: [80000, 80400, 60], materialPath: PARENT },
    { actorName: UNLIT, location: [80000, 80600, 60], materialPath: `${FOLDER}/M_AuditUnlit` },
    { actorName: `MCP_AuditGrid_${ts}`, location: [80400, 80000, 60], materialPath: GRID, repeat: { count: 5, offset: [0, 150, 0] } },
    { actorName: SMALL, location: [80400, 80800, 15], scale: [0.1, 0.1, 0.1], materialPath: GRID },
    { actorName: `MCP_AuditBall_${ts}`, classPath: '/Engine/BasicShapes/Sphere', location: [78000, 80000, 60], repeat: { count: 10, offset: [0, 150, 0] } },
  ] }, expected: 'success' },
  { scenario: 'READ: physically based values as the G-buffer holds them', toolName: 'inspect', arguments: { action: 'audit_materials', checks: ['pbr'], ...CAMERA }, expected: 'success', assertions: [
    { path: 'structuredContent.result.pbr.actors', includesObject: { actorName: BLACK, findings: { 0: 'too_dark' } }, label: 'the black cube is darker than charcoal' },
    { path: 'structuredContent.result.pbr.actors', includesObject: { actorName: HALF_METAL, findings: { 0: 'partial_metallic' } }, label: 'the half metal is named' },
    { path: 'structuredContent.result.pbr.actors', includesObject: { actorName: GREY, findings: { length: 0 } }, label: 'the mid grey dielectric is fine' },
    { path: 'structuredContent.result.pbr.skipped', includesObject: { actorName: UNLIT }, label: 'the unlit cube is skipped, not read as black' },
  ] },
  { scenario: 'READ: texel density and instancing', toolName: 'inspect', arguments: { action: 'audit_materials', checks: ['texel_density', 'instancing'], ...CAMERA }, expected: 'success', assertions: [
    { path: 'structuredContent.result.texelDensity.rows', includesObject: { actorName: SMALL, finding: 'too_sharp' }, label: 'the small cube draws its texture ten times denser' },
    { path: 'structuredContent.result.instancing.groups', includesObject: { mesh: '/Engine/BasicShapes/Sphere.Sphere' }, label: 'the ten spheres could be one instanced mesh' },
  ] },
  { scenario: 'ERROR: an unknown check', toolName: 'inspect', arguments: { action: 'audit_materials', checks: ['beauty'] }, expected: 'error' },
  { scenario: 'DELETE: the scene by its tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'Cleanup: delete the materials', toolName: 'manage_asset', arguments: { action: 'delete', paths: [`${FOLDER}/MI_AuditBlack`, `${FOLDER}/MI_AuditHalfMetal`, PARENT, `${FOLDER}/M_AuditUnlit`], force: true }, expected: 'success|not found' },
];

runToolTests('audit-materials', testCases, {});
