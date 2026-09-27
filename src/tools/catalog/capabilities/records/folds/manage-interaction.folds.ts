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
    summary: 'Configure an interactable: door, chest or switch properties, or its interaction trace.',
    topics: ['door properties', 'chest properties', 'switch properties', 'interaction trace'],
    members: {
      door: 'configure_door_properties', chest: 'configure_chest_properties', switch: 'configure_switch_properties', trace: 'configure_interaction_trace',
    },
  },
];
