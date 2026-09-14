// Fold specs for manage_interaction. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_INTERACTION_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_interactable', selector: 'kind',
    summary: 'Create an interactable: door, chest, switch, lever or trigger actor, an interactable interface, or an interaction component.',
    topics: ['door', 'chest', 'switch', 'lever', 'trigger actor', 'interactable interface', 'interaction component', 'create door'],
    members: { door: 'create_door_actor', chest: 'create_chest_actor', switch: 'create_switch_actor', lever: 'create_lever_actor', trigger: 'create_trigger_actor', interface: 'create_interactable_interface', component: 'create_interaction_component' },
  },
  {
    primary: 'configure_interactable', selector: 'setting',
    summary: 'Configure an interactable: door, chest or switch properties, trigger events/filter/response, interaction trace or widget, or add interaction events.',
    topics: ['door properties', 'chest properties', 'switch properties', 'trigger events', 'interaction trace', 'interaction widget'],
    members: {
      door: 'configure_door_properties', chest: 'configure_chest_properties', switch: 'configure_switch_properties', trigger_events: 'configure_trigger_events',
      trigger_filter: 'configure_trigger_filter', trigger_response: 'configure_trigger_response', trace: 'configure_interaction_trace', widget: 'configure_interaction_widget',
      add_events: 'add_interaction_events',
    },
  },
  {
    primary: 'configure_destruction', selector: 'setting',
    summary: 'Set up a destructible: add the component, set up the mesh, configure damage, effects and destruction levels.',
    topics: ['destructible', 'destruction component', 'destruction damage', 'destruction levels', 'chaos destruction'],
    members: { add_component: 'add_destruction_component', setup_mesh: 'setup_destructible_mesh', damage: 'configure_destruction_damage', effects: 'configure_destruction_effects', levels: 'configure_destruction_levels' },
  },
];
