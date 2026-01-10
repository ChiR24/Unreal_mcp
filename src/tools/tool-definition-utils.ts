/**
 * Common Schema Definitions for MCP Tool Definitions
 * Phase 49: Common Schema Extraction - reduces token consumption by centralizing repeated patterns
 */

export const commonSchemas = {
  // ============================================
  // TRANSFORM & VECTOR SCHEMAS
  // ============================================
  location: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D location (x, y, z).'
  },
  rotation: {
    type: 'object',
    properties: {
      pitch: { type: 'number' },
      yaw: { type: 'number' },
      roll: { type: 'number' }
    },
    description: '3D rotation (pitch, yaw, roll).'
  },
  scale: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D scale (x, y, z).'
  },
  vector3: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D vector.'
  },
  vector2: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' }
    },
    description: '2D vector.'
  },

  // ============================================
  // COLOR SCHEMAS
  // ============================================
  color: {
    type: 'array',
    items: { type: 'number' },
    description: 'RGBA color as an array [r, g, b, a].'
  },
  colorObject: {
    type: 'object',
    properties: {
      r: { type: 'number' },
      g: { type: 'number' },
      b: { type: 'number' },
      a: { type: 'number' }
    },
    description: 'RGBA color as an object.'
  },

  // ============================================
  // PATH SCHEMAS
  // ============================================
  assetPath: { type: 'string', description: 'Asset path (e.g., /Game/Path/Asset).' },
  blueprintPath: { type: 'string', description: 'Blueprint asset path.' },
  meshPath: { type: 'string', description: 'Mesh asset path.' },
  texturePath: { type: 'string', description: 'Texture asset path.' },
  materialPath: { type: 'string', description: 'Material asset path.' },
  soundPath: { type: 'string', description: 'Sound asset path.' },
  animationPath: { type: 'string', description: 'Animation asset path.' },
  levelPath: { type: 'string', description: 'Level asset path.' },
  skeletonPath: { type: 'string', description: 'Skeleton asset path.' },
  skeletalMeshPath: { type: 'string', description: 'Skeletal mesh path.' },
  niagaraPath: { type: 'string', description: 'Niagara system path.' },
  widgetPath: { type: 'string', description: 'Widget blueprint path.' },
  sequencePath: { type: 'string', description: 'Level Sequence asset path.' },
  mediaPlayerPath: { type: 'string', description: 'Media Player asset path.' },
  mediaSourcePath: { type: 'string', description: 'Media Source asset path.' },
  mediaTexturePath: { type: 'string', description: 'Media Texture asset path.' },
  exportPath: { type: 'string', description: 'Export file path.' },

  physicsAssetPath: { type: 'string', description: 'Path to physics asset.' },
  morphTargetPath: { type: 'string', description: 'Path to morph target.' },
  clothAssetPath: { type: 'string', description: 'Path to cloth asset.' },
  iconPath: { type: 'string', description: 'Path to icon texture.' },
  itemDataPath: { type: 'string', description: 'Path to item data asset.' },
  gameplayAbilityPath: { type: 'string', description: 'Path to gameplay ability.' },
  gameplayEffectPath: { type: 'string', description: 'Path to gameplay effect.' },
  gameplayCuePath: { type: 'string', description: 'Path to gameplay cue.' },
  meshAssetPath: { type: 'string', description: 'Path to mesh asset.' },
  textureAssetPath: { type: 'string', description: 'Path to texture asset.' },
  materialAssetPath: { type: 'string', description: 'Path to material asset.' },
  soundAssetPath: { type: 'string', description: 'Path to sound asset.' },
  animationAssetPath: { type: 'string', description: 'Path to animation asset.' },
  blueprintAssetPath: { type: 'string', description: 'Path to blueprint asset.' },

  directoryPath: { type: 'string', description: 'Path to a directory.' },
  outputPath: { type: 'string', description: 'Output file or directory path.' },
  destinationPath: { type: 'string', description: 'Destination path for move/copy.' },
  savePath: { type: 'string', description: 'Path to save the asset.' },
  sourcePath: { type: 'string', description: 'Source path for import/move/copy.' },
  targetPath: { type: 'string', description: 'Target path for operations.' },
  directoryPathForCreation: { type: 'string', description: 'Directory path for asset creation.' },

  // ============================================
  // NAME SCHEMAS
  // ============================================
  name: { type: 'string', description: 'Name identifier.' },
  newName: { type: 'string', description: 'New name for renaming.' },
  assetNameForCreation: { type: 'string', description: 'Name of the asset to create.' },
  actorName: { type: 'string', description: 'Name of the actor.' },
  actorNameInLevel: { type: 'string', description: 'Name of the actor in the level.' },
  childActorName: { type: 'string', description: 'Name of the child actor (for attach/detach operations).' },
  parentActorName: { type: 'string', description: 'Name of the parent actor (for attach operations).' },
  componentName: { type: 'string', description: 'Name of the component.' },
  boneName: { type: 'string', description: 'Name of the bone.' },
  socketName: { type: 'string', description: 'Name of the socket.' },
  slotName: { type: 'string', description: 'Name of the slot.' },
  parameterName: { type: 'string', description: 'Name of the parameter.' },
  propertyName: { type: 'string', description: 'Name of the property.' },
  variableName: { type: 'string', description: 'Name of the variable.' },
  functionName: { type: 'string', description: 'Name of the function.' },
  eventName: { type: 'string', description: 'Name of the event.' },
  tagName: { type: 'string', description: 'Name of the tag.' },
  attributeName: { type: 'string', description: 'Name of the attribute.' },
  stateName: { type: 'string', description: 'Name of the state.' },

  // ============================================
  // GRAPH & NODE SCHEMAS
  // ============================================
  nodeId: { type: 'string', description: 'ID of the node.' },
  sourceNodeId: { type: 'string', description: 'ID of the source node.' },
  targetNodeId: { type: 'string', description: 'ID of the target node.' },
  pinName: { type: 'string', description: 'Name of the pin.' },
  sourcePin: { type: 'string', description: 'Name of the source pin.' },
  targetPin: { type: 'string', description: 'Name of the target pin.' },
  graphName: { type: 'string', description: 'Name of the graph.' },
  nodeName: { type: 'string', description: 'Name of the node.' },

  // ============================================
  // COMMON PROPERTY TYPES
  // ============================================
  booleanProp: { type: 'boolean' },
  numberProp: { type: 'number' },
  stringProp: { type: 'string' },
  // Note: 'integer' is a valid JSON Schema type for whole numbers (counts, indices)
  integerProp: { type: 'integer' },
  objectProp: { type: 'object' },
  arrayOfStrings: { type: 'array', items: { type: 'string' } },
  // Note: arrayOfNumbers is used for SCS transforms [x,y,z] format in Blueprint Manager.
  // Use location/rotation/scale objects for Actor Control {x,y,z} format.
  arrayOfNumbers: { type: 'array', items: { type: 'number' } },
  arrayOfObjects: { type: 'array', items: { type: 'object' } },
  value: { description: 'Generic value (any type).' },
  parentClass: { type: 'string', description: 'Path or name of the parent class.' },

  // ============================================
  // COMMON FLAGS (BOOLEANS)
  // ============================================
  save: { type: 'boolean', description: 'Save the asset(s) after the operation.' },
  compile: { type: 'boolean', description: 'Compile the blueprint(s) after the operation.' },
  overwrite: { type: 'boolean', description: 'Overwrite if the asset/file already exists.' },
  recursive: { type: 'boolean', description: 'Perform the operation recursively.' },
  enabled: { type: 'boolean', description: 'Whether the item/feature is enabled.' },
  visible: { type: 'boolean', description: 'Whether the item/actor is visible.' },

  // ============================================
  // FILTERS & SETTINGS
  // ============================================
  filter: { type: 'string', description: 'General search filter.' },
  tagFilter: { type: 'string', description: 'Filter by tags.' },
  classFilter: { type: 'string', description: 'Filter by class.' },
  resolution: { type: 'string', description: 'Resolution setting (e.g., 1024x1024).' },

  // ============================================
  // OUTPUT SCHEMA COMPONENTS (Properties for spreading)
  // ============================================
  outputBase: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' }
  },
  outputWithPath: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    path: { type: 'string' },
    assetPath: { type: 'string' }
  },
  outputWithActor: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    actor: { type: 'string' },
    actorPath: { type: 'string' }
  },
  outputWithNodeId: {
    success: { type: 'boolean' },
    message: { type: 'string' },
    error: { type: 'string' },
    nodeId: { type: 'string' }
  },

  // ============================================
  // DIMENSIONS & SIZES
  // ============================================
  dimensions: {
    type: 'object',
    properties: {
      width: { type: 'number' },
      height: { type: 'number' },
      depth: { type: 'number' }
    },
    description: '3D dimensions (width, height, depth).'
  },
  size2D: {
    type: 'object',
    properties: {
      width: { type: 'number' },
      height: { type: 'number' }
    },
    description: '2D dimensions (width, height).'
  },
  extent: {
    type: 'object',
    properties: {
      x: { type: 'number' },
      y: { type: 'number' },
      z: { type: 'number' }
    },
    description: '3D extent (half-size).'
  },

  // ============================================
  // NUMBER SCHEMAS
  // ============================================
  width: { type: 'number', description: 'Width value.' },
  height: { type: 'number', description: 'Height value.' },
  depth: { type: 'number', description: 'Depth value.' },
  radius: { type: 'number', description: 'Radius value.' },
  intensity: { type: 'number', description: 'Intensity value.' },
  angle: { type: 'number', description: 'Angle in degrees.' },
  strength: { type: 'number', description: 'Strength or weight.' },
  speed: { type: 'number', description: 'Speed value.' },
  duration: { type: 'number', description: 'Duration in seconds.' },
  distance: { type: 'number', description: 'Distance value.' },

  // ============================================
  // RANGES & BOUNDS
  // ============================================
  floatRange: {
    type: 'object',
    properties: {
      min: { type: 'number' },
      max: { type: 'number' }
    },
    description: 'Range of float values.'
  },
  intRange: {
    type: 'object',
    properties: {
      min: { type: 'integer' },
      max: { type: 'integer' }
    },
    description: 'Range of integer values.'
  },
  bounds: {
    type: 'object',
    properties: {
      min: {
        type: 'object',
        properties: {
          x: { type: 'number' },
          y: { type: 'number' },
          z: { type: 'number' }
        }
      },
      max: {
        type: 'object',
        properties: {
          x: { type: 'number' },
          y: { type: 'number' },
          z: { type: 'number' }
        }
      }
    },
    description: 'Bounding box (min, max vectors).'
  },

  // ============================================
  // SESSION 2: Additional Common Schemas
  // ============================================
  
  // Path-like string schemas
  controllerPath: { type: 'string', description: 'Path to controller blueprint.' },
  behaviorTreePath: { type: 'string', description: 'Path to behavior tree asset.' },
  blackboardPath: { type: 'string', description: 'Path to blackboard asset.' },
  queryPath: { type: 'string', description: 'Path to EQS query asset.' },
  stateTreePath: { type: 'string', description: 'Path to State Tree asset.' },
  definitionPath: { type: 'string', description: 'Path to definition asset.' },
  configPath: { type: 'string', description: 'Path to config asset.' },
  lootTablePath: { type: 'string', description: 'Path to loot table asset.' },
  recipePath: { type: 'string', description: 'Path to crafting recipe asset.' },
  effectPath: { type: 'string', description: 'Path to effect asset.' },
  abilityPath: { type: 'string', description: 'Path to ability asset.' },
  animBlueprintPath: { type: 'string', description: 'Path to animation blueprint.' },
  
  // Name schemas for specific domains  
  keyName: { type: 'string', description: 'Name of the key.' },
  sessionName: { type: 'string', description: 'Name of the session.' },
  channelName: { type: 'string', description: 'Name of the channel.' },
  volumeName: { type: 'string', description: 'Name of the volume.' },
  linkName: { type: 'string', description: 'Name of the link.' },
  
  // Common boolean flags
  looping: { type: 'boolean', description: 'Whether to loop.' },
  locked: { type: 'boolean', description: 'Whether the item is locked.' },
  muted: { type: 'boolean', description: 'Whether the item is muted.' },
  replicated: { type: 'boolean', description: 'Whether to replicate.' },
  reliable: { type: 'boolean', description: 'Whether the operation is reliable.' },
  
  // Common number properties
  volume: { type: 'number', description: 'Volume level.' },
  pitch: { type: 'number', description: 'Pitch value.' },
  fadeTime: { type: 'number', description: 'Fade time in seconds.' },
  cooldown: { type: 'number', description: 'Cooldown time in seconds.' },
  respawnTime: { type: 'number', description: 'Respawn time in seconds.' },
  lifetime: { type: 'number', description: 'Lifetime in seconds.' },
  mass: { type: 'number', description: 'Mass value.' },
  friction: { type: 'number', description: 'Friction coefficient.' },
  restitution: { type: 'number', description: 'Restitution (bounciness).' },
  priority: { type: 'number', description: 'Priority value.' },
  
  // Input/output pin schemas
  inputName: { type: 'string', description: 'Name of the input.' },
  outputName: { type: 'string', description: 'Name of the output.' },

  // ============================================
  // SESSION 3: Additional Common Schemas
  // ============================================
  
  // Bone-related schemas
  parentBoneName: { type: 'string', description: 'Parent bone name.' },
  sourceBoneName: { type: 'string', description: 'Source bone name.' },
  targetBoneName: { type: 'string', description: 'Target bone name.' },
  attachBoneName: { type: 'string', description: 'Bone to attach to.' },
  startBone: { type: 'string', description: 'Start bone name.' },
  endBone: { type: 'string', description: 'End bone name.' },
  
  // State transition schemas
  fromState: { type: 'string', description: 'Source state name.' },
  toState: { type: 'string', description: 'Target state name.' },
  
  // Connection node/element schemas
  sourceNode: { type: 'string', description: 'Source node name.' },
  targetNode: { type: 'string', description: 'Target node name.' },
  sourceElement: { type: 'string', description: 'Source element name.' },
  targetElement: { type: 'string', description: 'Target element name.' },
  
  // Chain and section schemas
  chainName: { type: 'string', description: 'Name of the chain.' },
  sectionName: { type: 'string', description: 'Name of the section.' },
  cacheName: { type: 'string', description: 'Name of the cache.' },
  
  // Class path schemas
  widgetClass: { type: 'string', description: 'Widget class path.' },
  projectileClass: { type: 'string', description: 'Projectile class path.' },
  soundClassPath: { type: 'string', description: 'Sound class path.' },
  parentClassPath: { type: 'string', description: 'Parent class path.' },
  areaClass: { type: 'string', description: 'Navigation area class path.' },
  
  // Trace schemas
  traceChannel: { type: 'string', description: 'Collision trace channel.' },
  
  // IK/Rig path schemas
  sourceIKRigPath: { type: 'string', description: 'Source IK rig path.' },
  targetIKRigPath: { type: 'string', description: 'Target IK rig path.' },
  sourceChain: { type: 'string', description: 'Source chain name.' },
  targetChain: { type: 'string', description: 'Target chain name.' },
  
  // Socket-specific schemas
  muzzleSocketName: { type: 'string', description: 'Muzzle socket name.' },
  ejectionSocketName: { type: 'string', description: 'Shell ejection socket name.' },
  cameraSocketName: { type: 'string', description: 'Camera socket name.' },
  
  // Layer/Data schemas
  layerName: { type: 'string', description: 'Name of the layer.' },
  dataLayerName: { type: 'string', description: 'Name of the data layer.' },
  
  // Inventory/Item number schemas
  stackSize: { type: 'number', description: 'Stack size.' },
  weight: { type: 'number', description: 'Weight value.' },
  slotCount: { type: 'number', description: 'Number of slots.' },
  maxWeight: { type: 'number', description: 'Maximum weight.' },
  maxPlayers: { type: 'number', description: 'Maximum player count.' },
  playerIndex: { type: 'number', description: 'Player index.' },
  controllerId: { type: 'number', description: 'Controller ID.' },
  serverPort: { type: 'number', description: 'Server port number.' },
  
  // Text content schemas
  text: { type: 'string', description: 'Text content.' },
  code: { type: 'string', description: 'Code or expression.' },
  prompt: { type: 'string', description: 'Prompt text.' },
  
  // Group/Category schemas
  group: { type: 'string', description: 'Group name.' },
  
  // Audio-specific paths
  wavePath: { type: 'string', description: 'Path to SoundWave asset.' },
  attenuationPath: { type: 'string', description: 'Path to attenuation asset.' },
  concurrencyPath: { type: 'string', description: 'Path to concurrency asset.' },
  
  // Niagara-specific paths
  systemPath: { type: 'string', description: 'Path to Niagara system.' },
  emitterPath: { type: 'string', description: 'Path to Niagara emitter.' },
  emitterName: { type: 'string', description: 'Name of the emitter.' },
  
  // Level-related schemas
  sublevelName: { type: 'string', description: 'Name of the sublevel.' },
  parentLevel: { type: 'string', description: 'Parent level path.' },
  templateLevel: { type: 'string', description: 'Template level path.' },
  
  // Montage section schemas
  fromSection: { type: 'string', description: 'Source section name.' },
  toSection: { type: 'string', description: 'Target section name.' },
  
  // Axis names for blend spaces
  axisName: { type: 'string', description: 'Axis name.' },
  horizontalAxisName: { type: 'string', description: 'Horizontal axis name.' },
  verticalAxisName: { type: 'string', description: 'Vertical axis name.' },
  
  // Control Rig schemas
  controlName: { type: 'string', description: 'Control name.' },
  parentBone: { type: 'string', description: 'Parent bone name.' },
  parentControl: { type: 'string', description: 'Parent control name.' },
  unitName: { type: 'string', description: 'Rig unit name.' },
  goal: { type: 'string', description: 'IK goal name.' },
  
  // Animation notify schemas
  notifyClass: { type: 'string', description: 'Animation notify class.' },
  notifyName: { type: 'string', description: 'Animation notify name.' },
  curveName: { type: 'string', description: 'Animation curve name.' },
  markerName: { type: 'string', description: 'Sync marker name.' },
  
  // State machine schemas
  stateMachineName: { type: 'string', description: 'State machine name.' },
  
  // Physics body schemas
  bodyName: { type: 'string', description: 'Physics body name.' },
  bodyA: { type: 'string', description: 'First physics body.' },
  bodyB: { type: 'string', description: 'Second physics body.' },
  constraintName: { type: 'string', description: 'Constraint name.' },
  
  // Morph target schemas
  morphTargetName: { type: 'string', description: 'Morph target name.' },
  
  // Brush/Style schemas
  brush: { type: 'string', description: 'Brush asset path.' },
  style: { type: 'string', description: 'Style preset name.' },
  
  // Binding schemas
  bindingSource: { type: 'string', description: 'Binding source name.' },
  sourceBinding: { type: 'string', description: 'Source binding path.' },
  
  // Staging/Simulation schemas
  stageName: { type: 'string', description: 'Simulation stage name.' },
  
  // Network replication schemas
  repNotifyFunc: { type: 'string', description: 'RepNotify function name.' },

  // ============================================
  // SESSION 4: Additional Number Schemas
  // ============================================
  
  // Trace/Collision schemas
  traceDistance: { type: 'number', description: 'Trace distance.' },
  traceRadius: { type: 'number', description: 'Trace radius.' },
  traceFrequency: { type: 'number', description: 'Trace frequency.' },
  
  // Time-related schemas
  startTime: { type: 'number', description: 'Start time in seconds.' },
  endTime: { type: 'number', description: 'End time in seconds.' },
  blendTime: { type: 'number', description: 'Blend time in seconds.' },
  
  // Frame/Rate schemas
  frameRate: { type: 'number', description: 'Frames per second.' },
  numFrames: { type: 'number', description: 'Number of frames.' },
  frame: { type: 'number', description: 'Frame number.' },
  
  // Animation timing
  startFrame: { type: 'number', description: 'Start frame.' },
  endFrame: { type: 'number', description: 'End frame.' },
  trackIndex: { type: 'number', description: 'Track index.' },
  
  // Node positions
  nodeX: { type: 'number', description: 'Node X position.' },
  nodeY: { type: 'number', description: 'Node Y position.' },
  
  // Combat/Damage schemas
  damageMultiplier: { type: 'number', description: 'Damage multiplier.' },
  criticalMultiplier: { type: 'number', description: 'Critical hit multiplier.' },
  
  // Bounds schemas
  minValue: { type: 'number', description: 'Minimum value.' },
  maxValue: { type: 'number', description: 'Maximum value.' },

  // ============================================
  // SESSION 5: More Path Schemas
  // ============================================
  
  functionPath: { type: 'string', description: 'Path to function asset.' },
  particleSystemPath: { type: 'string', description: 'Path to particle system.' },
  cameraShakePath: { type: 'string', description: 'Path to camera shake asset.' },
  decalPath: { type: 'string', description: 'Path to decal material.' },
  actorPath: { type: 'string', description: 'Path to actor.' },
  volumePath: { type: 'string', description: 'Path to volume.' },
  sessionId: { type: 'string', description: 'Session ID.' },
  hlodLayerPath: { type: 'string', description: 'Path to HLOD layer.' },
  levelInstanceName: { type: 'string', description: 'Level instance name.' },
  serverAddress: { type: 'string', description: 'Server address.' },
  nodeClass: { type: 'string', description: 'Node class path.' },
  animationName: { type: 'string', description: 'Animation name.' }
};
