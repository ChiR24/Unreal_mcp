// manage_blueprint capability records (core blueprint + widget authoring),
// folded by MANAGE_BLUEPRINT_FOLDS into MANAGE_BLUEPRINT_SOURCES.
import type { CapabilityRecordSource } from '../../model.js';
import { BLUEPRINT_LIFECYCLE_RECORDS } from './blueprint-lifecycle.js';
import { FUNCTIONS_EVENTS_RECORDS } from './functions-events.js';
import { GRAPH_BATCH_RECORDS } from './graph-batch.js';
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
  ...GRAPH_BATCH_RECORDS,
  ...FUNCTIONS_EVENTS_RECORDS,
  ...PROBE_RECORDS,
  ...WIDGET_LIFECYCLE_RECORDS,
  ...WIDGET_PANELS_RECORDS,
  ...WIDGET_CONTENT_RECORDS,
  ...WIDGET_LAYOUT_RECORDS,
  ...WIDGET_BINDINGS_RECORDS,
  ...WIDGET_ANIMATION_RECORDS,
  ...WIDGET_INFO_RECORDS,
  ...WIDGET_GAME_UI_RECORDS,
  ...WIDGET_TEMPLATES_RECORDS,
];

export const MANAGE_BLUEPRINT_SOURCES: readonly CapabilityRecordSource[] = applyFolds(MANAGE_BLUEPRINT_UNFOLDED_SOURCES, MANAGE_BLUEPRINT_FOLDS, 'manage_blueprint');

