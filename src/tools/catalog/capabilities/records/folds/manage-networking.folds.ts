// Fold specs for manage_networking. Data only; see ../shared/fold.ts.
import { byTarget } from '../shared/fold-spec.js';
import type { FoldSpec } from '../shared/fold-types.js';

export const MANAGE_NETWORKING_FOLDS: readonly FoldSpec[] = [
  {
    primary: 'configure_replication', selector: 'setting',
    summary: 'Configure replication on a Blueprint: property replication and conditions, RepNotify, net role, dormancy, relevancy, priority, update frequency, cull distance, push model, replicated movement, replication graph, net driver.',
    topics: ['replicate variable', 'replication', 'rep notify', 'net dormancy', 'net role', 'relevancy', 'net update frequency', 'push model'],
    members: {
      property: 'set_property_replicated', condition: 'set_replication_condition', rep_notify: 'set_replicated_using', net_role: 'set_net_role', dormancy: 'set_net_dormancy',
      always_relevant: 'set_always_relevant', only_relevant_to_owner: 'set_only_relevant_to_owner', autonomous_proxy: 'set_autonomous_proxy', priority: 'configure_net_priority',
      update_frequency: 'configure_net_update_frequency', cull_distance: 'configure_net_cull_distance', push_model: 'configure_push_model',
      replicated_movement: 'configure_replicated_movement', replication_graph: 'configure_replication_graph', net_driver: 'configure_net_driver',
    },
  },
  {
    primary: 'configure_prediction', selector: 'setting',
    summary: 'Configure client-side prediction: client, movement, server correction, or add prediction data (a variable replicated only to the owning client).',
    topics: ['client prediction', 'movement prediction', 'server correction', 'network prediction', 'prediction data', 'owner only variable'],
    members: { client: 'configure_client_prediction', movement: 'configure_movement_prediction', server_correction: 'configure_server_correction', add_data: 'add_network_prediction_data' },
  },
  {
    primary: 'configure_rpc', selector: 'setting',
    summary: 'Create an RPC function, or set its validation or reliability.',
    topics: ['rpc', 'remote procedure call', 'rpc reliability', 'rpc validation'],
    members: { create: 'create_rpc_function', validation: 'configure_rpc_validation', reliability: 'set_rpc_reliability' },
  },
  {
    primary: 'check_authority', selector: 'check',
    summary: 'Check an actor\'s network role by actorName: check=has_authority returns hasAuthority and the local net role; check=is_locally_controlled returns whether a pawn is locally controlled and has a local player controller.',
    topics: ['locally controlled pawn', 'net role of actor', 'is server'],
    members: { has_authority: 'check_has_authority', is_locally_controlled: 'check_is_locally_controlled' },
  },
  {
    primary: 'create_framework_class', selector: 'kind',
    summary: 'Create a game framework Blueprint: game mode, game state, game instance, player controller, player state, HUD.',
    topics: ['game mode', 'game state', 'game instance', 'player controller', 'player state', 'hud class'],
    members: { game_mode: 'create_game_mode', game_state: 'create_game_state', game_instance: 'create_game_instance', player_controller: 'create_player_controller', player_state: 'create_player_state', hud: 'create_hud_class' },
  },
  {
    primary: 'configure_game_mode', selector: 'setting',
    summary: 'Configure a game mode: default classes (pawn, controller, state, HUD), rules, respawn, spectating.',
    topics: ['default pawn', 'game rules', 'respawn', 'spectating', 'set game mode'],
    members: {
      default_pawn_class: 'set_default_pawn_class', player_controller_class: 'set_player_controller_class', game_state_class: 'set_game_state_class', player_state_class: 'set_player_state_class',
      hud_class: 'set_hud_class', rules: 'configure_game_rules', respawn: 'set_respawn_rules', spectating: 'configure_spectating',
    },
  },
  {
    primary: 'configure_voice', selector: 'setting',
    summary: 'Voice chat: start or stop it now, set the project VoIP settings (online voice on or off, mic gain, noise gate, silence detection, sample rate), or mute a player.',
    topics: ['voice chat', 'voip settings', 'mic gain', 'noise gate', 'mute player'],
    members: { enable: 'enable_voice_chat', settings: 'configure_voice_settings', mute_player: 'mute_player' },
  },
  {
    primary: 'configure_session', selector: 'setting',
    summary: 'Configure multiplayer for the project: the default game port for LAN hosting and joining, the online subsystem (Null, Steam, EOS...), and the local split-screen layout. Written to DefaultEngine.ini.',
    topics: ['lan play', 'game port', 'server port', 'online subsystem', 'session interface', 'split screen', 'split screen layout'],
    members: { lan_play: 'configure_lan_play', interface: 'configure_session_interface', split_screen: 'configure_split_screen' },
    aliasMembers: { split_screen: 'set_split_screen_type' },
  },
  {
    primary: 'host_lan_server', selector: 'serverOp',
    summary: 'Host a LAN listen server (build the travel URL, optionally travel to it), or make a running Play-In-Editor instance join one.',
    topics: ['host server', 'join server', 'lan server', 'listen server', 'connect to server'],
    members: { host: 'host_lan_server', join: 'join_lan_server' },
  },
  {
    primary: 'configure_input', selector: 'setting',
    summary: 'Enhanced Input: create an input action or mapping context, add or map keys, set triggers and modifiers, enable a context.',
    topics: ['input action', 'input mapping context', 'enhanced input', 'key mapping', 'input trigger', 'input modifier'],
    members: {
      create_action: 'create_input_action', create_mapping_context: 'create_input_mapping_context', add_mapping: 'add_mapping', map_action: 'map_input_action',
      set_trigger: 'set_input_trigger', set_modifier: 'set_input_modifier', enable_mapping: 'enable_input_mapping',
    },
  },
  {
    primary: 'add_legacy_mapping', selector: 'mapping',
    summary: 'Add a legacy input action or axis mapping.',
    members: byTarget('add_legacy_', ['add_legacy_action_mapping', 'add_legacy_axis_mapping'], '_mapping'),
  },
  {
    primary: 'remove_legacy_mapping', selector: 'mapping',
    summary: 'Remove a legacy input action or axis mapping.',
    members: byTarget('remove_legacy_', ['remove_legacy_action_mapping', 'remove_legacy_axis_mapping'], '_mapping'),
  },
];
