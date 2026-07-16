/**
 * Geometry (C25 / C26) route dispositions.
 * Every row cites the concrete GeometryHandlers.cpp file (v2 source-derived).
 */
import { type RawRouteDisposition, ROUTE_EVIDENCE_PATHS } from './routedispositions-paths.js';

const { GEOMETRY_HANDLERS } = ROUTE_EVIDENCE_PATHS;

export const GEOMETRY_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  {
    key: 'route:geometry:difference',
    route: 'difference',
    domain: 'geometry',
    status: 'raw',
    owner: 'Geometry',
    evidenceSource: GEOMETRY_HANDLERS,
    evidenceSymbol: 'SubAction == TEXT("difference") -> HandleBooleanSubtract',
    evidenceTool: 'manage_geometry',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_geometry:difference',
    rationale:
      'C25/O29: twelve raw-only Geometry actions; difference is an exact boolean_subtract alias. Map to canonical id.',
  },
  {
    key: 'route:geometry:bridge',
    route: 'bridge',
    domain: 'geometry',
    status: 'hidden',
    owner: 'Geometry',
    evidenceSource: GEOMETRY_HANDLERS,
    evidenceSymbol: 'SubAction == TEXT("bridge") -> HandleBridge',
    evidenceTool: 'manage_geometry',
    disposition: 'promote',
    targetCanonicalId: 'cap:manage_geometry:bridge',
    rationale: 'C26/O29: pre-5.5 bridge fills holes and ignores subdivisions; version-dependent partial; promote with version gate.',
  },
  {
    key: 'route:geometry:loft',
    route: 'loft',
    domain: 'geometry',
    status: 'hidden',
    owner: 'Geometry',
    evidenceSource: GEOMETRY_HANDLERS,
    evidenceSymbol: 'SubAction == TEXT("loft") -> HandleLoft',
    evidenceTool: 'manage_geometry',
    disposition: 'promote',
    targetCanonicalId: 'cap:manage_geometry:loft',
    rationale: 'C26/O29: no-profile loft appends no geometry; version-dependent partial; promote with profile requirement.',
  },
  {
    key: 'route:geometry:dynamicmesh_raw_residual',
    route: 'dynamicmesh_raw_residual',
    domain: 'geometry',
    status: 'raw',
    owner: 'Geometry',
    evidenceSource: GEOMETRY_HANDLERS,
    evidenceSymbol:
      'create_procedural_mesh; append_triangle; set_uvs; set_vertex_color; split_normals; append_vertex; delete_vertex; delete_triangle; get_vertex_position; set_vertex_position; translate_mesh',
    evidenceTool: 'manage_geometry',
    disposition: 'promote',
    targetCanonicalId: 'cap:manage_geometry:dynamicmesh_raw_residual',
    rationale:
      'C25/O29: eleven concrete DynamicMesh behaviors are raw-only (absent from every canonical parent); promote to manage_geometry.',
  },
];
