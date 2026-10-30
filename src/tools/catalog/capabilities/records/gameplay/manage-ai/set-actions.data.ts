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
import type { CapabilityRecordSource } from '../../../model.js';
import { BT, aiRecord } from './builder.js';
import { NAV } from './properties-navigation.js';
import { AI } from './properties.js';

const A = AI;

// What the controller actions answer: McpBlueprintBehaviour::Author's report (McpAIControllerFocus.cpp).
export const CONTROLLER_LOGIC_OUT = {
  controllerPath: { type: 'string', description: 'The AIController Blueprint edited.' },
  compiled: { type: 'boolean', description: 'Whether the Blueprint compiled with the logic.' },
  saved: { type: 'boolean', description: 'Whether the Blueprint was saved.' },
  rolledBack: { type: 'boolean', description: 'A step failed and the Blueprint was put back as it was.' },
  previousRemoved: { type: 'number', description: 'Nodes an earlier run of the same action had made, taken out first.' },
  variables: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Variables the logic uses: {name, status added, redefaulted or kept}.' },
  hooks: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'The events the logic hangs on (On Possess, Tick), each made when the Blueprint lacked it.' },
} as const;
const TREE_OUT = {
  ...CONTROLLER_LOGIC_OUT,
  behaviorTreePath: { type: 'string', description: 'The Behavior Tree, by object path.' },
  variable: { type: 'string', description: 'The BehaviorTree variable that holds it: one the controller declared, else DefaultBehaviorTree.' },
} as const;
export const REMOVED_OUT = {
  ...CONTROLLER_LOGIC_OUT,
  removed: { type: 'boolean', description: 'Whether there was logic to take out; false changes nothing.' },
} as const;
const N = NAV;

const OUT_STR = (d: string) => ({ type: 'string', description: d });
const OUT_NUM = (d: string) => ({ type: 'number', description: d });
const OUT_BOOL = (d: string) => ({ type: 'boolean', description: d });
const OUT_STR_ARRAY = (d: string) => ({ type: 'array', items: { type: 'string' }, description: d });

export const AI_SET_RECORDS: readonly CapabilityRecordSource[] = [
  aiRecord({
    action: 'rebuild_navigation', summary: 'Rebuild navigation for the loaded editor world.',
    use: 'NavMesh geometry is stale after level or settings edits.',
    avoid: 'Use configure_nav_mesh_settings to change generation parameters.',
    props: {},
    example: {}, result: 'Navigation rebuilt',
  }),
  aiRecord({
    action: 'remove_node', summary: 'Remove a Behavior Tree graph node.',
    use: 'A graph node must be deleted.',
    avoid: 'Use break_connections to keep the node but detach it.',
    props: { assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath, nodeId: A.nodeId },
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_1' }, result: 'Node removed',
  }),
  aiRecord({
    action: 'run_behavior_tree', summary: 'Make an AIController Blueprint run a Behavior Tree whenever it possesses a pawn: its On Possess event calls Run Behavior Tree (which also starts the Blackboard) with its BehaviorTree variable, set to behaviorTreePath as assign_behavior_tree does. Engine nodes in the Blueprint, compiled and saved, so it works in a packaged game, for every pawn whose AI Controller Class is this one and whose Auto Possess AI covers how it enters the level. Re-running replaces it; stop_behavior_tree takes it out. NOT_AI_CONTROLLER for another Blueprint, NOT_FOUND for a missing tree, and refused during Play.',
    use: 'A controller should start a Behavior Tree whenever it possesses a pawn.',
    avoid: 'Use control_editor to start a PIE session; this edits the asset only.',
    props: { controllerPath: A.controllerPath, behaviorTreePath: A.behaviorTreePath },
    required: ['controllerPath', 'behaviorTreePath'], plugins: BT,
    out: TREE_OUT,
    example: { controllerPath: '/Game/AI/AIC_Enemy', behaviorTreePath: '/Game/AI/BT_Enemy' },
    result: 'Behavior Tree run configured',
  }),
  aiRecord({
    action: 'set_blackboard_value', summary: 'Set the default of a Blackboard key, the value it holds when an AI starts with the asset, and save it: UE 5.4+ for Bool, Int, Float, Vector, Rotator, Name and String keys, 5.5+ for Enum, Object and Class keys. value takes the kind the key holds: true or false, a whole number, a number, {x, y, z}, {pitch, yaw, roll}, text, an enumerator by name or value, an asset path for an Object key (placed actors are not assets; None clears it) or a class path for a Class key. A value of the wrong kind or outside the base class of the key is refused, as is a key with no default on this engine (NOT_SUPPORTED) or one a parent Blackboard holds (KEY_IN_PARENT); value in the reply is what the key now holds.',
    use: 'A Blackboard key needs a different authored default, such as a speed or a starting state.',
    avoid: 'Use add_blackboard_key to create the key; a value an AI writes while it runs comes from its Behavior Tree or Blueprint.',
    props: {
      blackboardPath: A.blackboardPath,
      keyName: A.keyName,
      value: { description: 'The default, in the kind the key holds: a boolean, a number, {x, y, z}, {pitch, yaw, roll}, text, an enumerator, or an asset or class path.' },
    },
    required: ['blackboardPath', 'keyName', 'value'], plugins: BT,
    out: { assetPath: A.assetPath, value: { type: 'string', description: 'The default the key now holds, as the engine writes it.' } },
    example: { blackboardPath: '/Game/AI/BB_Enemy', keyName: 'PatrolSpeed', value: 300 },
    result: 'Blackboard value set',
  }),
  aiRecord({
    action: 'set_focus', summary: 'Make an AIController Blueprint keep its focus on a target: the player 0 pawn (focusPlayer) or the first actor carrying focusTag. Each frame it has no focus actor, its Tick finds the target and calls Set Focus, so a target that spawns after the AI, or respawns, is picked up again; a pawn that uses its controller rotation turns to face it. Engine nodes in the Blueprint, compiled and saved; re-running replaces the target and clear_focus takes it out. Exactly one of focusPlayer and focusTag (INVALID_ARGUMENT); NOT_AI_CONTROLLER for another Blueprint.',
    use: 'An AI should face and track the player, or an actor marked with a tag.',
    avoid: 'Use clear_focus to take the focus logic out.',
    props: { controllerPath: A.controllerPath, focusPlayer: A.focusPlayer, focusTag: A.focusTag },
    required: ['controllerPath'], requiredOneOf: ['focusPlayer', 'focusTag'],
    out: { ...CONTROLLER_LOGIC_OUT, focusTarget: { type: 'string', description: 'What it focuses: player, or tag and the tag.' } },
    example: { controllerPath: '/Game/AI/AIC_Enemy', focusPlayer: true },
    result: 'Focus written',
  }),
  aiRecord({
    action: 'set_key_instance_synced', summary: 'Mark a Blackboard key instance-synced (isInstanceSynced, default true) or not, and save the asset. A synced key holds one value shared by every Blackboard component using the asset, so a value one AI sets reaches all of them. A key already set that way changes nothing (changed false), and a key a parent Blackboard holds is refused (KEY_IN_PARENT), naming the parent.',
    use: 'A Blackboard key must share its value across instances.',
    avoid: 'Use add_blackboard_key to create the key.',
    props: {
      blackboardPath: A.blackboardPath,
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
      agentRadius: N.agentRadius, agentHeight: N.agentHeight,
      agentStepHeight: N.agentStepHeight, agentMaxSlope: N.agentMaxSlope,
    },
    example: { agentRadius: 35, agentHeight: 144 }, result: 'Nav agent properties set',
  }),
  aiRecord({
    action: 'set_nav_area_class', summary: 'Set the navigation area class on a placed actor that already carries a NavModifierComponent (add one with create_nav_modifier first).',
    use: 'A placed actor should carve a specific navigation area.',
    avoid: 'Use configure_nav_area_cost to change the area cost itself.',
    props: {
      actorName: A.actorName, areaClass: N.areaClass,
      componentName: { type: 'string', description: 'NavModifierComponent to retarget when the actor carries several; defaults to the first one.' },
    },
    required: ['actorName', 'areaClass'],
    out: { actorName: A.actorName },
    example: { actorName: 'NavTestActor', areaClass: '/Script/NavigationSystem.NavArea_Obstacle' },
    result: 'Nav area class set',
  }),
  aiRecord({
    action: 'set_nav_link_type', summary: 'Switch a placed NavLinkProxy (actorName) between a simple link, its point link, and a smart link, its smart link alone (linkType simple or smart), carrying the endpoints and direction over. A proxy already of that type changes nothing (changed false). One undo step; the level is left unsaved.',
    use: 'A link must switch between the simple and smart form.',
    avoid: 'Use configure_smart_link_behavior to tune a smart link.',
    props: { actorName: A.actorName, linkType: N.linkType },
    required: ['actorName', 'linkType'],
    out: { actorName: A.actorName },
    example: { actorName: 'NavLinkProxy_0', linkType: 'smart' }, result: 'Nav link type set',
  }),
  aiRecord({
    action: 'set_node_properties', summary: 'Set a Behavior Tree graph node\'s comment and its node\'s properties (numbers, booleans, text, names, a Blackboard key selector by key name), in one undo step; a call that sets nothing is refused (PROPERTY_NOT_FOUND).',
    use: 'A graph node needs property or comment edits.',
    avoid: 'Use configure_bt_node for the asset-level route.',
    props: {
      assetPath: A.assetPath, behaviorTreePath: A.behaviorTreePath, nodeId: A.nodeId,
      comment: A.comment, properties: A.properties,
    },
    requiredOneOf: ['assetPath', 'behaviorTreePath'], plugins: BT,
    out: { assetPath: A.assetPath },
    example: { assetPath: '/Game/AI/BT_Enemy', nodeId: 'Node_1', properties: { WaitTime: 1 } },
    result: 'Node properties set',
  }),
  aiRecord({
    action: 'set_perception_team', summary: 'Store a team id (teamId 0 to 254, 255 meaning none) in a GenericTeamId integer variable on a Blueprint (added when missing) and save. It does not change the team the engine sees: an AI controller takes a team only in C++ (SetGenericTeamId is not exposed to Blueprints), so appliedToGenericTeamAgent is false, and an actor with no team counts as neutral to every AI. Read the variable from the project logic.',
    use: 'A team id should be stored on the AI for its own logic to read.',
    avoid: 'Use configure_sight_config for sense tuning.',
    props: { blueprintPath: A.blueprintPath, teamId: A.teamId },
    required: ['blueprintPath', 'teamId'],
    out: { blueprintPath: A.blueprintPath },
    example: { blueprintPath: '/Game/AI/BP_Enemy', teamId: 2 }, result: 'Perception team set',
  }),
  aiRecord({
    action: 'setup_perception', summary: 'Add or update the AIPerception component of an AI controller or pawn Blueprint and save: enableSight, enableHearing and enableDamage set up or update each sense (sightRadius, loseSightRadius, peripheralVisionAngle, hearingRange), and dominantSense (Sight, Hearing, Damage, Touch or None) picks the sense whose stimulus location wins. A sense already set up keeps what this call does not send; a call that enables nothing and names no dominant sense is refused. createdNew says whether the component was added.',
    use: 'A Blueprint needs perception added and several senses tuned at once.',
    avoid: 'Use the individual configure_*_config actions for one sense.',
    props: {
      blueprintPath: A.blueprintPath,
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
    action: 'stop_behavior_tree', summary: 'Take out the Run Behavior Tree logic run_behavior_tree wrote into an AIController Blueprint, so possessing a pawn no longer starts a tree (its BehaviorTree variable and On Possess event stay), compiled and saved; removed false, and nothing changed, when there was none.',
    use: 'A controller should no longer start a Behavior Tree when it possesses a pawn.',
    avoid: 'Use control_editor to stop a PIE session; this edits the asset only.',
    props: { controllerPath: A.controllerPath },
    required: ['controllerPath'], plugins: BT,
    out: REMOVED_OUT,
      example: { controllerPath: '/Game/AI/AIC_Enemy' }, result: 'Behavior Tree run cleared',
    }),
    aiRecord({
      action: 'set_ai_perception',
      summary: 'Add or update the AIPerception component of an AI controller Blueprint (controllerPath) and save, as setup_perception does: enableSight, enableHearing and enableDamage set up or update each sense (sightRadius, loseSightRadius, peripheralVisionAngle, hearingRange) and dominantSense picks the leading one. A sense already set up keeps what is not sent; a call that enables nothing and names no dominant sense is refused.',
      use: 'One call should add the perception component and set its senses together.',
      avoid: 'Only one sense needs tuning; configure_sight_config and configure_hearing_config edit them separately.',
      props: {
        controllerPath: A.controllerPath,
        enableSight: A.enableSight, sightRadius: A.sightRadius, loseSightRadius: A.loseSightRadius,
        peripheralVisionAngle: A.peripheralVisionAngle, enableHearing: A.enableHearing,
        hearingRange: A.hearingRange, enableDamage: A.enableDamage, dominantSense: A.dominantSense,
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
    }),
    aiRecord({
      action: 'set_ai_movement',
      summary: 'Set the movement limits on the CharacterMovement component of a Character Blueprint (an AI pawn) and save it: maxWalkSpeed, maxAcceleration, brakingDeceleration (walking), rotationRate (yaw, degrees per second), maxFlySpeed, jumpZVelocity and avoidanceWeight, plus useAccelerationForPaths (path following speeds up and slows down like a player instead of moving at full speed at once), orientRotationToMovement (turn to face where it goes) and useRVOAvoidance (steer around other agents). Only what is sent changes, 0 included; a negative number or a call that sets nothing is refused, as is a Blueprint with no CharacterMovement component (COMPONENT_NOT_FOUND). propertiesSet names what changed and currentValues reads the main values back.',
      use: 'Walk speed, acceleration, braking or rotation rate must change on a pawn asset.',
      avoid: 'The pawn has no CharacterMovement component; the call reports the missing component instead.',
      props: {
        blueprintPath: A.blueprintPath,
        maxWalkSpeed: A.maxWalkSpeed, maxAcceleration: A.maxAcceleration,
        brakingDeceleration: A.brakingDeceleration, rotationRate: A.rotationRate,
        useAccelerationForPaths: { type: 'boolean', description: 'Path following uses acceleration instead of setting velocity directly; left unchanged when omitted.' },
        orientRotationToMovement: { type: 'boolean', description: 'Rotate the pawn toward its movement direction; left unchanged when omitted.' },
        useRVOAvoidance: { type: 'boolean', description: 'Enable RVO avoidance; left unchanged when omitted.' },
        avoidanceWeight: { type: 'number', description: 'RVO avoidance weight (0 or more); left unchanged when omitted.' },
        maxFlySpeed: { type: 'number', description: 'Maximum flying speed; left unchanged when omitted.' },
        jumpZVelocity: { type: 'number', description: 'Initial upward jump velocity; left unchanged when omitted.' },
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
    }),
  ];
