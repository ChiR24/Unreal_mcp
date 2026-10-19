// Fold specs for manage_geometry. Data only; see ../shared/fold.ts.
import { byName, byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_GEOMETRY_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_primitive', selector: 'primitive',
    summary: 'Create a primitive mesh actor: box, sphere, cylinder, cone, capsule, plane, disc, ring, torus, pipe, arch, ramp, stairs, spiral stairs, or one smooth organic mesh from blended shapes (sdf) with soft fillets where they meet.',
    topics: ['create box', 'create sphere', 'create cylinder', 'create plane', 'box mesh', 'primitive mesh', 'stairs', 'torus', 'make a cube or ramp',
      'signed distance field', 'smooth blend shapes', 'organic mesh'],
    members: byTarget('create_', ['create_box', 'create_sphere', 'create_cylinder', 'create_cone', 'create_capsule', 'create_plane', 'create_disc',
      'create_ring', 'create_torus', 'create_pipe', 'create_arch', 'create_ramp', 'create_stairs', 'create_spiral_stairs', 'create_sdf']),
  },
  {
    primary: 'boolean_mesh', selector: 'booleanOp',
    summary: 'Boolean mesh operations: union, subtract, intersection, trim, self-union.',
    topics: ['boolean', 'union', 'subtract', 'intersection', 'trim mesh', 'csg', 'cut a hole in a mesh'],
    members: { union: 'boolean_union', subtract: 'boolean_subtract', intersection: 'boolean_intersection', trim: 'boolean_trim', self_union: 'self_union' },
  },
  {
    primary: 'model_mesh', selector: 'modeling',
    summary: 'Polygon modeling on a mesh: extrude, inset, outset, offset faces, bevel, chamfer, bridge, loft, sweep, revolve, shell, loop cut, edge split, poke faces, group triangles into quads, or extrude/duplicate along a spline. Extrude, inset, outset, offset, bevel, chamfer and poke work on every triangle, on triangleIndices, or on a region picked by box, facing direction, polygroup or material id. Dynamic meshes are always triangles.',
    topics: ['extrude', 'inset', 'bevel', 'chamfer', 'bridge', 'loft', 'sweep', 'revolve', 'poke faces', 'triangles to quads', 'select faces by region', 'extrude the top faces'],
    members: byName(['extrude', 'inset', 'outset', 'offset_faces', 'bevel', 'chamfer', 'bridge', 'loft', 'sweep', 'revolve', 'shell', 'loop_cut',
      'edge_split', 'poke', 'quadrangulate', 'extrude_along_spline', 'duplicate_along_spline']),
  },
  {
    primary: 'deform_mesh', selector: 'deform',
    summary: 'Deform a mesh: bend, twist, taper, stretch, spherify, cylindrify, smooth, relax, noise, lattice, displace by texture.',
    topics: ['bend', 'twist', 'taper', 'smooth mesh', 'noise deform', 'lattice', 'displace', 'spherify'],
    members: byName(['bend', 'twist', 'taper', 'stretch', 'spherify', 'cylindrify', 'smooth', 'relax', 'noise_deform', 'lattice_deform', 'displace_by_texture']),
  },
  {
    primary: 'optimize_mesh', selector: 'optimization',
    summary: 'Optimize, smooth or repair a mesh: simplify, remesh (uniform or voxel), subdivide (pn tessellation, or Catmull-Clark, Loop and bilinear surfaces from a polygroup cage), morphology (voxel offset: dilate, contract, close to fillet where unioned parts meet, open), merge or weld vertices, remove degenerates, fill holes, flip or recalculate normals, recompute tangents.',
    topics: ['simplify mesh', 'remesh', 'subdivide', 'weld vertices', 'fill holes', 'recalculate normals', 'flip normals', 'decimate', 'reduce triangles', 'catmull clark', 'smooth subdivision', 'fillet'],
    members: byName(['simplify_mesh', 'remesh_uniform', 'remesh_voxel', 'subdivide', 'morphology', 'merge_vertices', 'weld_vertices', 'remove_degenerates', 'fill_holes',
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
    summary: 'LODs for the meshes the geometry tools make. generate (lodOp generate, the default) bakes the dynamic mesh of actorName or targetActor into a new static mesh asset saved at outputPath (default /Game/GeneratedMeshes/<actor>_LOD) with lodCount LODs in all (default 4, LOD 0 included, each later one half the triangles of the one before) and replies assetPath, lodCount and the LOD 0 triangles; the actor keeps its dynamic mesh. set_settings and set_screen_sizes change the static mesh asset the first static mesh component of the actor draws, so every actor drawing it changes, and save it (saved says whether it was written); an engine mesh is refused with ENGINE_CONTENT (duplicate it into /Game and assign the copy first). set_settings rebuilds LOD lodIndex (default 1; INVALID_LOD_INDEX past the LODs the mesh has, so generate them first) at trianglePercent of the base triangles (default 50, or 100 minus reductionPercent; above 0 and at most 100), recomputing normals or tangents when asked. set_screen_sizes writes screenSizes, LOD 0 first, as the screen size at which each LOD takes over, and turns the automatic screen sizes off; values past the LODs the mesh has are ignored (screenSizesSet counts those written).',
    topics: ['generate lods', 'lod settings', 'lod screen size'],
    members: { generate: 'generate_lods', set_settings: 'set_lod_settings', set_screen_sizes: 'set_lod_screen_sizes' },
  },
  {
    primary: 'array_mesh', selector: 'arrayMode',
    summary: 'Repeat a mesh inside the same mesh: a row (linear, one offset per copy) or a ring round its local Z (radial). count includes the original.',
    topics: ['linear array', 'radial array', 'array duplicate'],
    members: { linear: 'array_linear', radial: 'array_radial' },
  },
  {
    primary: 'edit_dynamic_mesh', selector: 'edit',
    summary: 'Edit a procedural/dynamic mesh: create one, append vertices, triangles, a whole polygon cage (append_polygons), parts laid out in rows and grids (append_cuboids), a loft through cross sections (append_loft) or copies of a detailed static mesh (append_mesh) in one call, delete the triangles of a region (delete_triangles), give triangles material ids by region (set_material_id), bake AO, edge, cavity and height masks for materials to read (bake_vertex_colors), set vertex positions, colors or UVs, split normals, translate, or subtract another mesh.',
    topics: ['procedural mesh', 'dynamic mesh', 'append vertex', 'append triangle', 'vertex color', 'vertex position', 'move a vertex', 'create a mesh from vertices', 'polygon cage', 'material ids', 'bake masks', 'loft', 'kitbash'],
    members: { create: 'create_procedural_mesh', append_vertex: 'append_vertex', append_triangle: 'append_triangle', append_polygons: 'append_polygons', append_cuboids: 'append_cuboids', append_loft: 'append_loft', append_mesh: 'append_mesh',
      delete_triangles: 'delete_triangles', set_vertex_position: 'set_vertex_position', set_vertex_color: 'set_vertex_color', bake_vertex_colors: 'bake_vertex_colors', set_material_id: 'set_material_id', set_uvs: 'set_uvs', split_normals: 'split_normals',
      translate: 'translate_mesh', difference: 'difference' },
  },
];
