/**
 * Geometry primitive-creation family records (15 actions).
 */
import type { CapabilityRecordSource, JsonObject } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'primitives';
const PLUGIN = ['GeometryScripting'] as const;

// Names the spawned actor. NOTE: primitive create_* actions spawn a LEVEL
// actor (DynamicMeshActor) and do NOT consume a /Game asset path. `path` was
// declared here but silently ignored, which read as an asset-creation
// contract. Removed — pass `name` only, then use convert_to_static_mesh to
// persist an asset.
const IDENT = { name: P.name };
// Read by the shared ReadTransformFromPayload helper on every create_* action.
const XFORM = { location: P.location, rotation: P.rotation, scale: P.scale };

const cm = (d: string): JsonObject => ({ type: 'number', description: d });
const vec = (d: string): JsonObject => ({ type: 'object', description: d, properties: { x: cm('X'), y: cm('Y'), z: cm('Z') }, additionalProperties: false });
// create_primitive keeps the first variant's text for a parameter every variant shares, so each shared text speaks for
// all of them: the disc's "centred hole" was read as the arch's innerRadius, which is its tube.
const RADIUS = cm('Radius in cm (default 50). Torus and arch: from the centre to the middle of the tube (arch default 100). Pipe: the outer wall, as outerRadius. Spiral stairs: the inner radius when innerRadius is not given (default 150).');
const INNER_RADIUS = cm('In cm, per variant. Disc and ring: the centred hole (disc default 0, solid; ring 25). Pipe: the inner wall (default 40). ' +
  'Torus and arch: the TUBE radius, its round cross-section, not a hole (default 20 torus, 25 arch); the opening under an arch is radius - innerRadius. Spiral stairs: the radius of the central well.');
const SWEEP = cm('Sweep in degrees: torus default 360 (less leaves an open segment), arch default 180, a half ring. Both lie flat in the local XY plane, starting on local +X and turning toward +Y; give a rotation to stand an arch up.');
const SDF_SHAPES: JsonObject = {
  type: 'array', minItems: 1, maxItems: 64,
  description: 'Shapes combined in order onto shapes[0] (which must be a union) as one signed distance field, then meshed. ' +
    'Sizes are cm in the mesh\'s local space. Each joined shape takes a blend: the radius of the soft fillet where it meets what came before. ' +
    'An entry with mirror or repeat stands for all its copies (at most 1024 shapes in all), applied in its place in the order.',
  items: {
    type: 'object', required: ['type'], additionalProperties: false,
    properties: {
      type: { type: 'string', enum: ['sphere', 'ellipsoid', 'box', 'capsule', 'cylinder', 'torus', 'cone'], description: 'sphere (radius), ellipsoid (radii), box (extent = half size, rounding), capsule (radius, length of the straight part, along local Z), cylinder (radius, length, rounding, along local Z), torus (radius to the tube centre, thickness = tube radius, in local XY), cone (radius at the bottom, topRadius, length, along local Z, rounded ends). radius also sets an ellipsoid\'s radii and a box\'s extent when those are left out; any other size a type does not read is refused.' },
      operation: { type: 'string', enum: ['union', 'subtract', 'intersect'], description: 'union adds the shape (default), subtract carves it out of what came before (a recess, socket or groove), intersect keeps only the overlap.' },
      center: vec('Shape centre in the mesh\'s local space, cm (default 0,0,0); with relativeTo, in that shape\'s frame.'),
      relativeTo: { type: 'integer', minimum: 0, maximum: 63, description: 'Index of an earlier shapes[] entry to place this one on: center, rotation, repeat and mirror are then measured from that shape\'s centre along its own turned axes (not scaled by its size), so the eyes, mouth or buttons of a leaning or turned part stay on it without working out the rotation. mirror then reflects across that shape\'s own planes.' },
      rotation: { type: 'object', description: 'Shape rotation {pitch, yaw, roll} in degrees; turns the shape\'s local Z axis for capsule, cylinder, cone and torus.', properties: { pitch: cm('Pitch'), yaw: cm('Yaw'), roll: cm('Roll') }, additionalProperties: false },
      radius: cm('Radius in cm (sphere, capsule, cylinder, cone base; torus: distance to the tube centre). Default 50.'),
      radii: vec('Ellipsoid radii along local X, Y, Z in cm.'),
      extent: vec('Box half size along local X, Y, Z in cm.'),
      rounding: cm('Box or cylinder edge rounding radius in cm (0 = sharp).'),
      length: cm('Capsule straight length, cylinder or cone height, in cm (default 100).'),
      topRadius: cm('Cone radius at the top in cm (0 = point; larger than radius widens upward).'),
      thickness: cm('Torus tube radius in cm (default 10).'),
      blend: cm('Smooth-join radius in cm with the shapes before it: 0 is a hard edge; a few cm reads as a soft fillet on a union and as a rounded rim on a subtract.'),
      materialId: { type: 'integer', minimum: 0, maximum: 63, description: 'Material slot of the surface this shape forms (default 0). A subtract shape owns the surface it carves, so a carved visor can take its own slot.' },
      mirror: {
        type: 'array', minItems: 1, maxItems: 3, items: { type: 'string', enum: ['x', 'y', 'z'] },
        description: 'Also add this shape reflected across the mesh\'s own plane through the origin normal to each listed axis ("y" mirrors y to -y): both eyes, both ears or a pair of legs from one entry. Each axis doubles the copies so far, so ["x","y"] gives four; repeat copies are mirrored too.',
      },
      repeat: {
        type: 'object', additionalProperties: false, required: ['count'],
        description: 'Copies of this shape with the same size, blend, operation and materialId: a row with offset, or a ring with axis. A row of stitches, rivets round a rim or curls round a head is one entry. The reply\'s parts[] still lists one entry per shapes[] item, with copies.',
        properties: {
          count: { type: 'integer', minimum: 2, maximum: 256, description: 'How many, the shape itself included.' },
          offset: vec('Row: each copy moves this much further from the previous one, cm.'),
          axis: { type: 'string', enum: ['x', 'y', 'z'], description: 'Ring: turn the copies about this mesh axis through pivot (each copy turns with it).' },
          angle: cm('Ring: total sweep in degrees (default 360, evenly spaced all round; a smaller arc puts the last copy at its end).'),
          pivot: vec('Ring: a point on the turning axis, cm (default 0,0,0).'),
        },
      },
    },
  },
};

export const GEOMETRY_PRIMITIVES_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_box', plugins: PLUGIN,
    topics: ['box mesh', 'cube mesh', 'procedural box', 'geometry cube'],
    family: F, summary: 'Create a box dynamic mesh actor.', whenToUse: ['A box primitive must be created.'], whenNotToUse: ['A sphere is needed; use create_sphere.'],
    inputProps: { ...IDENT, ...XFORM, dimensions: P.dimensions, width: P.width, height: P.boxHeight, depth: P.depth, widthSegments: P.widthSegments, heightSegments: P.heightSegments, depthSegments: P.depthSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_box', dimensions: { x: 100, y: 100, z: 100 } },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_sphere', plugins: PLUGIN,
    family: F, summary: 'Create a sphere dynamic mesh actor.', whenToUse: ['A sphere primitive must be created.'], whenNotToUse: ['A box is needed; use create_box.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_sphere', radius: 50 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_cylinder', plugins: PLUGIN,
    family: F, summary: 'Create a cylinder dynamic mesh actor.', whenToUse: ['A cylinder primitive must be created.'], whenNotToUse: ['A cone is needed; use create_cone.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, height: P.height, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_cylinder', radius: 50, height: 200 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_cone', plugins: PLUGIN,
    family: F, summary: 'Create a cone dynamic mesh actor.', whenToUse: ['A cone primitive must be created.'], whenNotToUse: ['A cylinder is needed; use create_cylinder.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, baseRadius: P.baseRadius, topRadius: P.topRadius, height: P.height, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_cone', radius: 50, height: 200 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_capsule', plugins: PLUGIN,
    family: F, summary: 'Create a capsule dynamic mesh actor.', whenToUse: ['A capsule primitive must be created.'], whenNotToUse: ['A sphere is needed; use create_sphere.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, length: P.length, height: P.height, radialSegments: P.radialSegments, numRings: P.numRings, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_capsule', radius: 50, height: 200 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_torus', plugins: PLUGIN,
    family: F, summary: 'Create a torus dynamic mesh actor.', whenToUse: ['A torus primitive must be created.'], whenNotToUse: ['A ring is needed; use create_ring.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, innerRadius: INNER_RADIUS, angle: SWEEP, numSides: P.numSides, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_torus', radius: 100, innerRadius: 20 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_plane', plugins: PLUGIN,
    family: F, summary: 'Create a plane dynamic mesh actor: flat in XY, centred on its origin, facing +Z.', whenToUse: ['A flat plane primitive must be created.', 'A sign, banner or flag needs a subdivided sheet: stand the plane up with a 90 degree roll on the component that draws it.'], whenNotToUse: ['A box is needed; use create_box.'],
    inputProps: { ...IDENT, ...XFORM, width: P.width, depth: P.depth, widthSegments: P.widthSegments, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_plane', width: 500, depth: 500 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_disc', plugins: PLUGIN,
    family: F, summary: 'Create a disc dynamic mesh actor.', whenToUse: ['A circular disc primitive must be created.'], whenNotToUse: ['A plane is needed; use create_plane.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, innerRadius: INNER_RADIUS, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_disc', radius: 100 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_stairs', plugins: PLUGIN,
    family: F, summary: 'Create a stairs dynamic mesh actor.', whenToUse: ['A stair primitive must be created.'], whenNotToUse: ['A ramp is needed; use create_ramp.'],
    inputProps: { ...IDENT, ...XFORM, steps: { ...P.steps, description: 'Number of steps (default 8; numSteps wins when both are sent).' }, numSteps: P.numSteps, stepWidth: P.stepWidth, stepHeight: P.stepHeight, stepDepth: P.stepDepth, floating: P.floating }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_stairs', steps: 10, stepWidth: 200, stepHeight: 20, stepDepth: 30 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_spiral_stairs', plugins: PLUGIN,
    family: F, summary: 'Create a spiral stairs dynamic mesh actor.', whenToUse: ['A spiral stair primitive must be created.'], whenNotToUse: ['A straight stair is needed; use create_stairs.'],
    inputProps: { ...IDENT, ...XFORM, steps: { ...P.steps, description: 'Number of steps (default 8; numSteps wins when both are sent).' }, numSteps: P.numSteps, radius: RADIUS, innerRadius: INNER_RADIUS, numTurns: P.numTurns, stepWidth: P.stepWidth, stepHeight: P.stepHeight, floating: P.floating }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_spiral_stairs', steps: 20, radius: 200 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_ring', plugins: PLUGIN,
    family: F, summary: 'Create a ring dynamic mesh actor.', whenToUse: ['A ring/annulus primitive must be created.'], whenNotToUse: ['A torus is needed; use create_torus.'],
    inputProps: { ...IDENT, ...XFORM, innerRadius: INNER_RADIUS, outerRadius: P.outerRadius, numSides: P.numSides }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_ring', innerRadius: 80, outerRadius: 100 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_arch', plugins: PLUGIN,
    family: F, summary: 'Create an arch dynamic mesh actor.', whenToUse: ['An arch primitive must be created.'], whenNotToUse: ['A pipe is needed; use create_pipe.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, innerRadius: INNER_RADIUS, angle: SWEEP, numSides: P.numSides, radialSegments: P.radialSegments, numRings: P.numRings }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_arch', radius: 300, innerRadius: 50, angle: 180 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_pipe', plugins: PLUGIN,
    family: F, summary: 'Create a pipe dynamic mesh actor.', whenToUse: ['A pipe/tube primitive must be created.'], whenNotToUse: ['A cylinder is needed; use create_cylinder.'],
    inputProps: { ...IDENT, ...XFORM, radius: RADIUS, innerRadius: INNER_RADIUS, outerRadius: P.outerRadius, height: P.height, numSides: P.numSides, heightSegments: P.heightSegments }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_pipe', radius: 30, height: 400 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_ramp', plugins: PLUGIN,
    family: F, summary: 'Create a ramp dynamic mesh actor.', whenToUse: ['A ramp primitive must be created.'], whenNotToUse: ['A stair is needed; use create_stairs.'],
    inputProps: { ...IDENT, ...XFORM, width: P.width, length: P.length, height: P.height }, required: [], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_ramp', width: 100, length: 200, height: 50 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_roof', plugins: PLUGIN,
    topics: ['roof', 'hip roof', 'roof over a footprint', 'house roof'],
    family: F,
    summary: 'Create a hip roof mesh actor over a footprint: every face rises from its outline edge at pitch (default 30 degrees) and meets the others along ridges, hips and valleys where the outline\'s straight skeleton puts them, for any simple outline of 3 to 64 corners in either winding: rectangles, L, T and U plans, bays and angled walls (no holes). footprint is in the actor\'s space at the top of the walls (z 0); overhang (cm, default 0) carries the roof planes out past the walls, the eaves dropping by overhang x tan(pitch); base (default true) closes it underneath at the eaves\' height, so the mesh is watertight (with an overhang that underside lies below the wall top: set the roof that much higher, or base false to leave it open). Each face is its own polygroup, its UVs in metres along its eave and up its slope, so a tiling roof material lies in rows along every eave. The reply gives faces (one per outline edge), ridgeHeight above the walls, eaveHeight, triangles and watertight; one undo step. Refused, making nothing: INVALID_ARGUMENT (fewer than 3 corners apart, an outline that crosses itself, an overhang that folds it over itself), ROOF_FAILED (faces that do not make a clean mesh).',
    whenToUse: ['A house, shed or hall needs a roof whose faces meet properly over its plan, however many corners it has.'],
    whenNotToUse: ['A flat roof: a box primitive.', 'A gable roof with upright end walls: this makes hip roofs, every edge sloping.'],
    inputProps: {
      ...IDENT, ...XFORM,
      footprint: {
        type: 'array', minItems: 3, maxItems: 64,
        items: { type: 'object', additionalProperties: false, required: ['x', 'y'], properties: { x: { type: 'number' }, y: { type: 'number' } } },
        description: 'The outline under the roof, its corners in order around it as {x, y} in cm in the actor\'s space (either winding; corners under 0.5 cm apart or in a straight line are dropped).',
      },
      pitch: { type: 'number', minimum: 5, maximum: 75, description: 'Slope of every face in degrees (default 30).' },
      overhang: { type: 'number', minimum: 0, maximum: 500, description: 'How far the eaves reach past the walls, in cm (default 0).' },
      base: { type: 'boolean', description: 'Close the roof underneath (default true), so the mesh is watertight.' },
    },
    outputProps: {
      faces: { type: 'integer', description: 'Roof faces: one per outline edge.' },
      triangles: { type: 'integer', description: 'Triangles in the mesh.' },
      ridgeHeight: cm('Height of the highest ridge or peak above the walls.'),
      eaveHeight: cm('Height of the eaves: 0 without an overhang, below the walls with one.'),
      watertight: { type: 'boolean', description: 'Every edge of the mesh is shared by two triangles.' },
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope}.' },
    },
    required: ['footprint'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'create_roof', name: 'HouseRoof', footprint: [{ x: 0, y: 0 }, { x: 1000, y: 0 }, { x: 1000, y: 600 }, { x: 0, y: 600 }], pitch: 35, overhang: 40 },
  }),
  buildWorldRecord({
    parentTool: 'manage_geometry', action: 'create_sdf', plugins: PLUGIN,
    topics: ['sdf', 'signed distance field', 'smooth union', 'blend shapes', 'organic mesh', 'fillet', 'metaball', 'rounded character part'],
    family: F,
    summary: 'Create one smooth organic mesh actor from blended shapes (a signed distance field): spheres, ellipsoids, rounded boxes, capsules, cylinders, tori and cones joined with soft fillets, carved with rounded recesses, or intersected, each shape with its own material slot.',
    whenToUse: ['A smooth organic or toy-like form must be modelled from several parts that flow into each other: a helmet, a hand, a shoe, a character body, a rounded prop.', 'A recess, socket or groove with a soft rim must be carved into a rounded form (subtract with blend), for example a visor.'],
    whenNotToUse: ['One plain primitive is enough; use that primitive.', 'Hard mechanical parts with exact flat faces; use primitives with bevel and booleans.'],
    inputProps: { ...IDENT, ...XFORM, shapes: SDF_SHAPES, resolution: { type: 'integer', minimum: 16, maximum: 256, description: 'Grid cells along the longest side of the shapes\' bounds (default 128, 16-256): higher is finer and slower; 160-200 suits a hero part.' } },
    outputProps: {
      parts: {
        type: 'array', description: 'One entry per shape, saying which surface it formed.',
        items: {
          type: 'object', additionalProperties: false,
          properties: {
            shape: { type: 'integer', description: 'Index into shapes.' },
            groupId: { type: 'integer', description: 'Polygroup of the surface this shape formed (index+1).' },
            materialId: { type: 'integer', description: 'Material slot of that surface.' },
            triangles: { type: 'integer', description: 'Triangles of that surface; 0 when another shape covered it entirely.' },
            copies: { type: 'integer', description: 'How many shapes this entry became with its repeat and mirror (the shape itself included); absent for a single shape.' },
            warning: { type: 'string', description: 'Present when a union shape is under two grid cells across: its surface can come out ragged, or as almost nothing when it formed under 24 triangles; raise resolution or thicken it.' },
          },
        },
      },
      resolution: { type: 'integer', description: 'Grid cells along the longest side, as used.' },
      cellSize: cm('Grid cell size in cm.'),
      vertexCount: { type: 'integer', description: 'Vertices in the mesh.' },
      triangleCount: { type: 'integer', description: 'Triangles in the mesh.' },
    },
    required: ['shapes'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_sdf', name: 'Mitten', resolution: 160, shapes: [{ type: 'ellipsoid', radii: { x: 9, y: 4, z: 10 } }, { type: 'capsule', center: { x: 7, y: 0, z: -2 }, rotation: { pitch: 60, yaw: 0, roll: 0 }, radius: 3, length: 6, blend: 3 }] },
  }),
];
