// src/tools/catalog/capabilities/records/compensation.ts
// How to reverse a successful call, for the capabilities that have a way. An
// inverse names a capability that reverses the durable effect; it never implies
// the original call was rolled back. Keys and inverses are folded record ids
// (compensation.test.ts holds them to that). A family is listed only when every
// member's effect is reversed by the inverse and the call hands back everything
// the inverse needs, which is why edit_graph, edit_struct, create_sky_light
// (ensure_single) and the start/stop families (one record for both) have none.

import { CapabilityIdSchema } from '../identifiers.js';
import type { CapabilityCompensation } from '../model.js';

const inverse = (id: string): CapabilityCompensation => ({ inverse: [CapabilityIdSchema.parse(id)] });

export const COMPENSATION: ReadonlyMap<string, CapabilityCompensation> = new Map([
  ['control_actor.spawn', inverse('control_actor.delete')],
  ['control_actor.duplicate', inverse('control_actor.delete')],
  ['build_environment.create_light', inverse('build_environment.delete')],
  ['build_environment.create_landscape', inverse('build_environment.delete')],
  ['control_editor.open_asset', inverse('control_editor.close_asset')],
  ['manage_tools.enable_tools', inverse('manage_tools.disable_tools')],
  ['manage_tools.enable_category', inverse('manage_tools.disable_category')],
  ['sequence.mrq.create_render_job', {
    guidance: 'Rendered frames are written to the configured MRQ output directory and are not removed by any capability; delete them from disk. The render itself only supports advisory cancellation while running.',
  }],
]);
