/**
 * Aggregated reviewed route-disposition ledger.
 *
 * This module concatenates the per-domain `routedispositions-*.data.ts` shards
 * (each under the 250 pure-line ceiling) into the single `RAW_ROUTE_DISPOSITIONS`
 * array consumed by `routedispositions.ts`. The order is fixed (widget, graph,
 * animation, skeleton, ai, gas, effect, audio, asset, geometry) so the
 * generated artifact stays byte-stable across refactors.
 *
 * Every row carries concrete source evidence and a promote/map/remove
 * disposition; there are zero unresolved rows. `REVIEWED_ROUTE_KEYS` (in
 * `routedispositions.ts`) is derived from this array, and the builder's
 * completeness check fails if any key is omitted or any extra row is added.
 *
 * Categories covered (per plan acceptance + research residuals): widget/graph,
 * skeleton, animation, AI, GAS, effect, audio, asset, geometry, no-op,
 * manual/unreachable.
 */

import { AI_GAS_ROUTE_DISPOSITIONS } from './routedispositions-ai.data.js';
import { ANIMATION_SKELETON_ROUTE_DISPOSITIONS } from './routedispositions-animation.data.js';
import { AUDIO_ASSET_ROUTE_DISPOSITIONS } from './routedispositions-audio.data.js';
import { EFFECT_ROUTE_DISPOSITIONS } from './routedispositions-effect.data.js';
import { GEOMETRY_ROUTE_DISPOSITIONS } from './routedispositions-geometry.data.js';
import { GRAPH_ROUTE_DISPOSITIONS } from './routedispositions-graph.data.js';
import type { RawRouteDisposition } from './routedispositions-paths.js';
import { WIDGET_ROUTE_DISPOSITIONS } from './routedispositions-widget.data.js';

export type { RawRouteDisposition } from './routedispositions-paths.js';

export const RAW_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  ...WIDGET_ROUTE_DISPOSITIONS,
  ...GRAPH_ROUTE_DISPOSITIONS,
  ...ANIMATION_SKELETON_ROUTE_DISPOSITIONS,
  ...AI_GAS_ROUTE_DISPOSITIONS,
  ...EFFECT_ROUTE_DISPOSITIONS,
  ...AUDIO_ASSET_ROUTE_DISPOSITIONS,
  ...GEOMETRY_ROUTE_DISPOSITIONS,
];
