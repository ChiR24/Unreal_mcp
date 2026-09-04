/**
 * manage_ai records, shard 4 of 4: rebuild / remove / run / set_* / setup / stop.
 *
 * run_behavior_tree and stop_behavior_tree are AUTHORING actions, not runtime
 * ones: the native handlers add or remove a Blueprint variable on the
 * AIController ASSET (ControllerFocus.cpp) and never touch GEditor->PlayWorld,
 * so they declare editorState 'edit' like every other manage_ai action.
 * set_focus / clear_focus and the Blackboard value actions are asset edits for
 * the same reason.
 */
import type { CapabilityRecordSource } from '../../../index.js';
import { BT, aiRecord } from './builder.js';
import { NAV } from './properties-navigation.js';
import { AI } from './properties.js';

const A = AI;
const N = NAV;

const PROMOTED = 'Promoted from a hidden native manage_ai route after the gateway migration.';
const POST = 'post-migration' as const;
const OUT_STR = (d: string) => ({ type: 'string', description: d });
const OUT_NUM = (d: string) => ({ type: 'number', description: d });
const OUT_BOOL = (d: string) => ({ type: 'boolean', description: d });
const OUT_STR_ARRAY = (d: string) => ({ type: 'array', items: { type: 'string' }, description: d });

export const AI_SET_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'rebuild_navigation', summary: 'Rebuild navigation for the loaded editor world.',
    use: 'NavMesh geometry is stale after level or settings edits.',
    avoid: 'Use configure_nav_mesh_settings to change generation parameters.',
    props: { action: A.action },
    example: {}, result: 'Navigation rebuilt',
  }),
  aiRecord({
    action: 'remove_node', summary: 'Remove a Behavior Tree graph node.',
    use: 'A graph node must be deleted.',
    avoid: 'Use break_connections to keep the node but detach it.',
    props: { action: A.action, assetPath: A.assetPath, nodeId: A.nodeId },
    required: ['assetPath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_1' }, result: 'Node removed',
  }),
  aiRecord({
    action: 'run_behavior_tree', summary: 'Author the run-on-start Behavior Tree of an AIController asset.',
    use: 'A controller asset should start a Behavior Tree when it possesses a pawn.',
    avoid: 'Use control_editor to start a PIE session; this edits the asset only.',
    props: { action: A.action, controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath },
    required: ['controllerPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { controllerPath: '/Game/AI/AIC_Enemy', behaviorTreePath: '/Game/AI/BT_Enemy' },
    result: 'Behavior Tree run configured',
  }),
  aiRecord({
    action: 'set_blackboard_value', summary: 'Set a Blackboard key value on a Blackboard asset.',
    use: 'A Blackboard key needs a different authored default.',
    avoid: 'Use add_blackboard_key to create the key.',
    props: {
      action: A.action, blackboardPath: A.blackboardPath,
      keyName: A.keyName, value: A.value,
    },
    required: ['blackboardPath', 'keyName'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'TargetActor', value: 'true' },
    result: 'Blackboard value set',
  }),
  aiRecord({
    action: 'set_focus', summary: 'Author the focus target on an AIController asset.',
    use: 'A controller asset should focus a named actor.',
    avoid: 'Use clear_focus to remove the target.',
    props: { action: A.action, controllerPath: A.controllerPath, focusActorName: A.focusActorName },
    required: ['controllerPath'],
    out: { assetPath: A.assetPath },
    example: { controllerPath: '/Game/AI/AIC_Enemy', focusActorName: 'PlayerStart' },
    result: 'Focus set',
  }),
  aiRecord({
    action: 'set_key_instance_synced', summary: 'Set Blackboard key instance synchronization.',
    use: 'A Blackboard key must share its value across instances.',
    avoid: 'Use add_blackboard_key to create the key.',
    props: {
      action: A.action, blackboardPath: A.blackboardPath,
      keyName: A.keyName, isInstanceSynced: A.isInstanceSynced,
    },
    required: ['blackboardPath', 'keyName'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'TargetActor', isInstanceSynced: true },
    result: 'Key sync set',
  }),
  aiRecord({
    action: 'set_nav_agent_properties', summary: 'Set project navigation agent properties.',
    use: 'The navigation agent dimensions must change.',
    avoid: 'Use configure_nav_mesh_settings for generation tuning.',
    props: {
      action: A.action, agentRadius: N.agentRadius, agentHeight: N.agentHeight,
      agentStepHeight: N.agentStepHeight, agentMaxSlope: N.agentMaxSlope,
    },
    example: { agentRadius: 35, agentHeight: 144 }, result: 'Nav agent properties set',
  }),
  aiRecord({
    action: 'set_nav_area_class', summary: 'Set the navigation area class on a placed actor that already carries a NavModifierComponent (add one with create_nav_modifier first).',
    use: 'A placed actor should carve a specific navigation area.',
    avoid: 'Use configure_nav_area_cost to change the area cost itself.',
    props: { action: A.action, actorName: A.actorName, areaClass: N.areaClass },
    required: ['actorName', 'areaClass'],
    out: { actorName: A.actorName },
    example: { actorName: 'NavTestActor', areaClass: '/Script/NavigationSystem.NavArea_Obstacle' },
    result: 'Nav area class set',
  }),
  aiRecord({
    action: 'set_nav_link_type', summary: 'Set an authored navigation link type.',
    use: 'A link must switch between the simple and smart form.',
    avoid: 'Use configure_smart_link_behavior to tune a smart link.',
    props: { action: A.action, actorName: A.actorName, linkType: N.linkType },
    required: ['actorName'],
    out: { actorName: A.actorName },
    example: { actorName: 'NavLinkProxy_0', linkType: 'smart' }, result: 'Nav link type set',
  }),
  aiRecord({
    action: 'set_node_properties', summary: 'Set Behavior Tree graph node properties.',
    use: 'A graph node needs property or comment edits.',
    avoid: 'Use configure_bt_node for the asset-level route.',
    props: {
      action: A.action, assetPath: A.assetPath, nodeId: A.nodeId,
      comment: A.comment, properties: A.properties,
    },
    required: ['assetPath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_1', properties: { WaitTime: 1 } },
    result: 'Node properties set',
  }),
  aiRecord({
    action: 'set_perception_team', summary: 'Set perception team data on a Blueprint asset.',
    use: 'An AI must belong to a specific perception team.',
    avoid: 'Use configure_sight_config for sense tuning.',
    props: { action: A.action, blueprintPath: A.blueprintPath, teamId: A.teamId },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', teamId: 2 }, result: 'Perception team set',
  }),
  aiRecord({
    action: 'setup_perception', summary: 'Set up AI perception on a Blueprint asset.',
    use: 'A Blueprint needs perception added and several senses tuned at once.',
    avoid: 'Use the individual configure_*_config actions for one sense.',
    props: {
      action: A.action, blueprintPath: A.blueprintPath,
      enableSight: A.enableSight, enableHearing: A.enableHearing, enableDamage: A.enableDamage,
      sightRadius: A.sightRadius, loseSightRadius: A.loseSightRadius,
      peripheralVisionAngle: A.peripheralVisionAngle, hearingRange: A.hearingRange,
      dominantSense: A.dominantSense,
    },
    required: ['blueprintPath'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', enableSight: true, dominantSense: 'Sight' },
    result: 'Perception set up',
  }),
  aiRecord({
    action: 'stop_behavior_tree', summary: 'Clear the run-on-start Behavior Tree of an AIController asset.',
    use: 'A controller asset should no longer start a Behavior Tree.',
    avoid: 'Use control_editor to stop a PIE session; this edits the asset only.',
    props: { action: A.action, controllerPath: A.controllerPath },
    required: ['controllerPath'], plugins: BT,
    out: { assetPath: A.assetPath },
      example: { controllerPath: '/Game/AI/AIC_Enemy' }, result: 'Behavior Tree run cleared',
    }),
    aiRecord({
      action: 'set_ai_perception',
      summary: 'Configure the sight and hearing senses of an AIController perception component.',
      use: 'One call should add the perception component and set its senses together.',
      avoid: 'Only one sense needs tuning; configure_sight_config and configure_hearing_config edit them separately.',
      props: {
        action: A.action, controllerPath: A.controllerPath,
        enableSight: A.enableSight, sightRadius: A.sightRadius, loseSightRadius: A.loseSightRadius,
        peripheralVisionAngle: A.peripheralVisionAngle, enableHearing: A.enableHearing,
      },
      required: ['controllerPath'],
      out: {
        controllerPath: A.controllerPath,
        createdNew: OUT_BOOL('Whether the perception component was added by this call.'),
        sensesConfigured: OUT_STR_ARRAY('Names of the senses this call configured.'),
        dominantSense: OUT_STR('Sense the component treats as dominant.'),
      },
      example: { controllerPath: '/Game/AI/AIC_Enemy', enableSight: true, sightRadius: 2500, peripheralVisionAngle: 75 },
      result: 'AI perception configured',
      provenance: POST, rationale: PROMOTED,
    }),
    aiRecord({
      action: 'set_ai_movement',
      summary: 'Set the movement limits on the CharacterMovement component of a Blueprint.',
      use: 'Walk speed, acceleration, braking or rotation rate must change on a pawn asset.',
      avoid: 'The pawn has no CharacterMovement component; the call reports the missing component instead.',
      props: {
        action: A.action, blueprintPath: A.blueprintPath,
        maxWalkSpeed: A.maxWalkSpeed, maxAcceleration: A.maxAcceleration,
        brakingDeceleration: A.brakingDeceleration, rotationRate: A.rotationRate,
      },
      required: ['blueprintPath'],
      out: {
        blueprintPath: A.blueprintPath,
        propertiesSet: OUT_STR_ARRAY('Names of the movement properties this call changed.'),
        propertyCount: OUT_NUM('How many movement properties were changed.'),
        maxWalkSpeed: A.maxWalkSpeed, maxAcceleration: A.maxAcceleration,
        rotationRateYaw: OUT_NUM('Yaw component of the resulting rotation rate.'),
        orientRotationToMovement: OUT_BOOL('Whether the component orients rotation to movement.'),
        useRVOAvoidance: OUT_BOOL('Whether RVO avoidance is enabled on the component.'),
      },
      example: { blueprintPath: '/Game/AI/BP_Enemy', maxWalkSpeed: 450, rotationRate: 360 },
      result: 'AI movement configured',
      provenance: POST, rationale: PROMOTED,
    }),
  ];
