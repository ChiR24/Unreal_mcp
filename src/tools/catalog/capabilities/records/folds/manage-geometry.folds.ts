// Fold specs for manage_geometry. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_GEOMETRY_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_primitive', selector: 'primitive',
    summary: 'Create a primitive mesh actor: box, sphere, cylinder, cone, capsule, plane, disc, ring, torus, pipe, arch, ramp, stairs, spiral stairs.',
    topics: ['create box', 'create sphere', 'create cylinder', 'create plane', 'box mesh', 'primitive mesh', 'stairs', 'torus'],
    members: byTarget('create_', ['create_box', 'create_sphere', 'create_cylinder', 'create_cone', 'create_capsule', 'create_plane', 'create_disc',
      'create_ring', 'create_torus', 'create_pipe', 'create_arch', 'create_ramp', 'create_stairs', 'create_spiral_stairs']),
  },
  {
    primary: 'boolean_mesh', selector: 'booleanOp',
    summary: 'Boolean mesh operations: union, subtract, intersection, trim, self-union.',
    topics: ['boolean', 'union', 'subtract', 'intersection', 'trim mesh', 'csg'],
    members: { union: 'boolean_union', subtract: 'boolean_subtract', intersection: 'boolean_intersection', trim: 'boolean_trim', self_union: 'self_union' },
  },
  {
    primary: 'model_mesh', selector: 'modeling',
    summary: 'Polygon modeling on a mesh: extrude, inset, outset, offset faces, bevel, chamfer, bridge, loft, sweep, revolve, shell, loop cut, edge split, quadrangulate, or extrude/duplicate along a spline.',
    topics: ['extrude', 'inset', 'bevel', 'chamfer', 'bridge', 'loft', 'sweep', 'revolve'],
    members: byName(['extrude', 'inset', 'outset', 'offset_faces', 'bevel', 'chamfer', 'bridge', 'loft', 'sweep', 'revolve', 'shell', 'loop_cut',
      'edge_split', 'quadrangulate', 'extrude_along_spline', 'duplicate_along_spline']),
  },
  {
    primary: 'deform_mesh', selector: 'deform',
    summary: 'Deform a mesh: bend, twist, taper, stretch, spherify, cylindrify, smooth, relax, noise, lattice, displace by texture, poke, triangulate.',
    topics: ['bend', 'twist', 'taper', 'smooth mesh', 'noise deform', 'lattice', 'displace', 'spherify'],
    members: byName(['bend', 'twist', 'taper', 'stretch', 'spherify', 'cylindrify', 'smooth', 'relax', 'noise_deform', 'lattice_deform', 'displace_by_texture', 'poke', 'triangulate']),
  },
  {
    primary: 'optimize_mesh', selector: 'optimization',
    summary: 'Optimize or repair a mesh: simplify, remesh (uniform or voxel), subdivide, merge or weld vertices, remove degenerates, fill holes, flip or recalculate normals, recompute tangents.',
    topics: ['simplify mesh', 'remesh', 'subdivide', 'weld vertices', 'fill holes', 'recalculate normals', 'flip normals', 'decimate'],
    members: byName(['simplify_mesh', 'remesh_uniform', 'remesh_voxel', 'subdivide', 'merge_vertices', 'weld_vertices', 'remove_degenerates', 'fill_holes',
      'flip_normals', 'recalculate_normals', 'recompute_tangents']),
  },
  {
    primary: 'edit_uvs', selector: 'uvOp',
    summary: 'Edit mesh UVs: auto-generate, unwrap, project, pack islands, transform.',
    topics: ['uv', 'unwrap', 'auto uv', 'pack uv islands', 'project uv'],
    members: { auto: 'auto_uv', unwrap: 'unwrap_uv', project: 'project_uv', pack_islands: 'pack_uv_islands', transform: 'transform_uvs' },
  },
  {
    primary: 'configure_mesh_collision', selector: 'collisionOp',
    summary: 'Generate simple or complex collision for a mesh, or simplify its collision.',
    topics: ['generate collision', 'complex collision', 'simplify collision', 'convex hull'],
    members: { generate: 'generate_collision', generate_complex: 'generate_complex_collision', simplify: 'simplify_collision' },
  },
  {
    primary: 'configure_mesh_lods', selector: 'lodOp',
    summary: 'Generate mesh LODs, or set LOD settings and screen sizes.',
    topics: ['generate lods', 'lod settings', 'lod screen size'],
    members: { generate: 'generate_lods', set_settings: 'set_lod_settings', set_screen_sizes: 'set_lod_screen_sizes' },
  },
  {
    primary: 'array_mesh', selector: 'arrayMode',
    summary: 'Duplicate a mesh in a linear or radial array.',
    topics: ['linear array', 'radial array', 'array duplicate'],
    members: { linear: 'array_linear', radial: 'array_radial' },
  },
  {
    primary: 'edit_dynamic_mesh', selector: 'edit',
    summary: 'Edit a procedural/dynamic mesh: create one, append vertices or triangles, set vertex positions, colors or UVs, split normals, translate, or subtract another mesh.',
    topics: ['procedural mesh', 'dynamic mesh', 'append vertex', 'append triangle', 'vertex color', 'vertex position'],
    members: { create: 'create_procedural_mesh', append_vertex: 'append_vertex', append_triangle: 'append_triangle', set_vertex_position: 'set_vertex_position',
      set_vertex_color: 'set_vertex_color', set_uvs: 'set_uvs', split_normals: 'split_normals', translate: 'translate_mesh', difference: 'difference' },
  },
];
