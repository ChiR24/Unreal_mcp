/**
 * Audio (O40) and asset (C17 / C24) route dispositions.
 *
 * Audio: 4 shadowed cases only; the 3 invented MRQ audio routes
 * (cancel_render/get_render_progress/get_render_status) are EXCLUDED
 * per v2 - no source token exists for them. Asset: 3 rows with concrete
 * file evidence (v2 source-derived).
 */
import {
  MATERIAL_OVERLAP_CITATIONS,
  type RawRouteDisposition,
  ROUTE_EVIDENCE_PATHS,
} from './routedispositions-paths.js';

const { AUDIO_ASSETS, AUDIO_HANDLERS, ASSET_WORKFLOW_HANDLERS, ASSET_QUERY_HANDLERS, ASSET_MATERIAL_PINS } =
  ROUTE_EVIDENCE_PATHS;

export const AUDIO_ASSET_ROUTE_DISPOSITIONS: readonly RawRouteDisposition[] = [
  {
    key: 'route:audio:create_sound_cue',
    route: 'create_sound_cue',
    domain: 'audio',
    status: 'hidden',
    owner: 'Audio',
    evidenceSource: AUDIO_ASSETS,
    evidenceSymbol: 'Lower == TEXT("create_sound_cue")',
    evidenceTool: 'manage_audio',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_audio:create_sound_cue',
    rationale: 'O40: one of four shadowed audio cases; canonical via manage_audio; map.',
  },
  {
    key: 'route:audio:create_sound_class',
    route: 'create_sound_class',
    domain: 'audio',
    status: 'hidden',
    owner: 'Audio',
    evidenceSource: AUDIO_ASSETS,
    evidenceSymbol: 'Lower == TEXT("create_sound_class")',
    evidenceTool: 'manage_audio',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_audio:create_sound_class',
    rationale: 'O40: one of four shadowed audio cases; canonical via manage_audio; map.',
  },
  {
    key: 'route:audio:create_sound_mix',
    route: 'create_sound_mix',
    domain: 'audio',
    status: 'hidden',
    owner: 'Audio',
    evidenceSource: AUDIO_ASSETS,
    evidenceSymbol: 'Lower == TEXT("create_sound_mix")',
    evidenceTool: 'manage_audio',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_audio:create_sound_mix',
    rationale: 'O40: one of four shadowed audio cases; canonical via manage_audio; map.',
  },
  {
    key: 'route:audio:add_source_effect',
    route: 'add_source_effect',
    domain: 'audio',
    status: 'hidden',
    owner: 'Audio',
    evidenceSource: AUDIO_HANDLERS,
    evidenceSymbol: 'Lower == TEXT("add_source_effect")',
    evidenceTool: 'manage_audio',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_audio:add_source_effect',
    rationale: 'O40: one of four shadowed audio cases; canonical via manage_audio; map.',
  },
  {
    key: 'route:asset:analyze_graph',
    route: 'analyze_graph',
    domain: 'asset',
    status: 'raw',
    owner: 'AssetWorkflow',
    evidenceSource: ASSET_WORKFLOW_HANDLERS,
    evidenceSymbol: 'Lower == TEXT("analyze_graph")',
    evidenceTool: 'manage_asset',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_asset:analyze_graph',
    rationale: 'C17/O19: transport-dependent semantics/outputs; map to canonical id with transport-aware contract.',
  },
  {
    key: 'route:asset:get_source_control_state',
    route: 'get_source_control_state',
    domain: 'asset',
    status: 'raw',
    owner: 'AssetQuery',
    evidenceSource: ASSET_QUERY_HANDLERS,
    evidenceSymbol: 'SubAction == TEXT("get_source_control_state")',
    evidenceTool: 'manage_asset',
    disposition: 'map',
    targetCanonicalId: 'cap:manage_asset:get_source_control_state',
    rationale: 'C17/O19: transport-dependent semantics/outputs; map to canonical id with transport-aware contract.',
  },
  {
    key: 'route:asset:material_overlap_residual',
    route: 'material_overlap_residual',
    domain: 'asset',
    status: 'hidden',
    owner: 'AssetWorkflow',
    evidenceSource: ASSET_MATERIAL_PINS,
    evidenceSymbol: 'Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)',
    evidenceTool: 'manage_asset',
    citations: MATERIAL_OVERLAP_CITATIONS,
    disposition: 'map',
    targetCanonicalId: 'cap:manage_asset:material_overlap_group',
    rationale:
      'C24/O27: eight same-name material overlaps plus three translated aliases and three implemented forms outside canonical discovery; map overlaps to canonical material ids.',
  },
];
