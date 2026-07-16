/**
 * Graph (C20 / O38) route dispositions.
 * Cites the concrete BlueprintGraphHandlersQueries.cpp file (v2 source-derived).
 */
import { type RawRouteDisposition, ROUTE_EVIDENCE_PATHS } from './routedispositions-paths.js';

const { GRAPH_HANDLERS } = ROUTE_EVIDENCE_PATHS;

export const GRAPH_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  {
    key: 'route:graph:get_nodes',
    route: 'get_nodes',
    domain: 'graph',
    status: 'dead',
    owner: 'BlueprintGraph',
    evidenceSource: GRAPH_HANDLERS,
    evidenceSymbol: 'SubAction == TEXT("get_nodes")',
    evidenceTool: 'manage_blueprint',
    disposition: 'remove',
    removalGuidance:
      'C20/O38: orphaned graph op; implemented but absent from TS blueprintGraphActionSet, native core, and every manifest tool. Remove or promote to blueprint graph.',
    rationale:
      'C20/O38: exactly one orphaned graph operation; truly dead-from-MCP.',
  },
];
