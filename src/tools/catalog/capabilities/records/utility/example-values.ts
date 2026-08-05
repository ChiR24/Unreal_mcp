/**
 * Example-value synthesis for the utility capability records.
 *
 * WHY THIS FILE EXISTS
 * `helpers.ts` used to emit a fixed `input: { action }` / `output: { success: true }`
 * pair for all 127 utility records. That left 120 records whose first example
 * omitted a required input and 35 whose first example omitted a required
 * output, so the shipped examples could not survive the very validator the
 * gateway runs against real calls (`gateway-execute-validate.ts`).
 *
 * Values below are real Unreal vocabulary, not filler: enum members come from
 * the shipping handler types (e.g. `SessionsArgs.splitScreenType` and
 * `interfaceType: 'Default' | 'LAN' | 'Null'` in
 * `src/types/handlers/handler-session-types.ts`), and reflection-boundary
 * objects are populated from fields the plugin genuinely writes.
 *
 * HONESTY BOUNDARY - see the note above REFLECTED_EXAMPLES before editing.
 */
import type { JsonObject, JsonValue } from '../../index.js';

/** Asset-name and path defaults chosen per family so identifiers read true. */
const FAMILY_DEFAULTS: Readonly<Record<string, { readonly name: string; readonly path: string; readonly assetPath: string }>> = {
  authoring: { name: 'Cue_FootstepConcrete', path: '/Game/Audio/Cues', assetPath: '/Game/Audio/Cues/Cue_FootstepConcrete' },
  metasound: { name: 'MS_EngineTone', path: '/Game/Audio/MetaSounds', assetPath: '/Game/Audio/MetaSounds/MS_EngineTone' },
  runtime: { name: 'ReverbZone_Cavern', path: '/Game/Audio', assetPath: '/Game/Audio/Cues/Cue_FootstepConcrete' },
  replication: { name: 'BP_PlayerCharacter', path: '/Game/Blueprints', assetPath: '/Game/Blueprints/BP_PlayerCharacter' },
  session: { name: 'ArenaSession', path: '/Game/Blueprints/Framework', assetPath: '/Game/Blueprints/Framework/BP_ArenaGameMode' },
  gameFramework: { name: 'BP_ArenaGameMode', path: '/Game/Blueprints/Framework', assetPath: '/Game/Blueprints/Framework/BP_ArenaGameMode' },
  input: { name: 'IA_Jump', path: '/Game/Input/Actions', assetPath: '/Game/Input/Actions/IA_Jump' },
};

/** Field values that mean the same thing everywhere they appear. */
const FIELD_EXAMPLES: Readonly<Record<string, JsonValue>> = {
  actionPath: '/Game/Input/Actions/IA_Jump',
  actorName: 'BP_PlayerCharacter_C_0',
  attenuationRadius: 1200,
  blueprintPath: '/Game/Blueprints/BP_PlayerCharacter',
  channelName: 'TeamAlpha',
  componentName: 'FootstepAudio',
  condition: 'COND_OwnerOnly',
  contextPath: '/Game/Input/IMC_Default',
  controllerId: 0,
  dataType: 'FVector',
  dormancy: 'DORM_DormantAll',
  effectType: 'SourceEffectFilter',
  enabled: true,
  entryIndex: 0,
  existsAfter: true,
  functionName: 'ServerFireWeapon',
  gameModeBlueprint: '/Game/Blueprints/Framework/BP_ArenaGameMode',
  gameStateClass: '/Game/Blueprints/Framework/BP_ArenaGameState',
  hasAuthority: true,
  inputName: 'Frequency',
  inputType: 'Float',
  interfaceType: 'LAN',
  isLocalController: true,
  isLocallyControlled: true,
  key: 'SpaceBar',
  mapName: '/Game/Maps/Arena',
  mixName: 'CombatMix',
  modifierType: 'DeadZone',
  nodeClassName: 'UE.Sine.Audio',
  nodeId: 'Node_0',
  nodeType: 'WavePlayer',
  outputName: 'Out',
  outputType: 'Audio',
  pawnClass: '/Game/Blueprints/BP_PlayerCharacter',
  playerControllerClass: '/Game/Blueprints/Framework/BP_ArenaPlayerController',
  playerIndex: 1,
  playerName: 'PlayerOne',
  playerStateClass: '/Game/Blueprints/Framework/BP_ArenaPlayerState',
  propertyName: 'Health',
  pushToTalkEnabled: true,
  reliable: true,
  repNotifyFunc: 'OnRep_Health',
  role: 'ROLE_Authority',
  rpcType: 'Server',
  serverAddress: '127.0.0.1',
  sessionName: 'ArenaSession',
  soundClassName: 'SFX',
  soundClassPath: '/Game/Audio/Classes/SC_SFX',
  soundName: 'FootstepConcrete',
  soundPath: '/Game/Audio/Waves/SW_FootstepConcrete',
  sourceNodeId: 'Node_0',
  sourceOutputName: 'Out',
  splitScreenType: 'TwoPlayer_Horizontal',
  targetInputName: 'In',
  targetNodeId: 'Node_1',
  triggerType: 'Pressed',
  voiceEnabled: true,
  // Exactly the fields declared by `VoiceSettings` in handler-session-types.ts.
  voiceSettings: { volume: 0.8, noiseSuppression: true, echoCancellation: true, sampleRate: 48000 },
};

/**
 * Per-action values where one field name legitimately means different things.
 * Keyed `<action>.<field>` and consulted before FIELD_EXAMPLES.
 */
const ACTION_EXAMPLES: Readonly<Record<string, JsonValue>> = {
  'create_submix_effect.effectType': 'SubmixEffectReverb',
  'add_metasound_output.outputType': 'Audio',
  'add_metasound_input.inputType': 'Float',
  'add_legacy_axis_mapping.key': 'W',
  'remove_legacy_axis_mapping.key': 'W',
  'create_attenuation_settings.name': 'ATT_Footstep',
  'set_sound_attenuation.name': 'ATT_Footstep',
  'create_dialogue_voice.name': 'DV_Narrator',
  'create_dialogue_wave.name': 'DW_Intro',
  'create_reverb_effect.name': 'RE_Cavern',
  'create_source_effect_chain.name': 'SEC_Weapon',
  'create_submix_effect.name': 'SME_Reverb',
  'create_sound_class.name': 'SC_SFX',
  'create_sound_mix.name': 'SM_Combat',
  'create_game_state.name': 'BP_ArenaGameState',
  'create_player_controller.name': 'BP_ArenaPlayerController',
  'create_player_state.name': 'BP_ArenaPlayerState',
  'create_game_instance.name': 'BP_ArenaGameInstance',
  'create_hud_class.name': 'BP_ArenaHUD',
  'create_input_mapping_context.name': 'IMC_Default',
  'create_input_mapping_context.path': '/Game/Input',
  // The two flat `*Info` payloads report the resolved asset, so `assetClass`
  // and `type` name a different Unreal class in each and cannot share a value.
  'get_audio_info.assetClass': 'SoundCue',
  'get_audio_info.type': 'SoundCue',
  'get_input_info.assetClass': 'InputAction',
  'get_input_info.assetName': 'IA_Jump',
  'get_input_info.type': 'InputAction',
};

/**
 * Reflection-boundary examples, keyed by the name the plugin actually sends.
 * Every inner field is one the plugin is observed to write, so nothing here is
 * invented:
 *   networkingInfo    - Private/Domains/Networking/...HandlersInfo.cpp writes
 *                       role / remoteRole / hasAuthority.
 *   gameFrameworkInfo - Private/Domains/GameFramework/...HandlersInfo.cpp writes
 *                       defaultPawnClass / playerControllerClass / gameStateClass /
 *                       playerStateClass / hudClass / gameModeClass / source.
 *   sessionsInfo      - Private/Domains/Sessions/...HandlersInfo.cpp writes all
 *                       twelve fields below unconditionally, including the
 *                       nested `activeVoiceChannels` array. The values are the
 *                       state a single-player editor session really reports.
 *
 * The audio and Enhanced Input payloads are deliberately absent: both are
 * written FLAT, so their example values come from ACTION_EXAMPLES rather than
 * from a wrapper object that no transport sends.
 */
const REFLECTED_EXAMPLES: Readonly<Record<string, JsonObject>> = {
  networkingInfo: {
    role: 'ROLE_Authority',
    remoteRole: 'ROLE_SimulatedProxy',
    hasAuthority: true,
  },
  gameFrameworkInfo: {
    gameModeClass: '/Game/Blueprints/Framework/BP_ArenaGameMode_C',
    defaultPawnClass: '/Game/Blueprints/BP_PlayerCharacter_C',
    playerControllerClass: '/Game/Blueprints/Framework/BP_ArenaPlayerController_C',
    source: 'levelDefault',
  },
  sessionsInfo: {
    localPlayerCount: 1,
    inPlaySession: false,
    currentSessionName: 'None',
    isLANMatch: false,
    maxPlayers: 0,
    currentPlayers: 1,
    splitScreenEnabled: false,
    splitScreenType: 'None',
    voiceChatEnabled: false,
    isHosting: false,
    connectedServerAddress: '',
    activeVoiceChannels: [],
  },
};

function valueFor(action: string, field: string, family: string): JsonValue {
  const override = ACTION_EXAMPLES[`${action}.${field}`];
  if (override !== undefined) return override;

  const defaults = FAMILY_DEFAULTS[family];
  if (defaults !== undefined) {
    if (field === 'name') return defaults.name;
    if (field === 'path') return defaults.path;
    if (field === 'assetPath') return defaults.assetPath;
  }

  const reflected = REFLECTED_EXAMPLES[field];
  if (reflected !== undefined) return { ...reflected };

  const known = FIELD_EXAMPLES[field];
  if (known !== undefined) return known;
  return `Example${field.charAt(0).toUpperCase()}${field.slice(1)}`;
}

/** The first example input: the routed action plus every required parameter. */
export function buildExampleInput(
  action: string,
  family: string,
  required: readonly string[],
  requiredOneOf?: readonly string[],
): JsonObject {
  const input: Record<string, JsonValue> = { action };
  for (const field of required) {
    if (field === 'action') continue;
    input[field] = valueFor(action, field, family);
  }
  // A requiredOneOf group is part of the contract, so the first example must
  // satisfy it. Include the first declared member unless a required field (or
  // the routed action) already covers the group.
  for (const field of requiredOneOf ?? []) {
    if (field === 'action' || Object.hasOwn(input, field)) continue;
    input[field] = valueFor(action, field, family);
    break;
  }
  return input;
}

/** The first example output: the honest envelope plus every required result. */
export function buildExampleOutput(
  action: string,
  family: string,
  outputRequired: readonly string[],
): JsonObject {
  const output: Record<string, JsonValue> = {
    success: true,
    message: `${action} completed successfully.`,
  };
  for (const field of outputRequired) {
    if (field === 'success' || field === 'message') continue;
    output[field] = valueFor(action, field, family);
  }
  return output;
}
