/**
 * manage_blueprint capability records - 121 authored CapabilityRecordSource
 * entries, folded by MANAGE_BLUEPRINT_FOLDS into MANAGE_BLUEPRINT_RECORDS.
 *
 * 39 core (lifecycle 6, scs 8, variables 6, graph-nodes 9, graph-pins 5,
 * functions 4, probe 1) + 82 widget (lifecycle, panels, content, game-ui,
 * templates, layout, bindings, animation, info). The authored total is pinned
 * by parent-metadata.test.ts.
 *
 * 21 hidden operations have explicit route dispositions (promote/map/remove)
 * verified in tests against the normalization inventory:
 * - 18 widget promote routes (dead-from-MCP, documented for future promotion)
 * - 2 widget remove routes (apply_style_to_widget, set_animation_speed: no-op)
 * - 1 graph remove route (get_nodes: orphaned/dead)
 * - create_widget map alias encoded on create_widget_blueprint
 *
 * Every record is validated via createCapabilityRecord (Zod + hash) at module
 * load. This module IS on the runtime path: retrieval/aggregate.ts imports
 * MANAGE_BLUEPRINT_RECORDS and records/unfolded.ts imports the unfolded sources.
 */
import type { CapabilityRecord, CapabilityRecordSource } from '../../index.js';
import { createCapabilityRecord } from '../../index.js';
import { BLUEPRINT_LIFECYCLE_RECORDS } from './blueprint-lifecycle.js';
import { FUNCTIONS_EVENTS_RECORDS } from './functions-events.js';
import { GRAPH_NODES_RECORDS } from './graph-nodes.js';
import { GRAPH_PINS_RECORDS } from './graph-pins.js';
import { PROBE_RECORDS } from './probe.js';
import { SCS_COMPONENTS_RECORDS } from './scs-components.js';
import { VARIABLES_METADATA_RECORDS } from './variables-metadata.js';
import { WIDGET_ANIMATION_RECORDS } from './widget-animation.js';
import { WIDGET_BINDINGS_RECORDS } from './widget-bindings.js';
import { WIDGET_CONTENT_RECORDS } from './widget-content.js';
import { WIDGET_GAME_UI_RECORDS } from './widget-game-ui.js';
import { WIDGET_INFO_RECORDS } from './widget-info.js';
import { WIDGET_LAYOUT_RECORDS } from './widget-layout.js';
import { WIDGET_LIFECYCLE_RECORDS } from './widget-lifecycle.js';
import { WIDGET_PANELS_RECORDS } from './widget-panels.js';
import { WIDGET_TEMPLATES_RECORDS } from './widget-templates.js';
import { applyFolds } from '../shared/fold.js';
import { MANAGE_BLUEPRINT_FOLDS } from '../folds/manage-blueprint.folds.js';

/** The authored records before folding; per-action contract tests pin these. */
export const MANAGE_BLUEPRINT_UNFOLDED_SOURCES: readonly CapabilityRecordSource[] = [
  ...BLUEPRINT_LIFECYCLE_RECORDS,
  ...SCS_COMPONENTS_RECORDS,
  ...VARIABLES_METADATA_RECORDS,
  ...GRAPH_NODES_RECORDS,
  ...GRAPH_PINS_RECORDS,
  ...FUNCTIONS_EVENTS_RECORDS,
  ...PROBE_RECORDS,
  ...WIDGET_LIFECYCLE_RECORDS,
  ...WIDGET_PANELS_RECORDS,
  ...WIDGET_CONTENT_RECORDS,
  ...WIDGET_GAME_UI_RECORDS,
  ...WIDGET_TEMPLATES_RECORDS,
  ...WIDGET_LAYOUT_RECORDS,
  ...WIDGET_BINDINGS_RECORDS,
  ...WIDGET_ANIMATION_RECORDS,
  ...WIDGET_INFO_RECORDS,
];

const SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_BLUEPRINT_UNFOLDED_SOURCES, MANAGE_BLUEPRINT_FOLDS, 'manage_blueprint');


export const MANAGE_BLUEPRINT_RECORDS: readonly CapabilityRecord[] = SOURCES.map((source) =>
  createCapabilityRecord(source),
);

export const MANAGE_BLUEPRINT_RECORD_COUNT = MANAGE_BLUEPRINT_RECORDS.length;

export const MANAGE_BLUEPRINT_RECORD_IDS: readonly string[] = MANAGE_BLUEPRINT_RECORDS.map((r) => r.id);
