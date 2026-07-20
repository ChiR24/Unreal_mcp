/**
 * Geometry primitive-creation family records (14 actions).
 *
 * Grounded in manage-geometry-tool.ts create_* primitives and native Geometry
 * domain dispatch. Every geometry action requires the GeometryScripting
 * optional plugin (auto-enabled by the bridge when present). Dispatches through
 * the manage_geometry bridge tool (dispatchMode 'tool').
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'primitives';
const NR = 'Distinct manage_geometry primitive verb and target; no cross-tool duplicate.';
const PLUGIN = ['GeometryScripting'] as const;

// Names the spawned actor; `path` is accepted by the geometry path normalizer.
const IDENT = { name: P.name, path: P.path };
// Read by the shared ReadTransformFromPayload helper on every create_* action.
const XFORM = { location: P.location, rotation: P.rotation, scale: P.scale };

export const GEOMETRY_PRIMITIVES_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_box', plugins: PLUGIN,
    family: F, summary: 'Create a box dynamic mesh actor.', whenToUse: ['A box primitive must be created.'], whenNotToUse: ['A sphere is needed; use create_sphere.'],
    inputProps: { ...IDENT, ...XFORM, dimensions: P.dimensions, width: P.width, height: P.height, depth: P.depth, widthSegments: P.widthSegments, heightSegments: P.heightSegments, depthSegments: P.depthSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_box', dimensions: { x: 100, y: 100, z: 100 } }, exampleOutput: { success: true, message: 'Box created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_sphere', plugins: PLUGIN,
    family: F, summary: 'Create a sphere dynamic mesh actor.', whenToUse: ['A sphere primitive must be created.'], whenNotToUse: ['A box is needed; use create_box.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_sphere', radius: 50 }, exampleOutput: { success: true, message: 'Sphere created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_cylinder', plugins: PLUGIN,
    family: F, summary: 'Create a cylinder dynamic mesh actor.', whenToUse: ['A cylinder primitive must be created.'], whenNotToUse: ['A cone is needed; use create_cone.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, height: P.height, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_cylinder', radius: 50, height: 200 }, exampleOutput: { success: true, message: 'Cylinder created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_cone', plugins: PLUGIN,
    family: F, summary: 'Create a cone dynamic mesh actor.', whenToUse: ['A cone primitive must be created.'], whenNotToUse: ['A cylinder is needed; use create_cylinder.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, height: P.height, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_cone', radius: 50, height: 200 }, exampleOutput: { success: true, message: 'Cone created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_capsule', plugins: PLUGIN,
    family: F, summary: 'Create a capsule dynamic mesh actor.', whenToUse: ['A capsule primitive must be created.'], whenNotToUse: ['A sphere is needed; use create_sphere.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, height: P.height, radialSegments: P.radialSegments, numRings: P.numRings, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_capsule', radius: 50, height: 200 }, exampleOutput: { success: true, message: 'Capsule created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_torus', plugins: PLUGIN,
    family: F, summary: 'Create a torus dynamic mesh actor.', whenToUse: ['A torus primitive must be created.'], whenNotToUse: ['A ring is needed; use create_ring.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, innerRadius: P.innerRadius, numSides: P.numSides, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_torus', radius: 100, innerRadius: 20 }, exampleOutput: { success: true, message: 'Torus created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_plane', plugins: PLUGIN,
    family: F, summary: 'Create a plane dynamic mesh actor.', whenToUse: ['A flat plane primitive must be created.'], whenNotToUse: ['A box is needed; use create_box.'],
    inputProps: { ...IDENT, ...XFORM, dimensions: P.dimensions, width: P.width, depth: P.depth, widthSegments: P.widthSegments, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_plane', dimensions: { x: 500, y: 500, z: 1 } }, exampleOutput: { success: true, message: 'Plane created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_disc', plugins: PLUGIN,
    family: F, summary: 'Create a disc dynamic mesh actor.', whenToUse: ['A circular disc primitive must be created.'], whenNotToUse: ['A plane is needed; use create_plane.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_disc', radius: 100 }, exampleOutput: { success: true, message: 'Disc created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_stairs', plugins: PLUGIN,
    family: F, summary: 'Create a stairs dynamic mesh actor.', whenToUse: ['A stair primitive must be created.'], whenNotToUse: ['A ramp is needed; use create_ramp.'],
    inputProps: { ...IDENT, ...XFORM, steps: P.steps, numSteps: P.numSteps, dimensions: P.dimensions, stepWidth: P.stepWidth, stepHeight: P.stepHeight, stepDepth: P.stepDepth }, required: ['steps'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_stairs', steps: 10, dimensions: { x: 200, y: 100, z: 300 } }, exampleOutput: { success: true, message: 'Stairs created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_spiral_stairs', plugins: PLUGIN,
    family: F, summary: 'Create a spiral stairs dynamic mesh actor.', whenToUse: ['A spiral stair primitive must be created.'], whenNotToUse: ['A straight stair is needed; use create_stairs.'],
    inputProps: { ...IDENT, ...XFORM, steps: P.steps, numSteps: P.numSteps, radius: P.radius, innerRadius: P.innerRadius, numTurns: P.numTurns, stepWidth: P.stepWidth, stepHeight: P.stepHeight }, required: ['steps'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_spiral_stairs', steps: 20, radius: 200 }, exampleOutput: { success: true, message: 'Spiral stairs created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_ring', plugins: PLUGIN,
    family: F, summary: 'Create a ring dynamic mesh actor.', whenToUse: ['A ring/annulus primitive must be created.'], whenNotToUse: ['A torus is needed; use create_torus.'],
    inputProps: { ...IDENT, ...XFORM, innerRadius: P.innerRadius, outerRadius: P.outerRadius, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_ring', innerRadius: 80, outerRadius: 100 }, exampleOutput: { success: true, message: 'Ring created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_arch', plugins: PLUGIN,
    family: F, summary: 'Create an arch dynamic mesh actor.', whenToUse: ['An arch primitive must be created.'], whenNotToUse: ['A pipe is needed; use create_pipe.'],
    inputProps: { ...IDENT, ...XFORM, dimensions: P.dimensions, radius: P.radius, innerRadius: P.innerRadius, numSides: P.numSides, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_arch', dimensions: { x: 300, y: 50, z: 400 } }, exampleOutput: { success: true, message: 'Arch created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_pipe', plugins: PLUGIN,
    family: F, summary: 'Create a pipe dynamic mesh actor.', whenToUse: ['A pipe/tube primitive must be created.'], whenNotToUse: ['A cylinder is needed; use create_cylinder.'],
    inputProps: { ...IDENT, ...XFORM, radius: P.radius, innerRadius: P.innerRadius, outerRadius: P.outerRadius, height: P.height, numSides: P.numSides, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_pipe', radius: 30, height: 400 }, exampleOutput: { success: true, message: 'Pipe created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_ramp', plugins: PLUGIN,
    family: F, summary: 'Create a ramp dynamic mesh actor.', whenToUse: ['A ramp primitive must be created.'], whenNotToUse: ['A stair is needed; use create_stairs.'],
    inputProps: { ...IDENT, ...XFORM, dimensions: P.dimensions, width: P.width }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_ramp', dimensions: { x: 200, y: 100, z: 300 } }, exampleOutput: { success: true, message: 'Ramp created' },
    normalizationRationale: NR,
  }),
];
