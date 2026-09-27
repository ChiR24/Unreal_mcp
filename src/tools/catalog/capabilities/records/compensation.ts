// src/tools/catalog/capabilities/records/compensation.ts
// How to reverse a successful call, for the capabilities that have a way. An
// inverse names a capability that reverses the durable effect; it never implies
// the original call was rolled back. One is listed only when the original call
// hands back everything the inverse needs, which is why delete_node,
// break_pin_links, set_pin_default_value and set_node_property have none.
// blueprint.connect_pins' inverse is over-broad: break_pin_links severs every
// link on the pin, not just the one connect_pins added.

import { CapabilityIdSchema } from '../identifiers.js';
import type { CapabilityCompensation } from '../model.js';

const inverse = (id: string): CapabilityCompensation => ({ inverse: [CapabilityIdSchema.parse(id)] });

export const COMPENSATION: ReadonlyMap<string, CapabilityCompensation> = new Map([
  ['control_actor.spawn', inverse('control_actor.delete')],
  ['control_actor.spawn_actor', inverse('control_actor.delete')],
  ['control_actor.spawn_blueprint', inverse('control_actor.delete')],
  ['control_actor.duplicate', inverse('control_actor.delete')],
  ['build_environment.spawn_light', inverse('build_environment.delete')],
  ['build_environment.spawn_sky_light', inverse('build_environment.delete')],
  ['build_environment.create_landscape', inverse('build_environment.delete')],
  ['manage_audio.push_sound_mix', inverse('manage_audio.pop_sound_mix')],
  ['control_editor.start_recording', inverse('control_editor.stop_recording')],
  ['control_editor.open_asset', inverse('control_editor.close_asset')],
  ['system_control.start_profiling', inverse('system_control.stop_profiling')],
  ['system_control.start_session', inverse('system_control.stop_session')],
  ['manage_tools.enable_tools', inverse('manage_tools.disable_tools')],
  ['manage_tools.enable_category', inverse('manage_tools.disable_category')],
  ['sequence.take.start_recording', inverse('sequence.take.stop_recording')],
  ['sequence.replay.start_demo_recording', inverse('sequence.replay.stop_demo_recording')],
  ['struct.import_struct', inverse('struct.delete_struct')],
  ['blueprint.create_node', inverse('blueprint.delete_node')],
  ['blueprint.create_reroute_node', inverse('blueprint.delete_node')],
  ['blueprint.connect_pins', inverse('blueprint.break_pin_links')],
  ['sequence.mrq.start_render', {
    guidance: 'Rendered frames are written to the configured MRQ output directory and are not removed by any capability; delete them from disk. The render itself only supports advisory cancellation while running.',
  }],
]);
