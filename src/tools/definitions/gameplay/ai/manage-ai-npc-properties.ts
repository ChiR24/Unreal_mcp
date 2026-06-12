import { commonSchemas } from '../../../catalog/tool-definition-utils.js';

export const manageAiNpcProperties = {
  // Dialogue
  dialoguePath: {
    type: 'string',
    description: 'Asset path for the NPC dialogue tree.'
  },
  dialogueNodeType: {
    type: 'string',
    enum: ['line', 'choice', 'condition', 'event', 'end'],
    description: 'Type of dialogue node to add.'
  },
  speakerName: {
    type: 'string',
    description: 'Name of the speaker for a dialogue node.'
  },
  dialogueText: {
    type: 'string',
    description: 'Text content of the dialogue line.'
  },
  dialogueCondition: {
    type: 'string',
    description: 'Condition expression (blackboard key or Blueprint function) to evaluate for a dialogue branch.'
  },
  fromNodeId: commonSchemas.nodeId,
  toNodeId: commonSchemas.nodeId,

  // Behavior Modes
  behaviorMode: {
    type: 'string',
    enum: ['patrol', 'alert', 'combat', 'idle'],
    description: 'NPC behavior mode to configure.'
  },
  waypointList: {
    type: 'array',
    items: {
      type: 'object',
      properties: {
        x: { type: 'number' },
        y: { type: 'number' },
        z: { type: 'number' },
        waitTime: { type: 'number' }
      }
    },
    description: 'List of waypoints for patrol routes.'
  },
  detectionRadius: {
    type: 'number',
    description: 'Alert detection radius in world units.'
  },
  combatStrategy: {
    type: 'string',
    enum: ['aggressive', 'defensive', 'flanking', 'ranged', 'retreat'],
    description: 'Strategy used during combat mode.'
  },
  idleAnimations: {
    type: 'array',
    items: { type: 'string' },
    description: 'List of animation asset paths for idle activities.'
  },
  fromMode: {
    type: 'string',
    enum: ['patrol', 'alert', 'combat', 'idle'],
    description: 'Source behavior mode for a transition.'
  },
  toMode: {
    type: 'string',
    enum: ['patrol', 'alert', 'combat', 'idle'],
    description: 'Target behavior mode for a transition.'
  },
  transitionCondition: {
    type: 'string',
    description: 'Blackboard key or condition expression that triggers mode transition.'
  },

  // NPC Director / Spawning
  spawnerName: {
    type: 'string',
    description: 'Name of the NPC spawner configuration.'
  },
  npcClass: {
    type: 'string',
    description: 'NPC character Blueprint class path.'
  },
  maxCount: {
    type: 'number',
    description: 'Maximum number of NPC instances in the pool.'
  },
  spawnRadius: {
    type: 'number',
    description: 'Radius around the spawner origin for NPC placement.'
  },
  spawnConditions: {
    type: 'object',
    properties: {
      minDistanceToPlayer: { type: 'number' },
      maxDistanceToPlayer: { type: 'number' },
      triggerEvent: { type: 'string' },
      timeOfDay: { type: 'string' }
    },
    description: 'Conditions controlling when NPCs spawn.'
  },
  groupName: {
    type: 'string',
    description: 'Name of the NPC group.'
  },
  leaderName: {
    type: 'string',
    description: 'Actor name of the group leader NPC.'
  },
  groupTactic: {
    type: 'string',
    enum: ['flank', 'surround', 'retreat', 'hold_position', 'follow_leader'],
    description: 'Group-level tactical behavior.'
  },

  // Memory & Personality
  memoryEventType: {
    type: 'string',
    enum: ['attacked_by', 'saw_enemy', 'heard_noise', 'found_item', 'custom'],
    description: 'Type of memory event to record.'
  },
  memorySubject: {
    type: 'string',
    description: 'Actor name that is the subject of the memory.'
  },
  memoryData: {
    type: 'object',
    description: 'Additional freeform data for the memory record.'
  },
  personalityTraits: {
    type: 'object',
    properties: {
      aggression: { type: 'number', description: '0.0 - 1.0' },
      curiosity: { type: 'number', description: '0.0 - 1.0' },
      cowardice: { type: 'number', description: '0.0 - 1.0' },
      loyalty: { type: 'number', description: '0.0 - 1.0' }
    },
    description: 'NPC personality trait values (0.0 = low, 1.0 = high).'
  },
  factionName: {
    type: 'string',
    description: 'Faction name for the reputation system.'
  },
  reputationScore: {
    type: 'number',
    description: 'Initial reputation score with the faction (-100 to 100).'
  }
};
