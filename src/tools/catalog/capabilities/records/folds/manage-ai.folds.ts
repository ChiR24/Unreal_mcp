// Fold specs for manage_ai. Data only; see ../shared/fold.ts.
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_AI_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'create_behavior_tree', selector: 'kind',
    summary: 'Create an AI asset named name, chosen by kind, each saved (saved): behavior_tree a Behavior Tree in path (default /Game/AI/BehaviorTrees), graph_route the same in savePath (default /Game), blackboard a Blackboard in path (default /Game/AI/Blackboards), where a Blackboard already there is kept (alreadyExisted). Any other asset already at the path is refused (ASSET_ALREADY_EXISTS).',
    topics: ['behavior tree', 'blackboard', 'create ai asset'],
    members: { behavior_tree: 'create_behavior_tree', graph_route: 'create', blackboard: 'create_blackboard' },
    aliasMembers: { blackboard: 'create_blackboard_asset' },
  },
  {
    primary: 'edit_behavior_tree', selector: 'edit',
    summary: 'Edit a Behavior Tree graph: add composite, task, decorator, service or generic nodes and subnodes, connect or break connections, configure or remove nodes.',
    topics: ['behavior tree node', 'composite node', 'task node', 'decorator', 'service', 'bt node', 'enemy chases player', 'chase the player', 'enemy that follows the player'],
    members: {
      add_composite: 'add_composite_node', add_task: 'add_task_node', add_decorator: 'add_decorator', add_service: 'add_service', add_node: 'add_node', add_subnode: 'add_subnode',
      connect: 'connect_nodes', break_connections: 'break_connections', configure_node: 'configure_bt_node', set_node_properties: 'set_node_properties', remove_node: 'remove_node',
    },
  },
  {
    primary: 'edit_blackboard', selector: 'edit',
    summary: 'Edit a Blackboard asset (blackboardPath), chosen by edit, each saved: add_key adds keyName of keyType (an Object key a MoveTo target uses wants baseObjectClass /Script/Engine.Actor; an Enum key takes enumType), set_value sets the default a key starts with (UE 5.4+, Enum, Object and Class keys 5.5+), set_key_instance_synced shares one value across every AI. A taken name (NAME_TAKEN), a value of the wrong kind, a key of a parent Blackboard (KEY_IN_PARENT) and engine content are refused.',
    topics: ['blackboard key', 'blackboard value', 'instance synced'],
    members: { add_key: 'add_blackboard_key', set_value: 'set_blackboard_value', set_key_instance_synced: 'set_key_instance_synced' },
  },
  {
    primary: 'run_behavior_tree', selector: 'control',
    summary: 'Make an AIController Blueprint run a Behavior Tree whenever it possesses a pawn (control run, the default: On Possess calls Run Behavior Tree with its BehaviorTree variable), take that out again (stop), only set the variable that holds the tree (assign_tree), or bind a Blackboard to a controller or tree (assign_blackboard). The run logic is engine nodes in the Blueprint, compiled and saved, so it works in a packaged game; a Blueprint that is not an AIController is refused (NOT_AI_CONTROLLER).',
    topics: ['run behavior tree', 'stop behavior tree', 'assign behavior tree', 'assign blackboard'],
    members: { run: 'run_behavior_tree', stop: 'stop_behavior_tree', assign_tree: 'assign_behavior_tree', assign_blackboard: 'assign_blackboard' },
  },
  {
    primary: 'get_tree', selector: 'info',
    summary: 'Read a Behavior Tree as a nested node tree: root composite with its tasks, decorators and services, node counts and assigned Blackboard; info=blackboard_value reads a Blackboard key\'s type and default value.',
    topics: ['behavior structure', 'nodes and services', 'blackboard key default'],
    members: { tree: 'get_tree', blackboard_value: 'get_blackboard_value' },
  },
  {
    primary: 'setup_perception', selector: 'setting',
    summary: 'Set up AI perception on a Blueprint or controller: add the component, configure sight, hearing or damage senses, team, or runtime perception.',
    topics: ['ai perception', 'sight config', 'hearing config', 'damage sense', 'perception team', 'sight range'],
    members: {
      setup: 'setup_perception', add_component: 'add_ai_perception_component', sight: 'configure_sight_config', hearing: 'configure_hearing_config',
      damage: 'configure_damage_sense_config', team: 'set_perception_team', controller: 'set_ai_perception',
    },
  },
  {
    primary: 'configure_navigation', selector: 'setting',
    summary: 'Configure navigation: nav mesh settings, agent properties, area cost or class, nav links, smart links, or rebuild the nav mesh. A level with no nav mesh yet gets one from a NavMeshBoundsVolume (manage_level_structure create_volume).',
    topics: ['nav mesh', 'navigation', 'nav agent', 'nav area', 'nav link', 'smart link', 'rebuild navigation', 'navmesh', 'add navmesh'],
    members: {
      mesh_settings: 'configure_nav_mesh_settings', agent_properties: 'set_nav_agent_properties', area_cost: 'configure_nav_area_cost', area_class: 'set_nav_area_class',
      nav_link: 'configure_nav_link', link_type: 'set_nav_link_type', smart_link_behavior: 'configure_smart_link_behavior', rebuild: 'rebuild_navigation',
    },
  },
  {
    primary: 'create_nav_actor', selector: 'kind',
    summary: 'Create navigation helpers, chosen by kind: link_proxy places a NavLinkProxy whose point link joins startPoint and endPoint (relative to it) so agents cross a gap the navmesh does not connect; smart_link places one whose smart link alone does, switchable on and off; modifier places a NavModifierVolume marking the navmesh under it with areaClass (default NavArea_Obstacle) or, with blueprintPath, adds a NavModifier component to a Blueprint; modifier_component adds that component to a Blueprint or a placed actor. A label or component name already taken is refused.',
    topics: ['nav link proxy', 'smart link', 'nav modifier', 'add nav modifier'],
    members: { link_proxy: 'create_nav_link_proxy', smart_link: 'create_smart_link', modifier: 'create_nav_modifier', modifier_component: 'create_nav_modifier_component' },
  },
  {
    primary: 'set_focus', topics: ['make look player'], selector: 'focusOp',
    summary: 'Make an AIController Blueprint keep its focus on the player 0 pawn (focusPlayer) or the first actor carrying focusTag, finding it again each frame it has none (focusOp set, the default), or take that logic out (clear). Written into the Blueprint as engine nodes, compiled and saved, so it works in a packaged game.',
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
    topics: ['state tree', 'state tree state', 'state tree transition', 'state tree task', 'make a state tree'],
    members: { create: 'create_state_tree', add_state: 'add_state_tree_state', add_transition: 'add_state_tree_transition', configure_task: 'configure_state_tree_task' },
  },
  {
    primary: 'get_ai_info', selector: 'info',
    summary: 'Read an AI asset: a pawn or AI controller Blueprint (perception, team, movement), a Behavior Tree summary, Blackboard keys, State Tree states or EQS query options; info=navigation returns NavMesh and agent settings.',
    topics: ['controller settings', 'list blackboard keys', 'navmesh settings', 'state tree states'],
    members: { ai: 'get_ai_info', navigation: 'get_navigation_info' },
  },
];
