#!/usr/bin/env node
/**
 * manage_networking Tool Integration Tests
 * Exercises all networking actions against real Blueprint/actor targets.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayNetworking_${ts}`;
const ACTOR_BP_NAME = `BP_NetworkActor_${ts}`;
const CHARACTER_BP_NAME = `BP_NetworkCharacter_${ts}`;
const ACTOR_BP_PATH = `${TEST_FOLDER}/${ACTOR_BP_NAME}`;
const CHARACTER_BP_PATH = `${TEST_FOLDER}/${CHARACTER_BP_NAME}`;
const EXPECTED_ACTOR_BP_ASSET_PATH = ACTOR_BP_PATH;
const EXPECTED_CHARACTER_BP_ASSET_PATH = CHARACTER_BP_PATH;
const TARGET_ACTOR = `MCP_NetworkTarget_${ts}`;
const OWNER_ACTOR = `MCP_NetworkOwner_${ts}`;
const PAWN_ACTOR = `MCP_NetworkPawn_${ts}`;
const RPC_FUNCTION = `Server_DoThing_${ts}`;
const REP_NOTIFY_FUNCTION = `OnRep_NetworkValue_${ts}`;
const PREDICTION_VAR = `PredictedLocation_${ts}`;

const blueprintAssertion = (expectedPath, label) => [
  { path: 'structuredContent.result.success', equals: true, label: `${label} native success flag` },
  { path: 'structuredContent.result.existsAfter', equals: true, label: `${label} asset still exists` },
  { path: 'structuredContent.result.assetPath', equals: expectedPath, label: `${label} target package verified` }
];

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  {
    scenario: 'Setup: create replicated actor blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create_blueprint', name: ACTOR_BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success|already exists',
    assertions: [{ path: 'structuredContent.result.assetPath', equals: EXPECTED_ACTOR_BP_ASSET_PATH, label: 'network actor blueprint created' }]
  },
  { scenario: 'Setup: add replicated flag variable', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: ACTOR_BP_PATH, variableName: 'ReplicatedFlag', variableType: 'Boolean' }, expected: 'success|already exists' },
  { scenario: 'Setup: add rep notify variable', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: ACTOR_BP_PATH, variableName: 'RepNotifyValue', variableType: 'Float' }, expected: 'success|already exists' },
  { scenario: 'Setup: compile network actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'compile', blueprintPath: ACTOR_BP_PATH }, expected: 'success' },
  {
    scenario: 'Setup: create network character blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create_blueprint', name: CHARACTER_BP_NAME, path: TEST_FOLDER, parentClass: 'Character' },
    expected: 'success|already exists',
    assertions: [{ path: 'structuredContent.result.assetPath', equals: EXPECTED_CHARACTER_BP_ASSET_PATH, label: 'network character blueprint created' }]
  },
  { scenario: 'Setup: spawn target actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TARGET_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TARGET_ACTOR, label: 'network target actor spawned' }] },
  { scenario: 'Setup: spawn owner actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: OWNER_ACTOR, location: { x: 160, y: 0, z: 100 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: OWNER_ACTOR, label: 'network owner actor spawned' }] },
  { scenario: 'Setup: spawn pawn actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Script/Engine.DefaultPawn', actorName: PAWN_ACTOR, location: { x: 320, y: 0, z: 140 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: PAWN_ACTOR, label: 'network pawn actor spawned' }] },

  // === REPLICATION ===
  { scenario: 'CONFIG: set_property_replicated', toolName: 'manage_networking', arguments: { action: 'set_property_replicated', blueprintPath: ACTOR_BP_PATH, propertyName: 'ReplicatedFlag', replicated: true }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_property_replicated') },
  { scenario: 'CONFIG: set_replication_condition', toolName: 'manage_networking', arguments: { action: 'set_replication_condition', blueprintPath: ACTOR_BP_PATH, propertyName: 'ReplicatedFlag', condition: 'COND_OwnerOnly' }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_replication_condition') },
  { scenario: 'CONFIG: configure_net_update_frequency', toolName: 'manage_networking', arguments: { action: 'configure_net_update_frequency', blueprintPath: ACTOR_BP_PATH, netUpdateFrequency: 33, minNetUpdateFrequency: 7 }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_net_update_frequency') },
  { scenario: 'CONFIG: configure_net_priority', toolName: 'manage_networking', arguments: { action: 'configure_net_priority', blueprintPath: ACTOR_BP_PATH, netPriority: 2.75 }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_net_priority') },
  { scenario: 'CONFIG: set_net_dormancy', toolName: 'manage_networking', arguments: { action: 'set_net_dormancy', blueprintPath: ACTOR_BP_PATH, dormancy: 'DORM_Awake' }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_net_dormancy') },
  {
    scenario: 'CONFIG: configure_replication_graph',
    toolName: 'manage_networking',
    arguments: { action: 'configure_replication_graph', blueprintPath: ACTOR_BP_PATH, spatiallyLoaded: true, netLoadOnClient: false, replicationPolicy: 'Spatial' },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_replication_graph'),
      { path: 'structuredContent.result.spatiallyLoaded', equals: true, label: 'replication graph spatial flag returned' },
      { path: 'structuredContent.result.netLoadOnClient', equals: false, label: 'replication graph client load flag returned' },
      { path: 'structuredContent.result.replicationPolicy', equals: 'Spatial', label: 'replication policy returned' }
    ]
  },

  // === RPCS ===
  {
    scenario: 'CREATE: create_rpc_function',
    toolName: 'manage_networking',
    arguments: { action: 'create_rpc_function', blueprintPath: ACTOR_BP_PATH, functionName: RPC_FUNCTION, rpcType: 'Server', reliable: true },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'create_rpc_function'),
      { path: 'structuredContent.result.functionName', equals: RPC_FUNCTION, label: 'RPC function name returned' },
      { path: 'structuredContent.result.rpcType', equals: 'Server', label: 'RPC type returned' },
      { path: 'structuredContent.result.reliable', equals: true, label: 'RPC reliability returned at creation' }
    ]
  },
  {
    scenario: 'CONFIG: configure_rpc_validation',
    toolName: 'manage_networking',
    arguments: { action: 'configure_rpc_validation', blueprintPath: ACTOR_BP_PATH, functionName: RPC_FUNCTION, withValidation: true },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_rpc_validation'),
      { path: 'structuredContent.result.withValidation', equals: true, label: 'RPC validation flag returned' }
    ]
  },
  {
    scenario: 'CONFIG: set_rpc_reliability',
    toolName: 'manage_networking',
    arguments: { action: 'set_rpc_reliability', blueprintPath: ACTOR_BP_PATH, functionName: RPC_FUNCTION, reliable: false },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_rpc_reliability'),
      { path: 'structuredContent.result.reliable', equals: false, label: 'RPC reliability updated' }
    ]
  },

  // === AUTHORITY / OWNERSHIP ===
  { scenario: 'CONFIG: set_owner', toolName: 'manage_networking', arguments: { action: 'set_owner', actorName: TARGET_ACTOR, ownerActorName: OWNER_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'set_owner native success flag' }, { path: 'structuredContent.result.actorName', equals: TARGET_ACTOR, label: 'set_owner target actor verified' }, { path: 'structuredContent.result.existsAfter', equals: true, label: 'set_owner target actor still exists' }] },
  {
    scenario: 'CONFIG: set_autonomous_proxy',
    toolName: 'manage_networking',
    arguments: { action: 'set_autonomous_proxy', blueprintPath: ACTOR_BP_PATH, isAutonomousProxy: true },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_autonomous_proxy'),
      { path: 'structuredContent.result.isAutonomousProxy', equals: true, label: 'autonomous proxy flag returned' }
    ]
  },
  { scenario: 'INFO: check_has_authority', toolName: 'manage_networking', arguments: { action: 'check_has_authority', actorName: TARGET_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'authority check native success flag' }, { path: 'structuredContent.result.hasAuthority', equals: true, label: 'editor actor has authority' }, { path: 'structuredContent.result.role', equals: 'ROLE_Authority', label: 'editor actor role returned' }] },
  { scenario: 'INFO: check_is_locally_controlled', toolName: 'manage_networking', arguments: { action: 'check_is_locally_controlled', actorName: PAWN_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'local-control check native success flag' }, { path: 'structuredContent.result.isLocallyControlled', equals: false, label: 'unpossessed editor pawn is not locally controlled' }, { path: 'structuredContent.result.isLocalController', equals: false, label: 'unpossessed editor pawn has no local controller' }] },

  // === RELEVANCY / SERIALIZATION ===
  { scenario: 'CONFIG: configure_net_cull_distance', toolName: 'manage_networking', arguments: { action: 'configure_net_cull_distance', blueprintPath: ACTOR_BP_PATH, netCullDistanceSquared: 640000, useOwnerNetRelevancy: true }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_net_cull_distance') },
  { scenario: 'CONFIG: set_always_relevant', toolName: 'manage_networking', arguments: { action: 'set_always_relevant', blueprintPath: ACTOR_BP_PATH, alwaysRelevant: true }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_always_relevant') },
  { scenario: 'CONFIG: set_only_relevant_to_owner', toolName: 'manage_networking', arguments: { action: 'set_only_relevant_to_owner', blueprintPath: ACTOR_BP_PATH, onlyRelevantToOwner: false }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_only_relevant_to_owner') },
  {
    scenario: 'CONFIG: configure_net_serialization',
    toolName: 'manage_networking',
    arguments: { action: 'configure_net_serialization', blueprintPath: ACTOR_BP_PATH, structName: 'MCPNetPayload', customSerialization: true },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_net_serialization'),
      { path: 'structuredContent.result.customSerialization', equals: true, label: 'custom serialization flag returned' },
      { path: 'structuredContent.result.structName', equals: 'MCPNetPayload', label: 'net serialization struct returned' }
    ]
  },
  { scenario: 'CONFIG: set_replicated_using', toolName: 'manage_networking', arguments: { action: 'set_replicated_using', blueprintPath: ACTOR_BP_PATH, propertyName: 'RepNotifyValue', repNotifyFunc: REP_NOTIFY_FUNCTION }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_replicated_using') },
  {
    scenario: 'CONFIG: configure_push_model',
    toolName: 'manage_networking',
    arguments: { action: 'configure_push_model', blueprintPath: ACTOR_BP_PATH, usePushModel: true },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_push_model'),
      { path: 'structuredContent.result.usePushModel', equals: true, label: 'push model flag returned' }
    ]
  },

  // === NETWORK PREDICTION ===
  {
    scenario: 'CONFIG: configure_client_prediction',
    toolName: 'manage_networking',
    arguments: { action: 'configure_client_prediction', blueprintPath: CHARACTER_BP_PATH, enablePrediction: true, predictionThreshold: 0.25 },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_CHARACTER_BP_ASSET_PATH, 'configure_client_prediction'),
      { path: 'structuredContent.result.enablePrediction', equals: true, label: 'client prediction flag returned' },
      { path: 'structuredContent.result.predictionThreshold', equals: 0.25, label: 'client prediction threshold returned' }
    ]
  },
  {
    scenario: 'CONFIG: configure_server_correction',
    toolName: 'manage_networking',
    arguments: { action: 'configure_server_correction', blueprintPath: CHARACTER_BP_PATH, correctionThreshold: 1.5, smoothingRate: 0.35 },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_CHARACTER_BP_ASSET_PATH, 'configure_server_correction'),
      { path: 'structuredContent.result.correctionThreshold', equals: 1.5, label: 'server correction threshold returned' },
      { path: 'structuredContent.result.smoothingRate', equals: 0.35, label: 'server correction smoothing rate returned' }
    ]
  },
  {
    scenario: 'ADD: add_network_prediction_data',
    toolName: 'manage_networking',
    arguments: { action: 'add_network_prediction_data', blueprintPath: CHARACTER_BP_PATH, dataType: 'Vector', variableName: PREDICTION_VAR },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_CHARACTER_BP_ASSET_PATH, 'add_network_prediction_data'),
      { path: 'structuredContent.result.variableName', equals: PREDICTION_VAR, label: 'network prediction variable returned' },
      { path: 'structuredContent.result.dataType', equals: 'Vector', label: 'network prediction data type returned' }
    ]
  },
  { scenario: 'CONFIG: configure_movement_prediction', toolName: 'manage_networking', arguments: { action: 'configure_movement_prediction', blueprintPath: CHARACTER_BP_PATH, networkSmoothingMode: 'Exponential', networkMaxSmoothUpdateDistance: 512, networkNoSmoothUpdateDistance: 768 }, expected: 'success', assertions: blueprintAssertion(EXPECTED_CHARACTER_BP_ASSET_PATH, 'configure_movement_prediction') },

  // === CONNECTION / ROLE / INFO ===
  {
    scenario: 'CONFIG: configure_net_driver',
    toolName: 'manage_networking',
    arguments: { action: 'configure_net_driver', maxClientRate: 20000, maxInternetClientRate: 12000, netServerMaxTickRate: 45 },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.success', equals: true, label: 'net driver native success flag' },
      { path: 'structuredContent.result.maxClientRate', equals: 20000, label: 'net driver max client rate returned' },
      { path: 'structuredContent.result.maxInternetClientRate', equals: 12000, label: 'net driver internet client rate returned' },
      { path: 'structuredContent.result.netServerMaxTickRate', equals: 45, label: 'net driver max tick rate returned' }
    ]
  },
  {
    scenario: 'CONFIG: set_net_role',
    toolName: 'manage_networking',
    arguments: { action: 'set_net_role', blueprintPath: ACTOR_BP_PATH, role: 'ROLE_AutonomousProxy' },
    expected: 'success',
    assertions: [
      ...blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'set_net_role'),
      { path: 'structuredContent.result.role', equals: 'ROLE_AutonomousProxy', label: 'net role returned' },
      { path: 'structuredContent.result.replicates', equals: true, label: 'proxy role enables replication' }
    ]
  },
  { scenario: 'CONFIG: configure_replicated_movement', toolName: 'manage_networking', arguments: { action: 'configure_replicated_movement', blueprintPath: ACTOR_BP_PATH, replicateMovement: true }, expected: 'success', assertions: blueprintAssertion(EXPECTED_ACTOR_BP_ASSET_PATH, 'configure_replicated_movement') },
  {
    scenario: 'INFO: get_networking_info',
    toolName: 'manage_networking',
    arguments: { action: 'get_networking_info', blueprintPath: ACTOR_BP_PATH },
    expected: 'success',
    assertions: [
      { path: 'structuredContent.result.success', equals: true, label: 'networking info native success flag' },
      { path: 'structuredContent.result.networkingInfo.bReplicates', equals: true, label: 'networking info reads replicated actor CDO' },
      { path: 'structuredContent.result.networkingInfo.bAlwaysRelevant', equals: true, label: 'networking info reads always relevant flag' },
      { path: 'structuredContent.result.networkingInfo.bOnlyRelevantToOwner', equals: false, label: 'networking info reads owner-only relevancy flag' },
      { path: 'structuredContent.result.networkingInfo.netUpdateFrequency', equals: 33, label: 'networking info reads net update frequency' },
      { path: 'structuredContent.result.networkingInfo.minNetUpdateFrequency', equals: 7, label: 'networking info reads min net update frequency' },
      { path: 'structuredContent.result.networkingInfo.netCullDistanceSquared', equals: 640000, label: 'networking info reads cull distance' },
      { path: 'structuredContent.result.networkingInfo.netPriority', equals: 2.75, label: 'networking info reads net priority' },
      { path: 'structuredContent.result.networkingInfo.netDormancy', equals: 'DORM_Awake', label: 'networking info reads dormancy' }
    ]
  },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [TARGET_ACTOR, OWNER_ACTOR, PAWN_ACTOR] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

// === SESSION AND LOCAL MULTIPLAYER ACTIONS ===
{
  /**
   * manage_networking session action integration tests
   * Exercises all session/local-multiplayer/LAN/voice actions against the live bridge.
   */

  const TEST_FOLDER = '/Game/MCPTest/GameplayAssets';
  const ts = Date.now();

  const SESSION_NAME = `MCP_LiveSession_${ts}`;
  const SERVER_NAME = `MCP_LAN_${ts}`;
  const VOICE_CHANNEL = `MCP_Voice_${ts}`;
  const PLAYER_NAME = `MCP_Player_${ts}`;
  const TARGET_PLAYER_ID = `MCP_TargetId_${ts}`;
  const MAP_PATH = '/Game/MCPTest/MainLevel';
  const SERVER_PORT = 7788;
  const SERVER_PASSWORD = 'mcp-test-password';
  const JOIN_OPTIONS = '?Name=MCPClient';
  const HOST_OPTIONS = '?MCPTest=1';
  const HOST_TRAVEL_URL = `${MAP_PATH}?listen?bIsLanMatch=1?MaxPlayers=4${HOST_OPTIONS}`;
  const JOIN_CONNECTION_URL = `127.0.0.1:${SERVER_PORT}${JOIN_OPTIONS}?Password=${SERVER_PASSWORD}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

    // === SESSION CONFIGURATION ===
    {
      scenario: 'CONFIG: configure_local_session_settings',
      toolName: 'manage_networking',
      arguments: {
        action: 'configure_local_session_settings',
        sessionName: SESSION_NAME,
        maxPlayers: 4,
        bIsLANMatch: true,
        bAllowJoinInProgress: false,
        bAllowInvites: true,
        bUsesPresence: false,
        bUseLobbiesIfAvailable: false,
        bShouldAdvertise: true
      },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.sessionName', equals: SESSION_NAME, label: 'session name applied' },
        { path: 'structuredContent.result.maxPlayers', equals: 4, label: 'session max players applied' },
        { path: 'structuredContent.result.bIsLANMatch', equals: true, label: 'LAN match flag applied' },
        { path: 'structuredContent.result.bAllowJoinInProgress', equals: false, label: 'join-in-progress flag applied' }
      ]
    },
    {
      scenario: 'CONFIG: configure_session_interface',
      toolName: 'manage_networking',
      arguments: { action: 'configure_session_interface', interfaceType: 'Null' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.interfaceType', equals: 'Null', label: 'session interface type applied' },
        { path: 'structuredContent.result.status', equals: 'configured', label: 'session interface configured status returned' }
      ]
    },

    // === SPLIT-SCREEN CONFIGURATION ===
    {
      scenario: 'CONFIG: configure_split_screen',
      toolName: 'manage_networking',
      arguments: { action: 'configure_split_screen', enabled: true, splitScreenType: 'TwoPlayer_Vertical' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.enabled', equals: true, label: 'split-screen enabled flag applied' },
        { path: 'structuredContent.result.splitScreenType', equals: 'TwoPlayer_Vertical', label: 'split-screen type applied' },
        { path: 'structuredContent.result.verticalSplit', equals: true, label: 'vertical split detected' },
        { path: 'structuredContent.result.success', equals: true, label: 'split-screen native configuration succeeded' }
      ]
    },
    {
      scenario: 'CONFIG: set_split_screen_type',
      toolName: 'manage_networking',
      arguments: { action: 'set_split_screen_type', splitScreenType: 'FourPlayer_Grid' },
      expected: 'success',
      assertions: [{ path: 'structuredContent.result.splitScreenType', equals: 'FourPlayer_Grid', label: 'split-screen layout returned' }]
    },

    // === PIE-ONLY LOCAL MULTIPLAYER ===
    { scenario: 'PLAYBACK: start PIE for local players', toolName: 'control_editor', arguments: { action: 'play' }, expected: 'success' },
    {
      scenario: 'INFO: get_sessions_info before adding local player',
      toolName: 'manage_networking',
      arguments: { action: 'get_sessions_info' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.sessionsInfo.inPlaySession', equals: true, label: 'PIE session active before local-player mutation' },
        { path: 'structuredContent.result.sessionsInfo.localPlayerCount', equals: 1, label: 'primary local player present before add' }
      ]
    },
    {
      scenario: 'ADD: add_local_player',
      toolName: 'manage_networking',
      arguments: { action: 'add_local_player', controllerId: 1 },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.playerIndex', equals: 1, label: 'secondary local player index returned' },
        { path: 'structuredContent.result.controllerId', equals: 1, label: 'secondary local player controller returned' },
        { path: 'structuredContent.result.totalLocalPlayers', equals: 2, label: 'local player count incremented' }
      ]
    },
    {
      scenario: 'INFO: get_sessions_info after adding local player',
      toolName: 'manage_networking',
      arguments: { action: 'get_sessions_info' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.sessionsInfo.localPlayerCount', equals: 2, label: 'local player count read back after add' },
        { path: 'structuredContent.result.sessionsInfo.currentPlayers', equals: 2, label: 'current player count read back after add' },
        { path: 'structuredContent.result.sessionsInfo.splitScreenEnabled', equals: true, label: 'split-screen active with two local players' }
      ]
    },
    {
      scenario: 'DELETE: remove_local_player',
      toolName: 'manage_networking',
      arguments: { action: 'remove_local_player', playerIndex: 1 },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.removedPlayerIndex', equals: 1, label: 'secondary local player removed' },
        { path: 'structuredContent.result.remainingPlayers', equals: 1, label: 'local player count decremented' }
      ]
    },
    {
      scenario: 'INFO: get_sessions_info after removing local player',
      toolName: 'manage_networking',
      arguments: { action: 'get_sessions_info' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.sessionsInfo.localPlayerCount', equals: 1, label: 'local player count read back after remove' },
        { path: 'structuredContent.result.sessionsInfo.currentPlayers', equals: 1, label: 'current player count read back after remove' },
        { path: 'structuredContent.result.sessionsInfo.splitScreenEnabled', equals: false, label: 'split-screen inactive after removing secondary player' }
      ]
    },

    // === LAN CONFIGURATION ===
    {
      scenario: 'CONFIG: configure_lan_play',
      toolName: 'manage_networking',
      arguments: { action: 'configure_lan_play', enabled: true, serverPort: SERVER_PORT, serverPassword: SERVER_PASSWORD },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.enabled', equals: true, label: 'LAN play enabled flag applied' },
        { path: 'structuredContent.result.serverPort', equals: SERVER_PORT, label: 'LAN server port applied' },
        { path: 'structuredContent.result.hasPassword', equals: true, label: 'LAN password presence returned' }
      ]
    },
    {
      scenario: 'ACTION: host_lan_server',
      toolName: 'manage_networking',
      arguments: { action: 'host_lan_server', serverName: SERVER_NAME, mapName: MAP_PATH, maxPlayers: 4, travelOptions: HOST_OPTIONS, executeTravel: false },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.serverName', equals: SERVER_NAME, label: 'LAN server name returned' },
        { path: 'structuredContent.result.mapPath', equals: MAP_PATH, label: 'LAN host map path returned' },
        { path: 'structuredContent.result.travelURL', equals: HOST_TRAVEL_URL, label: 'LAN host travel URL constructed' },
        { path: 'structuredContent.result.travelExecuted', equals: false, label: 'LAN host test avoids disruptive travel' }
      ]
    },
    {
      scenario: 'ACTION: join_lan_server',
      toolName: 'manage_networking',
      arguments: { action: 'join_lan_server', serverAddress: '127.0.0.1', serverPort: SERVER_PORT, serverPassword: SERVER_PASSWORD, travelOptions: JOIN_OPTIONS },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.serverAddress', equals: `127.0.0.1:${SERVER_PORT}`, label: 'LAN join address normalized with port' },
        { path: 'structuredContent.result.connectionURL', equals: JOIN_CONNECTION_URL, label: 'LAN join connection URL constructed' },
        { path: 'structuredContent.result.status', equals: 'configured', label: 'LAN join configured status returned' }
      ]
    },

    // === VOICE CHAT CONFIGURATION ===
    {
      scenario: 'TOGGLE: enable_voice_chat disabled state',
      toolName: 'manage_networking',
      arguments: { action: 'enable_voice_chat', voiceEnabled: false },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.voiceEnabled', equals: false, label: 'voice chat disabled flag applied' },
        { path: 'structuredContent.result.success', equals: true, label: 'voice chat disable path completed' }
      ]
    },
    {
      scenario: 'CONFIG: configure_voice_settings',
      toolName: 'manage_networking',
      arguments: {
        action: 'configure_voice_settings',
        voiceSettings: {
          volume: 0.42,
          noiseGateThreshold: 0.03,
          noiseSuppression: false,
          echoCancellation: true,
          sampleRate: 24000
        }
      },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.voiceSettings.volume', equals: 0.42, label: 'voice volume applied' },
        { path: 'structuredContent.result.voiceSettings.noiseGateThreshold', equals: 0.03, label: 'voice noise gate applied' },
        { path: 'structuredContent.result.voiceSettings.noiseSuppression', equals: false, label: 'voice noise suppression applied' },
        { path: 'structuredContent.result.voiceSettings.echoCancellation', equals: true, label: 'voice echo cancellation applied' },
        { path: 'structuredContent.result.voiceSettings.sampleRate', equals: 24000, label: 'voice sample rate applied' }
      ]
    },
    {
      scenario: 'CONFIG: set_voice_channel',
      toolName: 'manage_networking',
      arguments: { action: 'set_voice_channel', channelName: VOICE_CHANNEL, channelType: 'Party' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.channelName', equals: VOICE_CHANNEL, label: 'voice channel name returned' },
        { path: 'structuredContent.result.channelType', equals: 'Party', label: 'voice channel type returned' }
      ]
    },
    {
      scenario: 'ACTION: mute_player',
      toolName: 'manage_networking',
      arguments: { action: 'mute_player', playerName: PLAYER_NAME, muted: true, localPlayerNum: 0, systemWide: false },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.target', equals: PLAYER_NAME, label: 'mute player target returned' },
        { path: 'structuredContent.result.muted', equals: true, label: 'mute state returned' },
        { path: 'structuredContent.result.success', equals: true, label: 'mute request completed' }
      ]
    },
    {
      scenario: 'ACTION: mute_player via targetPlayerId',
      toolName: 'manage_networking',
      arguments: { action: 'mute_player', targetPlayerId: TARGET_PLAYER_ID, muted: false, localPlayerNum: 0, systemWide: true },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.target', equals: TARGET_PLAYER_ID, label: 'target player id returned' },
        { path: 'structuredContent.result.muted', equals: false, label: 'unmute state returned' },
        { path: 'structuredContent.result.localPlayerNum', equals: 0, label: 'local player number returned' },
        { path: 'structuredContent.result.systemWide', equals: true, label: 'system-wide mute flag returned' },
        { path: 'structuredContent.result.success', equals: true, label: 'target id mute request completed' }
      ]
    },
    {
      scenario: 'CONFIG: set_voice_attenuation',
      toolName: 'manage_networking',
      arguments: { action: 'set_voice_attenuation', attenuationRadius: 1800, attenuationFalloff: 2.5 },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.attenuationRadius', equals: 1800, label: 'voice attenuation radius returned' },
        { path: 'structuredContent.result.attenuationFalloff', equals: 2.5, label: 'voice attenuation falloff returned' }
      ]
    },
    {
      scenario: 'CONFIG: configure_push_to_talk',
      toolName: 'manage_networking',
      arguments: { action: 'configure_push_to_talk', pushToTalkEnabled: true, pushToTalkKey: 'LeftShift' },
      expected: 'success',
      assertions: [
        { path: 'structuredContent.result.pushToTalkEnabled', equals: true, label: 'push-to-talk enabled flag returned' },
        { path: 'structuredContent.result.pushToTalkKey', equals: 'LeftShift', label: 'push-to-talk key returned' }
      ]
    },

    // === CLEANUP ===
    { scenario: 'PLAYBACK: stop PIE after session tests', toolName: 'control_editor', arguments: { action: 'stop_pie' }, expected: 'success' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

// === GAME FRAMEWORK ACTIONS ===
{
  /**
   * manage_networking game framework action integration tests
   * Covers all 20 actions with live Blueprint mutations and readbacks.
   */

  const ts = Date.now();
  const TEST_FOLDER = `/Game/MCPTest/GameplayFramework_${ts}`;

  const GAME_MODE_NAME = `BP_FrameworkGameMode_${ts}`;
  const GAME_STATE_NAME = `BP_FrameworkGameState_${ts}`;
  const PLAYER_CONTROLLER_NAME = `BP_FrameworkPlayerController_${ts}`;
  const PLAYER_STATE_NAME = `BP_FrameworkPlayerState_${ts}`;
  const GAME_INSTANCE_NAME = `BP_FrameworkGameInstance_${ts}`;
  const HUD_NAME = `BP_FrameworkHUD_${ts}`;

  const GAME_MODE_ASSET_PATH = `${TEST_FOLDER}/${GAME_MODE_NAME}`;
  const GAME_MODE_OBJECT_PATH = `${GAME_MODE_ASSET_PATH}.${GAME_MODE_NAME}`;
  const GAME_STATE_ASSET_PATH = `${TEST_FOLDER}/${GAME_STATE_NAME}`;
  const GAME_STATE_OBJECT_PATH = `${GAME_STATE_ASSET_PATH}.${GAME_STATE_NAME}`;
  const PLAYER_CONTROLLER_ASSET_PATH = `${TEST_FOLDER}/${PLAYER_CONTROLLER_NAME}`;
  const PLAYER_CONTROLLER_OBJECT_PATH = `${PLAYER_CONTROLLER_ASSET_PATH}.${PLAYER_CONTROLLER_NAME}`;
  const PLAYER_STATE_ASSET_PATH = `${TEST_FOLDER}/${PLAYER_STATE_NAME}`;
  const PLAYER_STATE_OBJECT_PATH = `${PLAYER_STATE_ASSET_PATH}.${PLAYER_STATE_NAME}`;
  const GAME_INSTANCE_ASSET_PATH = `${TEST_FOLDER}/${GAME_INSTANCE_NAME}`;
  const GAME_INSTANCE_OBJECT_PATH = `${GAME_INSTANCE_ASSET_PATH}.${GAME_INSTANCE_NAME}`;
  const HUD_ASSET_PATH = `${TEST_FOLDER}/${HUD_NAME}`;
  const HUD_OBJECT_PATH = `${HUD_ASSET_PATH}.${HUD_NAME}`;

  const DEFAULT_PAWN_CLASS = '/Script/Engine.DefaultPawn';
  const PLAYER_CONTROLLER_CLASS = '/Script/Engine.PlayerController';
  const GAME_STATE_CLASS = '/Script/Engine.GameState';
  const PLAYER_STATE_CLASS = '/Script/Engine.PlayerState';
  const SPECTATOR_CLASS = '/Script/Engine.SpectatorPawn';

  const createBlueprintAssertions = (assetPath, objectPath, assetName, label) => [
    { path: 'structuredContent.result.success', equals: true, label: `${label} native success flag` },
    { path: 'structuredContent.result.blueprintPath', equals: objectPath, label: `${label} object path returned` },
    { path: 'structuredContent.result.assetPath', equals: assetPath, label: `${label} package path verified` },
    { path: 'structuredContent.result.assetName', equals: assetName, label: `${label} asset name verified` },
    { path: 'structuredContent.result.existsAfter', equals: true, label: `${label} exists after creation` },
    { path: 'structuredContent.result.assetClass', equals: 'Blueprint', label: `${label} asset class verified` }
  ];

  const gameModePathAssertion = (label) => [
    { path: 'structuredContent.result.success', equals: true, label: `${label} native success flag` },
    { path: 'structuredContent.result.blueprintPath', equals: GAME_MODE_OBJECT_PATH, label: `${label} target game mode returned` }
  ];

  testCases.push(
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },

    { scenario: 'CREATE: create_hud_class', toolName: 'manage_networking', arguments: { action: 'create_hud_class', name: HUD_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.HUD', timeoutMs: 120000 }, expected: 'success', assertions: createBlueprintAssertions(HUD_ASSET_PATH, HUD_OBJECT_PATH, HUD_NAME, 'hud blueprint') },
    { scenario: 'CREATE: create_game_mode', toolName: 'manage_networking', arguments: { action: 'create_game_mode', name: GAME_MODE_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.GameMode', defaultPawnClass: DEFAULT_PAWN_CLASS, hudClass: HUD_OBJECT_PATH, save: true }, expected: 'success', assertions: createBlueprintAssertions(GAME_MODE_ASSET_PATH, GAME_MODE_OBJECT_PATH, GAME_MODE_NAME, 'game mode blueprint') },
    { scenario: 'CREATE: create_game_state', toolName: 'manage_networking', arguments: { action: 'create_game_state', name: GAME_STATE_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.GameState' }, expected: 'success', assertions: createBlueprintAssertions(GAME_STATE_ASSET_PATH, GAME_STATE_OBJECT_PATH, GAME_STATE_NAME, 'game state blueprint') },
    { scenario: 'CREATE: create_player_controller', toolName: 'manage_networking', arguments: { action: 'create_player_controller', name: PLAYER_CONTROLLER_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.PlayerController' }, expected: 'success', assertions: createBlueprintAssertions(PLAYER_CONTROLLER_ASSET_PATH, PLAYER_CONTROLLER_OBJECT_PATH, PLAYER_CONTROLLER_NAME, 'player controller blueprint') },
    { scenario: 'CREATE: create_player_state', toolName: 'manage_networking', arguments: { action: 'create_player_state', name: PLAYER_STATE_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.PlayerState' }, expected: 'success', assertions: createBlueprintAssertions(PLAYER_STATE_ASSET_PATH, PLAYER_STATE_OBJECT_PATH, PLAYER_STATE_NAME, 'player state blueprint') },
    { scenario: 'CREATE: create_game_instance', toolName: 'manage_networking', arguments: { action: 'create_game_instance', name: GAME_INSTANCE_NAME, path: TEST_FOLDER, parentClass: '/Script/Engine.GameInstance' }, expected: 'success', assertions: createBlueprintAssertions(GAME_INSTANCE_ASSET_PATH, GAME_INSTANCE_OBJECT_PATH, GAME_INSTANCE_NAME, 'game instance blueprint') },

    { scenario: 'CONFIG: set_default_pawn_class', toolName: 'manage_networking', arguments: { action: 'set_default_pawn_class', gameModeBlueprint: GAME_MODE_OBJECT_PATH, pawnClass: DEFAULT_PAWN_CLASS }, expected: 'success', assertions: gameModePathAssertion('default pawn class') },
    { scenario: 'INFO: read back default pawn class', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: GAME_MODE_OBJECT_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.gameFrameworkInfo.defaultPawnClass', equals: DEFAULT_PAWN_CLASS, label: 'default pawn class read back from CDO' }] },
    { scenario: 'CONFIG: set_player_controller_class', toolName: 'manage_networking', arguments: { action: 'set_player_controller_class', gameModeBlueprint: GAME_MODE_OBJECT_PATH, playerControllerClass: PLAYER_CONTROLLER_CLASS }, expected: 'success', assertions: gameModePathAssertion('player controller class') },
    { scenario: 'INFO: read back player controller class', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: GAME_MODE_OBJECT_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.gameFrameworkInfo.playerControllerClass', equals: PLAYER_CONTROLLER_CLASS, label: 'player controller class read back from CDO' }] },
    { scenario: 'CONFIG: set_game_state_class', toolName: 'manage_networking', arguments: { action: 'set_game_state_class', gameModeBlueprint: GAME_MODE_OBJECT_PATH, gameStateClass: GAME_STATE_CLASS }, expected: 'success', assertions: gameModePathAssertion('game state class') },
    { scenario: 'INFO: read back game state class', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: GAME_MODE_OBJECT_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.gameFrameworkInfo.gameStateClass', equals: GAME_STATE_CLASS, label: 'game state class read back from CDO' }] },
    { scenario: 'CONFIG: set_player_state_class', toolName: 'manage_networking', arguments: { action: 'set_player_state_class', gameModeBlueprint: GAME_MODE_OBJECT_PATH, playerStateClass: PLAYER_STATE_CLASS }, expected: 'success', assertions: gameModePathAssertion('player state class') },
    { scenario: 'INFO: read back player state class', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: GAME_MODE_OBJECT_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.gameFrameworkInfo.playerStateClass', equals: PLAYER_STATE_CLASS, label: 'player state class read back from CDO' }] },
    { scenario: 'CONFIG: configure_game_rules', toolName: 'manage_networking', arguments: { action: 'configure_game_rules', gameModeBlueprint: GAME_MODE_OBJECT_PATH, bDelayedStart: true }, expected: 'success', assertions: gameModePathAssertion('game rules') },

    { scenario: 'ACTION: setup_match_states', toolName: 'manage_networking', arguments: { action: 'setup_match_states', gameModeBlueprint: GAME_MODE_OBJECT_PATH, states: ['Waiting', 'Warmup', 'InProgress', 'PostMatch'] }, expected: 'success', assertions: [...gameModePathAssertion('match states'), { path: 'structuredContent.result.variablesAdded', equals: 6, label: 'match flow variables added' }, { path: 'structuredContent.result.stateCount', equals: 4, label: 'match state count returned' }, { path: 'structuredContent.result.configuredStates', length: 4, label: 'configured match states returned' }] },
    { scenario: 'CONFIG: configure_round_system', toolName: 'manage_networking', arguments: { action: 'configure_round_system', gameModeBlueprint: GAME_MODE_OBJECT_PATH, numRounds: 3, roundTime: 180, intermissionTime: 15 }, expected: 'success', assertions: [...gameModePathAssertion('round system'), { path: 'structuredContent.result.variablesAdded', equals: 7, label: 'round system variables added' }, { path: 'structuredContent.result.configuration.numRounds', equals: 3, label: 'round count configured' }, { path: 'structuredContent.result.configuration.roundTime', equals: 180, label: 'round time configured' }, { path: 'structuredContent.result.configuration.intermissionTime', equals: 15, label: 'intermission time configured' }] },
    { scenario: 'CONFIG: configure_team_system', toolName: 'manage_networking', arguments: { action: 'configure_team_system', gameModeBlueprint: GAME_MODE_OBJECT_PATH, numTeams: 4, teamSize: 5, autoBalance: false, friendlyFire: true }, expected: 'success', assertions: [...gameModePathAssertion('team system'), { path: 'structuredContent.result.variablesAdded', equals: 6, label: 'team system variables added' }, { path: 'structuredContent.result.configuration.numTeams', equals: 4, label: 'team count configured' }, { path: 'structuredContent.result.configuration.teamSize', equals: 5, label: 'team size configured' }, { path: 'structuredContent.result.configuration.autoBalance', equals: false, label: 'auto balance configured' }, { path: 'structuredContent.result.configuration.friendlyFire', equals: true, label: 'friendly fire configured' }] },
    { scenario: 'CONFIG: configure_scoring_system', toolName: 'manage_networking', arguments: { action: 'configure_scoring_system', gameModeBlueprint: GAME_MODE_OBJECT_PATH, scorePerKill: 125, scorePerAssist: 25, scorePerObjective: 750, winScore: 1500, scorePerDeath: -10 }, expected: 'success', assertions: [...gameModePathAssertion('scoring system'), { path: 'structuredContent.result.variablesAdded', equals: 5, label: 'scoring system variables added' }, { path: 'structuredContent.result.configuration.scorePerKill', equals: 125, label: 'kill score configured' }, { path: 'structuredContent.result.configuration.scorePerAssist', equals: 25, label: 'assist score configured' }, { path: 'structuredContent.result.configuration.scorePerObjective', equals: 750, label: 'objective score configured' }, { path: 'structuredContent.result.configuration.winScore', equals: 1500, label: 'win score configured' }, { path: 'structuredContent.result.configuration.scorePerDeath', equals: -10, label: 'death score configured' }] },
    { scenario: 'CONFIG: configure_spawn_system', toolName: 'manage_networking', arguments: { action: 'configure_spawn_system', gameModeBlueprint: GAME_MODE_OBJECT_PATH, spawnSelectionMethod: 'TeamWeighted', respawnDelay: 6.5, usePlayerStarts: true, canRespawn: false, maxRespawns: 2 }, expected: 'success', assertions: [...gameModePathAssertion('spawn system'), { path: 'structuredContent.result.variablesAdded', equals: 5, label: 'spawn system variables added' }, { path: 'structuredContent.result.configuration.spawnSelectionMethod', equals: 'TeamWeighted', label: 'spawn selection configured' }, { path: 'structuredContent.result.configuration.respawnDelay', equals: 6.5, label: 'spawn respawn delay configured' }, { path: 'structuredContent.result.configuration.usePlayerStarts', equals: true, label: 'player starts enabled' }, { path: 'structuredContent.result.configuration.canRespawn', equals: false, label: 'can respawn configured' }, { path: 'structuredContent.result.configuration.maxRespawns', equals: 2, label: 'max respawns configured' }] },

    { scenario: 'CONFIG: configure_player_start', toolName: 'manage_networking', arguments: { action: 'configure_player_start', blueprintPath: GAME_MODE_OBJECT_PATH, teamIndex: 2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'player start native success flag' }, { path: 'structuredContent.result.teamIndex', equals: 2, label: 'player start team index returned' }, { path: 'structuredContent.result.playerStartTag', equals: 'Team2', label: 'player start team tag generated' }] },
    { scenario: 'CONFIG: set_respawn_rules', toolName: 'manage_networking', arguments: { action: 'set_respawn_rules', gameModeBlueprint: GAME_MODE_OBJECT_PATH, respawnDelay: 9.25, respawnLocation: 'TeamStart', forceRespawn: false, respawnLives: 3 }, expected: 'success', assertions: [...gameModePathAssertion('respawn rules'), { path: 'structuredContent.result.variablesAdded', equals: 3, label: 'respawn variables added' }, { path: 'structuredContent.result.configuration.respawnDelay', equals: 9.25, label: 'respawn delay configured' }, { path: 'structuredContent.result.configuration.respawnLocation', equals: 'TeamStart', label: 'respawn location configured' }, { path: 'structuredContent.result.configuration.forceRespawn', equals: false, label: 'force respawn configured' }, { path: 'structuredContent.result.configuration.respawnLives', equals: 3, label: 'respawn lives configured' }] },
    { scenario: 'CONFIG: configure_spectating', toolName: 'manage_networking', arguments: { action: 'configure_spectating', gameModeBlueprint: GAME_MODE_OBJECT_PATH, spectatorClass: SPECTATOR_CLASS, allowSpectating: true, spectatorViewMode: 'FreeCam' }, expected: 'success', assertions: gameModePathAssertion('spectating') },

    { scenario: 'INFO: get_game_framework_info final readback', toolName: 'manage_networking', arguments: { action: 'get_game_framework_info', gameModeBlueprint: GAME_MODE_OBJECT_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.success', equals: true, label: 'game framework info native success flag' }, { path: 'structuredContent.result.gameFrameworkInfo.gameModeClass', equals: `${GAME_MODE_OBJECT_PATH}_C`, label: 'game mode generated class read back' }, { path: 'structuredContent.result.gameFrameworkInfo.defaultPawnClass', equals: DEFAULT_PAWN_CLASS, label: 'final default pawn readback' }, { path: 'structuredContent.result.gameFrameworkInfo.playerControllerClass', equals: PLAYER_CONTROLLER_CLASS, label: 'final player controller readback' }, { path: 'structuredContent.result.gameFrameworkInfo.gameStateClass', equals: GAME_STATE_CLASS, label: 'final game state readback' }, { path: 'structuredContent.result.gameFrameworkInfo.playerStateClass', equals: PLAYER_STATE_CLASS, label: 'final player state readback' }, { path: 'structuredContent.result.gameFrameworkInfo.hudClass', equals: `${HUD_OBJECT_PATH}_C`, label: 'final HUD class readback' }] },

    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' }
  );
}

// === INPUT ACTIONS ===
{
  /**
   * manage_networking input action integration tests
   * Covers all 10 actions with proper setup/teardown sequencing.
   */

  const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
  const INPUT_FOLDER_ALIAS = 'Game/MCPTest';
  const ts = Date.now();
  const INPUT_ACTION = '/Game/MCPTest/Testinput_action';
  const INPUT_CONTEXT = '/Game/MCPTest/Testinput_mapping_context';

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create test blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_Test_${ts}`, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },

    // === CREATE ===
    { scenario: 'CREATE: create_input_action', toolName: 'manage_networking', arguments: {"action": "create_input_action", "name": "Testinput_action", "path": INPUT_FOLDER_ALIAS}, expected: 'success|already exists' },
    { scenario: 'CREATE: create_input_mapping_context', toolName: 'manage_networking', arguments: {"action": "create_input_mapping_context", "name": "Testinput_mapping_context", "path": INPUT_FOLDER_ALIAS}, expected: 'success|already exists' },
    // === ADD ===
    { scenario: 'ADD: add_mapping', toolName: 'manage_networking', arguments: { action: 'add_mapping', contextPath: INPUT_CONTEXT, actionPath: INPUT_ACTION, key: 'SpaceBar' }, expected: 'success|already exists' },
    // === ACTION ===
    { scenario: 'ACTION: map_input_action', toolName: 'manage_networking', arguments: { action: 'map_input_action', contextPath: INPUT_CONTEXT, actionPath: INPUT_ACTION, key: 'LeftMouseButton' }, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_input_trigger', toolName: 'manage_networking', arguments: { action: 'set_input_trigger', actionPath: INPUT_ACTION, triggerType: 'Pressed' }, expected: 'success' },
    { scenario: 'CONFIG: set_input_modifier', toolName: 'manage_networking', arguments: { action: 'set_input_modifier', contextPath: INPUT_CONTEXT, actionPath: INPUT_ACTION, key: 'SpaceBar', modifierType: 'Negate' }, expected: 'success' },
    // === DELETE ===
    { scenario: 'DELETE: remove_mapping', toolName: 'manage_networking', arguments: { action: 'remove_mapping', contextPath: INPUT_CONTEXT, actionPath: INPUT_ACTION, key: 'SpaceBar' }, expected: 'success|not found' },
    // === TOGGLE ===
    { scenario: 'TOGGLE: enable_input_mapping', toolName: 'manage_networking', arguments: { action: 'enable_input_mapping', contextPath: INPUT_CONTEXT, priority: 1 }, expected: 'success' },
    { scenario: 'TOGGLE: disable_input_action', toolName: 'manage_networking', arguments: { action: 'disable_input_action', actionPath: INPUT_ACTION }, expected: 'success' },
    // === INFO ===
    { scenario: 'INFO: get_input_info', toolName: 'manage_networking', arguments: { action: 'get_input_info', assetPath: INPUT_CONTEXT }, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test blueprint', toolName: 'manage_asset', arguments: { action: 'delete', path: `${TEST_FOLDER}/BP_Test_${ts}`, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'DELETE_FAILED|success|not found' },
  );
}

runToolTests('manage-networking', testCases);
