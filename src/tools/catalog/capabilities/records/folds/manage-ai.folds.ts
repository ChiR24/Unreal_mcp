// Fold specs for manage_ai. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_AI_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_behavior_tree', selector: 'kind',
    summary: 'Create a Behavior Tree (directly or through the graph route) or a Blackboard asset.',
    topics: ['behavior tree', 'blackboard', 'create ai asset'],
    members: { behavior_tree: 'create_behavior_tree', graph_route: 'create', blackboard: 'create_blackboard' },
    aliasMembers: ['create_blackboard_asset'],
  },
  {
    primary: 'edit_behavior_tree', selector: 'edit',
    summary: 'Edit a Behavior Tree graph: add composite, task, decorator, service or generic nodes and subnodes, connect or break connections, configure or remove nodes.',
    topics: ['behavior tree node', 'composite node', 'task node', 'decorator', 'service', 'bt node'],
    members: {
      add_composite: 'add_composite_node', add_task: 'add_task_node', add_decorator: 'add_decorator', add_service: 'add_service', add_node: 'add_node', add_subnode: 'add_subnode',
      connect: 'connect_nodes', break_connections: 'break_connections', configure_node: 'configure_bt_node', set_node_properties: 'set_node_properties', remove_node: 'remove_node',
    },
  },
  {
    primary: 'edit_blackboard', selector: 'edit',
    summary: 'Edit a Blackboard: add a key, set a key value, or mark a key instance-synced.',
    topics: ['blackboard key', 'blackboard value', 'instance synced'],
    members: { add_key: 'add_blackboard_key', set_value: 'set_blackboard_value', set_key_instance_synced: 'set_key_instance_synced' },
  },
  {
    primary: 'run_behavior_tree', selector: 'control',
    summary: 'Run or stop a Behavior Tree on an AI controller, or assign a tree or blackboard to it.',
    topics: ['run behavior tree', 'stop behavior tree', 'assign behavior tree', 'assign blackboard'],
    members: { run: 'run_behavior_tree', stop: 'stop_behavior_tree', assign_tree: 'assign_behavior_tree', assign_blackboard: 'assign_blackboard' },
  },
  {
    primary: 'get_tree', selector: 'info',
    summary: 'Read a Behavior Tree graph or a Blackboard key value.',
    members: { tree: 'get_tree', blackboard_value: 'get_blackboard_value' },
  },
  {
    primary: 'setup_perception', selector: 'setting',
    summary: 'Set up AI perception on a Blueprint or controller: add the component, configure sight, hearing or damage senses, team, or runtime perception.',
    topics: ['ai perception', 'sight config', 'hearing config', 'damage sense', 'perception team'],
    members: {
      setup: 'setup_perception', add_component: 'add_ai_perception_component', sight: 'configure_sight_config', hearing: 'configure_hearing_config',
      damage: 'configure_damage_sense_config', team: 'set_perception_team', controller: 'set_ai_perception',
    },
  },
  {
    primary: 'configure_navigation', selector: 'setting',
    summary: 'Configure navigation: nav mesh settings, agent properties, area cost or class, nav links, smart links, or rebuild the nav mesh.',
    topics: ['nav mesh', 'navigation', 'nav agent', 'nav area', 'nav link', 'smart link', 'rebuild navigation'],
    members: {
      mesh_settings: 'configure_nav_mesh_settings', agent_properties: 'set_nav_agent_properties', area_cost: 'configure_nav_area_cost', area_class: 'set_nav_area_class',
      nav_link: 'configure_nav_link', link_type: 'set_nav_link_type', smart_link_behavior: 'configure_smart_link_behavior', rebuild: 'rebuild_navigation',
    },
  },
  {
    primary: 'create_nav_actor', selector: 'kind',
    summary: 'Create a nav link proxy, a smart link, or a nav modifier (actor or component).',
    topics: ['nav link proxy', 'smart link', 'nav modifier'],
    members: { link_proxy: 'create_nav_link_proxy', smart_link: 'create_smart_link', modifier: 'create_nav_modifier', modifier_component: 'create_nav_modifier_component' },
  },
  {
    primary: 'set_focus', selector: 'focusOp',
    summary: 'Set or clear an AI controller\'s focus actor.',
    members: { set: 'set_focus', clear: 'clear_focus' },
  },
  {
    primary: 'edit_eqs_query', selector: 'edit',
    summary: 'Create an EQS query or edit it: add generators, tests and contexts, configure test scoring.',
    topics: ['eqs', 'environment query', 'eqs generator', 'eqs test'],
    members: { create: 'create_eqs_query', add_generator: 'add_eqs_generator', add_test: 'add_eqs_test', add_context: 'add_eqs_context', configure_test_scoring: 'configure_test_scoring' },
  },
  {
    primary: 'edit_mass_entity', selector: 'edit',
    summary: 'Create a Mass entity config, configure its traits, or add a Mass spawner.',
    topics: ['mass entity', 'mass spawner', 'mass config'],
    members: { create_config: 'create_mass_entity_config', configure: 'configure_mass_entity', add_spawner: 'add_mass_spawner' },
  },
  {
    primary: 'edit_smart_object', selector: 'edit',
    summary: 'Create a Smart Object definition, add slots, configure slot behavior, or add a Smart Object component.',
    topics: ['smart object', 'smart object slot', 'smart object definition'],
    members: { create_definition: 'create_smart_object_definition', add_slot: 'add_smart_object_slot', configure_slot: 'configure_slot_behavior', add_component: 'add_smart_object_component' },
  },
  {
    primary: 'edit_state_tree', selector: 'edit',
    summary: 'Create a State Tree or edit it: add states and transitions, configure tasks.',
    topics: ['state tree', 'state tree state', 'state tree transition', 'state tree task'],
    members: { create: 'create_state_tree', add_state: 'add_state_tree_state', add_transition: 'add_state_tree_transition', configure_task: 'configure_state_tree_task' },
  },
  {
    primary: 'get_ai_info', selector: 'info',
    summary: 'Read AI asset or controller information, or navigation system information.',
    members: { ai: 'get_ai_info', navigation: 'get_navigation_info' },
  },
];
