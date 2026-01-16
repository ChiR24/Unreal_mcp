/**
 * Networking & Multiplayer Handlers (Phase 20)
 *
 * Complete networking and replication system including:
 * - Replication (property replication, conditions, net update frequency, dormancy)
 * - RPCs (Server, Client, NetMulticast functions with validation)
 * - Authority & Ownership (owner, autonomous proxy, authority checks)
 * - Network Relevancy (cull distance, always relevant, only relevant to owner)
 * - Net Serialization (custom serialization, struct replication)
 * - Network Prediction (client-side prediction, server reconciliation)
 * - Utility (info queries)
 *
 * @module networking-handlers
 */

import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { requireNonEmptyString, executeAutomationRequest } from './common-handlers.js';

function getTimeoutMs(): number {
  const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
  return Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
}

/**
 * Handles all networking actions for the manage_networking tool.
 */
export async function handleNetworkingTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  const argsRecord = args as Record<string, unknown>;
  const timeoutMs = getTimeoutMs();

  // All actions are dispatched to C++ via automation bridge
  const sendRequest = async (subAction: string): Promise<HandlerResult> => {
    const payload = { ...argsRecord, subAction };
    const result = await executeAutomationRequest(
      tools,
      'manage_networking',
      payload as HandlerArgs,
      `Automation bridge not available for networking action: ${subAction}`,
      { timeoutMs }
    );
    return cleanObject(result) as HandlerResult;
  };

  switch (action) {
    // =========================================================================
    // 20.1 Replication (6 actions)
    // =========================================================================

    case 'set_property_replicated': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.propertyName, 'propertyName', 'Missing required parameter: propertyName');
      // Sets a property to be replicated
      // Optional: replicated (bool, default true), replicationCondition (enum)
      return sendRequest('set_property_replicated');
    }

    case 'set_replication_condition': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.propertyName, 'propertyName', 'Missing required parameter: propertyName');
      requireNonEmptyString(argsRecord.condition, 'condition', 'Missing required parameter: condition');
      // Sets replication condition (COND_None, COND_OwnerOnly, COND_SkipOwner, COND_SimulatedOnly, etc.)
      return sendRequest('set_replication_condition');
    }

    case 'configure_net_update_frequency': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures net update frequency and min net update frequency
      // Optional: netUpdateFrequency (float), minNetUpdateFrequency (float)
      return sendRequest('configure_net_update_frequency');
    }

    case 'configure_net_priority': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Sets net priority for bandwidth allocation
      // Optional: netPriority (float, default 1.0)
      return sendRequest('configure_net_priority');
    }

    case 'set_net_dormancy': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.dormancy, 'dormancy', 'Missing required parameter: dormancy');
      // Sets net dormancy mode (DORM_Never, DORM_Awake, DORM_DormantAll, DORM_DormantPartial, DORM_Initial)
      return sendRequest('set_net_dormancy');
    }

    case 'configure_replication_graph': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures replication graph settings
      // Optional: nodeClass, spatialBias, defaultSettingsClass
      return sendRequest('configure_replication_graph');
    }

    // =========================================================================
    // 20.2 RPCs (3 actions)
    // =========================================================================

    case 'create_rpc_function': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.functionName, 'functionName', 'Missing required parameter: functionName');
      requireNonEmptyString(argsRecord.rpcType, 'rpcType', 'Missing required parameter: rpcType');
      // Creates an RPC function (Server, Client, NetMulticast)
      // Optional: reliable (bool), parameters (array), returnType
      return sendRequest('create_rpc_function');
    }

    case 'configure_rpc_validation': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.functionName, 'functionName', 'Missing required parameter: functionName');
      // Configures RPC validation function
      // Optional: validationFunctionName, withValidation (bool)
      return sendRequest('configure_rpc_validation');
    }

    case 'set_rpc_reliability': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.functionName, 'functionName', 'Missing required parameter: functionName');
      // Sets RPC reliability (reliable/unreliable)
      // Required: reliable (bool)
      return sendRequest('set_rpc_reliability');
    }

    // =========================================================================
    // 20.3 Authority & Ownership (4 actions)
    // =========================================================================

    case 'set_owner': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Sets the owner of an actor at runtime
      // Optional: ownerActorName (string, null to clear owner)
      return sendRequest('set_owner');
    }

    case 'set_autonomous_proxy': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures autonomous proxy role for player-controlled actors
      // Optional: isAutonomousProxy (bool)
      return sendRequest('set_autonomous_proxy');
    }

    case 'check_has_authority': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Checks if local machine has authority over the actor
      // Returns: hasAuthority (bool), role (string)
      return sendRequest('check_has_authority');
    }

    case 'check_is_locally_controlled': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Checks if actor is locally controlled (for Pawns/Characters)
      // Returns: isLocallyControlled (bool), isLocalController (bool)
      return sendRequest('check_is_locally_controlled');
    }

    // =========================================================================
    // 20.4 Network Relevancy (3 actions)
    // =========================================================================

    case 'configure_net_cull_distance': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures network culling distance
      // Optional: netCullDistanceSquared (float), useOwnerNetRelevancy (bool)
      return sendRequest('configure_net_cull_distance');
    }

    case 'set_always_relevant': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Sets actor to always be relevant to all clients
      // Optional: alwaysRelevant (bool, default true)
      return sendRequest('set_always_relevant');
    }

    case 'set_only_relevant_to_owner': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Sets actor to only be relevant to its owner
      // Optional: onlyRelevantToOwner (bool, default true)
      return sendRequest('set_only_relevant_to_owner');
    }

    // =========================================================================
    // 20.5 Net Serialization (3 actions)
    // =========================================================================

    case 'configure_net_serialization': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures custom net serialization for a struct
      // Optional: structName, useNetSerialize (bool)
      return sendRequest('configure_net_serialization');
    }

    case 'set_replicated_using': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.propertyName, 'propertyName', 'Missing required parameter: propertyName');
      requireNonEmptyString(argsRecord.repNotifyFunc, 'repNotifyFunc', 'Missing required parameter: repNotifyFunc');
      // Sets the ReplicatedUsing notify function for a property
      return sendRequest('set_replicated_using');
    }

    case 'configure_push_model': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures push-model replication for properties
      // Optional: usePushModel (bool), propertyNames (array)
      return sendRequest('configure_push_model');
    }

    // =========================================================================
    // 20.6 Network Prediction (4 actions)
    // =========================================================================

    case 'configure_client_prediction': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures client-side prediction settings
      // Optional: enablePrediction (bool), predictionKey (string)
      return sendRequest('configure_client_prediction');
    }

    case 'configure_server_correction': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures server correction/reconciliation
      // Optional: correctionThreshold (float), smoothingRate (float)
      return sendRequest('configure_server_correction');
    }

    case 'add_network_prediction_data': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.dataType, 'dataType', 'Missing required parameter: dataType');
      // Adds network prediction data structure
      // Optional: properties (array of predicted properties)
      return sendRequest('add_network_prediction_data');
    }

    case 'configure_movement_prediction': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures movement component network prediction
      // Optional: networkSmoothingMode, networkMaxSmoothUpdateDistance, networkNoSmoothUpdateDistance
      return sendRequest('configure_movement_prediction');
    }

    // =========================================================================
    // 20.7 Connection & Session (3 actions)
    // =========================================================================

    case 'configure_net_driver': {
      // Configures net driver settings
      // Optional: maxClientRate, maxInternetClientRate, netServerMaxTickRate
      return sendRequest('configure_net_driver');
    }

    case 'set_net_role': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      requireNonEmptyString(argsRecord.role, 'role', 'Missing required parameter: role');
      // Sets the initial net role (ROLE_Authority, ROLE_AutonomousProxy, ROLE_SimulatedProxy, ROLE_None)
      return sendRequest('set_net_role');
    }

    case 'configure_replicated_movement': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configures replicated movement settings
      // Optional: replicateMovement (bool), replicatedMovementMode, locationQuantizationLevel
      return sendRequest('configure_replicated_movement');
    }

    // =========================================================================
    // 20.8 Utility (1 action)
    // =========================================================================

    case 'get_networking_info': {
      // Returns networking info for a blueprint or runtime actor
      // Optional: blueprintPath OR actorName
      return sendRequest('get_networking_info');
    }

    // =========================================================================
    // Wave 3.31-3.40: Networking System Actions
    // =========================================================================

    case 'debug_replication_graph': {
      // Get replication graph debug info
      // Optional: showConnections, showActorList
      return sendRequest('debug_replication_graph');
    }

    case 'configure_net_relevancy': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Configure actor net relevancy
      // Optional: relevancyRadius, relevancyMode
      return sendRequest('configure_net_relevancy');
    }

    case 'get_rpc_statistics': {
      // Get RPC call statistics
      // Optional: includeRpcDetails, resetStats
      return sendRequest('get_rpc_statistics');
    }

    case 'configure_prediction_settings': {
      requireNonEmptyString(argsRecord.blueprintPath, 'blueprintPath', 'Missing required parameter: blueprintPath');
      // Configure network prediction settings
      // Optional: predictionEnabled, predictionAmount, interpolationEnabled
      return sendRequest('configure_prediction_settings');
    }

    case 'simulate_network_conditions': {
      // Simulate network latency, packet loss, jitter
      // Optional: latencyMs, packetLoss, jitterMs, bandwidthLimit
      return sendRequest('simulate_network_conditions');
    }

    case 'get_session_players': {
      // Get all players in current session
      // Optional: includeInactive
      return sendRequest('get_session_players');
    }

    case 'configure_team_settings': {
      // Configure team assignment for actor or player
      // Optional: teamId, teamName, autoBalance, actorName
      return sendRequest('configure_team_settings');
    }

    case 'send_server_rpc': {
      requireNonEmptyString(argsRecord.rpcName, 'rpcName', 'Missing required parameter: rpcName');
      // Send custom server RPC
      // Optional: targetActor, rpcParameters
      return sendRequest('send_server_rpc');
    }

    case 'get_net_role_info': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Get network role information for actor
      return sendRequest('get_net_role_info');
    }

    case 'configure_dormancy': {
      requireNonEmptyString(argsRecord.actorName, 'actorName', 'Missing required parameter: actorName');
      // Configure actor dormancy settings
      // Optional: dormancyMode, flushDormancy
      return sendRequest('configure_dormancy');
    }

    // =========================================================================
    // Default: Unknown action
    // =========================================================================

    default:
      return cleanObject({
        success: false,
        error: 'UNKNOWN_ACTION',
        message: `Unknown manage_networking action: ${action}`
      });
  }
}
