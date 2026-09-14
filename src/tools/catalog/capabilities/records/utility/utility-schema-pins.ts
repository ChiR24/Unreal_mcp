// INPUT-field type pins and descriptions for the utility capability records.
//
// WHY THIS FILE EXISTS
// `helpers.ts` used to decide a field's declared type from its bare name and,
// for any name with no entry in `FIELD_DESCRIPTIONS`, invent a description by
// de-camel-casing it (`humanizeFieldName`). That inference is CONTENT-FREE for
// a field whose name is not English (`attenuationFalloff` shipped "Attenuation
// falloff."), which is the MCPBB-091 defect class, and it is a silent trap: a
// field's declared type changes the moment its name is added to one of these
// sets. Three fields relied on it (`action`, `details`, plus any future name),
// so those descriptions are pinned explicitly here instead.
//
// The pins are keyed on BARE FIELD NAMES and are shared with the INPUT schema
// only. The output envelope is built in `utility-output-schema.ts` and must not
// consult this map - adding `success` here would retype any input named
// `success` (`utility-contract-honesty.test.ts` pins that invariant).

import type { JsonObject } from '../../index.js';

/** Field names whose JSON-Schema type is `boolean` on the wire. */
export const BOOLEAN_FIELDS = new Set([
  'enabled', 'enable', 'replicated', 'reliable', 'withValidation', 'alwaysRelevant',
  'onlyRelevantToOwner', 'usePushModel', 'enablePrediction', 'replicateMovement',
  'autoPlay', 'looping', 'save', 'muted', 'voiceEnabled', 'pushToTalkEnabled',
  'bIsLANMatch', 'bAllowJoinInProgress', 'bAllowInvites', 'bUsesPresence',
  'bUseLobbiesIfAvailable', 'bShouldAdvertise', 'executeTravel', 'forceRespawn',
  'canRespawn', 'systemWide',
  // Legacy input-mapping modifier flags. The native handler reads these with
  // TryGetBoolField (McpAutomationBridge_InputHandlersLegacyMappings.cpp), so
  // publishing them as strings made a schema-valid boolean unrepresentable.
  'shift', 'ctrl', 'alt', 'cmd',
]);

/** Field names whose JSON-Schema type is `number` on the wire. */
export const NUMBER_FIELDS = new Set([
  'volume', 'pitch', 'startTime', 'fadeTime', 'fadeInTime', 'fadeOutTime',
  'targetVolume', 'innerRadius', 'falloffDistance', 'windowSize', 'dopplerIntensity',
  'velocityScale', 'occlusionVolumeScale', 'occlusionFilterScale',
  'occlusionInterpolationTime', 'netUpdateFrequency', 'minNetUpdateFrequency',
  'netPriority', 'netCullDistanceSquared', 'correctionThreshold', 'smoothingRate',
  'priority', 'playerIndex', 'controllerId', 'serverPort', 'attenuationRadius',
  'attenuationFalloff', 'numRounds', 'roundTime', 'intermissionTime', 'numTeams',
  'teamSize', 'scorePerKill', 'scorePerAssist', 'scorePerObjective', 'winScore',
  'respawnDelay', 'teamIndex', 'scale',
  'lowPassFilterFrequency', 'maxRespawns', 'localPlayerNum', 'maxPlayers',
]);

/** Field names whose value is an arbitrary reflection-boundary object. */
export const OBJECT_FIELDS = new Set(['location', 'rotation', 'size', 'properties', 'settings', 'voiceSettings']);

/** Field names whose JSON-Schema type is `array`. */
export const ARRAY_FIELDS = new Set(['states', 'sessions', 'players', 'mappings']);

/**
 * Real descriptions for fields whose bare names read as placeholder text.
 * Descriptions are the one place the caller can learn what a value means
 * without reading the native handler, so echoing the field name is content-free
 * (the MCPBB-091 defect class).
 */
export const FIELD_DESCRIPTIONS: Readonly<Record<string, string>> = {
  name: 'Name of the asset or mapping to create or remove.',
  actionName: 'Legacy input action name. Overrides name when both are supplied.',
  key: 'Input key name, e.g. SpaceBar, W, LeftMouseButton.',
  shift: 'Whether the Shift modifier must be held.',
  ctrl: 'Whether the Ctrl modifier must be held.',
  alt: 'Whether the Alt modifier must be held.',
  cmd: 'Whether the Cmd modifier must be held.',
  axisName: 'Legacy input axis name. Overrides name when both are supplied.',
  scale: 'Axis scale value.',
  actorName: 'Target actor label or name in the current level.',
  analysisType: 'Audio analysis type to enable (for example spectrum or loudness).',
  assetPath: 'Canonical /Game asset path.',
  attachPointName: 'Socket or bone name to attach the sound to.',
  attenuationPath: 'Canonical /Game SoundAttenuation asset path.',
  attenuationShape: 'Attenuation shape (Sphere, Capsule, Box, Cone).',
  autoPlay: 'Whether the sound starts playing on spawn.',
  componentName: 'Name of the component to create or address.',
  concurrencyPath: 'Canonical /Game SoundConcurrency asset path.',
  defaultValue: 'Default value for the input.',
  dopplerIntensity: 'Doppler effect intensity multiplier.',
  effectType: 'Source effect preset class or short name.',
  enable: 'Whether the feature is enabled.',
  enableReverbSend: 'Whether the sound sends to reverb.',
  enabled: 'Whether the feature is enabled.',
  fadeInTime: 'Fade-in duration in seconds.',
  fadeOutTime: 'Fade-out duration in seconds.',
  fadeTime: 'Fade duration in seconds.',
  fadeType: 'Fade curve type (FadeTo, FadeIn, FadeOut).',
  falloffDistance: 'Distance over which attenuation falls off, in centimetres.',
  falloffMode: 'Attenuation falloff mode.',
  innerRadius: 'Inner radius of full volume, in centimetres.',
  inputName: 'Graph input name.',
  inputType: 'Graph input data type (Float, Int32, Bool, String, Trigger, Audio).',
  location: 'World location as {x, y, z} (an [x, y, z] array is accepted).',
  looping: 'Whether playback loops.',
  lowPassFilterFrequency: 'Low-pass filter cutoff frequency in Hz.',
  mixName: 'Sound Mix name.',
  nodeClassName: 'Node class name; short names such as Sine resolve against the MetaSound registry (UE.Sine.Audio).',
  nodeType: 'Node type or class short name.',
  occlusionFilterScale: 'Low-pass filter scale applied while occluded (0-1).',
  occlusionInterpolationTime: 'Seconds to interpolate occlusion changes.',
  occlusionVolumeScale: 'Volume scale applied while occluded (0-1).',
  outputName: 'Graph output name.',
  outputType: 'Graph output data type.',
  parentClass: 'Parent class path or short name.',
  path: 'Canonical /Game folder for the created asset.',
  pitch: 'Pitch multiplier.',
  properties: 'Key-value property map applied by reflection.',
  reverbDistanceMax: 'Distance at which the reverb wet level reaches its maximum.',
  reverbDistanceMin: 'Distance at which the reverb send starts.',
  reverbEffect: 'Canonical /Game ReverbEffect asset path.',
  reverbWetLevelMax: 'Maximum reverb wet level (0-1).',
  reverbWetLevelMin: 'Minimum reverb wet level (0-1).',
  rotation: 'World rotation as {pitch, yaw, roll}.',
  save: 'Persist the created or modified asset to disk.',
  size: 'Reverb zone extent as {x, y, z}.',
  soundClassName: 'Sound Class name.',
  soundClassPath: 'Canonical /Game SoundClass asset path.',
  soundCue: 'Sound cue.',
  soundName: 'Actor label/name or AudioComponent name of the playing sound.',
  soundPath: 'Canonical /Game sound asset path (SoundWave, SoundCue or MetaSound).',
  sourceNode: 'Source node id or name.',
  sourceNodeId: 'Source graph node id.',
  sourceOutputName: 'Output pin name on the source node.',
  sourcePin: 'Output pin name on the source node.',
  spatialization: 'Spatialization method (Default, Binaural).',
  speakerPath: 'Canonical /Game DialogueVoice asset path of the speaker.',
  startTime: 'Playback start offset in seconds.',
  targetInputName: 'Input pin name on the target node.',
  targetNode: 'Target node id or name.',
  targetNodeId: 'Target graph node id.',
  targetPin: 'Input pin name on the target node.',
  targetVolume: 'Target volume multiplier (0-1).',
  velocityScale: 'Velocity scale for Doppler calculations.',
  volume: 'Volume multiplier.',
  volumeAdjuster: 'Volume multiplier applied by the mix modifier.',
  wavePath: 'Canonical /Game SoundWave asset path.',
  windowSize: 'Analysis window size in samples.',
  sessionName: 'Session name.',
  maxPlayers: 'Max players.',
  bIsLANMatch: 'Whether lan match applies.',
  bAllowJoinInProgress: 'Whether allow join in progress applies.',
  bAllowInvites: 'Whether allow invites applies.',
  bUsesPresence: 'Whether uses presence applies.',
  bUseLobbiesIfAvailable: 'Whether use lobbies if available applies.',
  bShouldAdvertise: 'Whether advertise applies.',
  interfaceType: 'Interface type.',
  splitScreenType: 'Split screen type.',
  controllerId: 'Controller id.',
  playerIndex: 'Player index.',
  serverPort: 'Server port.',
  mapName: 'Map name.',
  serverName: 'Server name.',
  serverPassword: 'Server password.',
  travelOptions: 'Travel options.',
  executeTravel: 'Execute travel.',
  serverAddress: 'Server address.',
  voiceEnabled: 'Voice enabled.',
  voiceSettings: 'Voice settings.',
  channelName: 'Channel name.',
  channelType: 'Channel type.',
  playerName: 'Player name.',
  targetPlayerId: 'Target player id.',
  muted: 'Muted.',
  localPlayerNum: 'Local player num.',
  systemWide: 'System wide.',
  attenuationRadius: 'Attenuation radius.',
  attenuationFalloff: 'Attenuation falloff.',
  pushToTalkEnabled: 'Push to talk enabled.',
  pushToTalkKey: 'Push to talk key.',
  blueprintPath: 'Blueprint path (canonical /Game asset path).',
  propertyName: 'Property name.',
  replicated: 'Replicated.',
  condition: 'Condition.',
  netUpdateFrequency: 'Net update frequency.',
  minNetUpdateFrequency: 'Min net update frequency.',
  netPriority: 'Net priority.',
  dormancy: 'Dormancy.',
  spatiallyLoaded: 'Spatially loaded.',
  netLoadOnClient: 'Net load on client.',
  replicationPolicy: 'Replication policy.',
  functionName: 'Function name.',
  rpcType: 'Rpc type.',
  reliable: 'Reliable.',
  withValidation: 'With validation.',
  ownerActorName: 'Owner actor name.',
  isAutonomousProxy: 'Whether autonomous proxy applies.',
  netCullDistanceSquared: 'Net cull distance squared.',
  useOwnerNetRelevancy: 'Use owner net relevancy.',
  alwaysRelevant: 'Always relevant.',
  onlyRelevantToOwner: 'Only relevant to owner.',
  structName: 'Struct name.',
  customSerialization: 'Custom serialization.',
  repNotifyFunc: 'Rep notify func.',
  usePushModel: 'Use push model.',
  enablePrediction: 'Whether prediction applies.',
  predictionThreshold: 'Prediction threshold.',
  correctionThreshold: 'Correction threshold.',
  smoothingRate: 'Smoothing rate.',
  dataType: 'Data type.',
  variableName: 'Variable name.',
  networkSmoothingMode: 'Network smoothing mode.',
  networkMaxSmoothUpdateDistance: 'Network max smooth update distance.',
  networkNoSmoothUpdateDistance: 'Network no smooth update distance.',
  maxClientRate: 'Max client rate.',
  maxInternetClientRate: 'Max internet client rate.',
  netServerMaxTickRate: 'Net server max tick rate.',
  role: 'Role.',
  replicateMovement: 'Replicate movement.',
  valueType: 'Value type.',
  priority: 'Priority.',
  contextPath: 'Context path (canonical /Game asset path).',
  actionPath: 'Action path (canonical /Game asset path).',
  triggerType: 'Trigger type.',
  modifierType: 'Modifier type.',
  defaultPawnClass: 'Default pawn class.',
  playerControllerClass: 'Player controller class.',
  gameStateClass: 'Game state class.',
  playerStateClass: 'Player state class.',
  hudClass: 'Hud class.',
  gameModeBlueprint: 'Game mode blueprint.',
  pawnClass: 'Pawn class.',
  bDelayedStart: 'Whether delayed start applies.',
  states: 'States.',
  numRounds: 'Num rounds.',
  roundTime: 'Round time.',
  intermissionTime: 'Intermission time.',
  numTeams: 'Num teams.',
  teamSize: 'Team size.',
  autoBalance: 'Auto balance.',
  friendlyFire: 'Friendly fire.',
  scorePerKill: 'Score per kill.',
  scorePerAssist: 'Score per assist.',
  scorePerObjective: 'Score per objective.',
  scorePerDeath: 'Score per death.',
  winScore: 'Win score.',
  spawnSelectionMethod: 'Spawn selection method.',
  respawnDelay: 'Respawn delay.',
  respawnLocation: 'Respawn location.',
  usePlayerStarts: 'Use player starts.',
  canRespawn: 'Can respawn.',
  maxRespawns: 'Max respawns.',
  teamIndex: 'Team index.',
  forceRespawn: 'Force respawn.',
  respawnLives: 'Respawn lives.',
  allowSpectating: 'Allow spectating.',
  spectatorClass: 'Spectator class.',
  spectatorViewMode: 'Spectator view mode.',
  // The three descriptions below were previously produced by the removed
  // `humanizeFieldName` fallback. They are hoisted verbatim so no emitted
  // description or type changes on the three fields that actually resolved to
  // them. `action` needs the pin because the generic verb branch would have
  // matched `/^(b|is|has|should|enable)/` and read "Whether action applies.".
  action: 'Action.',
  details: 'Details.',
  description: 'Description.',
};

/**
 * A described property whose JSON-Schema type is inferred from the field name.
 * The description is always the pinned string; a name missing from the map is a
 * gap in this module, so it throws rather than returning `undefined` (which
 * `JSON.stringify` would drop, shipping a contract with no description at all).
 */
export function property(name: string): JsonObject {
  const description = FIELD_DESCRIPTIONS[name];
  if (description === undefined) throw new TypeError(`utility field has no pinned description: ${name}`);
  if (BOOLEAN_FIELDS.has(name)) return { type: 'boolean', description };
  if (NUMBER_FIELDS.has(name)) return { type: 'number', description };
  if (OBJECT_FIELDS.has(name)) {
    return {
      type: 'object',
      description,
      additionalProperties: true,
      'x-unreal-reflection-boundary': true,
    };
  }
  if (ARRAY_FIELDS.has(name)) return { type: 'array', description, items: {} };
  return { type: 'string', description };
}
