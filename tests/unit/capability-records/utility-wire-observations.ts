/**
 * tests/unit/capability-records/utility-wire-observations.ts
 *
 * OBSERVED NATIVE WIRE OUTPUT for the five utility `*Info` capabilities.
 *
 * Every entry below was read out of the shipping plugin sources, not inferred
 * from the record. Each key names the exact C++ file and the exact
 * `Set<T>Field(TEXT("<key>")` construction that emits it, and
 * `utility-wire-contract.test.ts` re-reads those files and fails if the
 * construction is no longer there. The fixture therefore cannot drift away from
 * the plugin, and it cannot be fabricated: a key nobody writes has no provenance
 * line to match.
 *
 * WHAT "THE WIRE" MEANS HERE
 * Both transports carry the handler's `Result` object:
 *   - native `/mcp` puts it in `structuredContent` verbatim
 *     (`MCP/Protocol/McpJsonRpc.cpp` -> `Result->SetObjectField(TEXT("structuredContent"), Data)`)
 *   - the WebSocket bridge nests it under `result` in the automation frame
 *     (`Transport/Connection/McpConnectionManagerResponses.cpp`)
 * so the domain payload keys named below are what a client actually observes on
 * both surfaces. The canonical output schema describes that payload.
 *
 * ENVELOPE FIELDS (`success`, `message`) ARE NOT LISTED as domain keys. Their
 * requiredness is owned by the catalog-wide envelope contract and by
 * `utility-contract-honesty.test.ts`, not by this fixture.
 *
 * `success` IS tracked separately, via `successSourceFile`. T29-B5 was that
 * `get_sessions_info` and `get_input_info` passed success ONLY as the
 * `SendAutomationResponse(..., true, ...)` envelope argument, so it never
 * reached native `structuredContent` (which carries the handler `Result`
 * verbatim) while the three sibling handlers wrote it into `Result`. All five
 * now write it, and the gate below re-reads each file to keep it that way.
 */

/** JSON type produced by each Unreal setter. */
export type WireType = 'string' | 'number' | 'boolean' | 'object' | 'array';

const SETTER_BY_TYPE: Readonly<Record<WireType, string>> = Object.freeze({
  string: 'SetStringField',
  number: 'SetNumberField',
  boolean: 'SetBoolField',
  object: 'SetObjectField',
  array: 'SetArrayField',
});

/** The `Set<T>Field(TEXT("<key>")` fragment that must exist in `sourceFile`. */
export function provenanceFragment(key: string, type: WireType): string {
  return `${SETTER_BY_TYPE[type]}(TEXT("${key}")`;
}

export interface WireField {
  readonly key: string;
  readonly type: WireType;
  /**
   * `true` when every success path of the handler emits the key, so the record
   * may require it. `false` when the key is gated on the resolved asset class
   * or an optional engine value, so the record must declare it WITHOUT
   * requiring it.
   */
  readonly always: boolean;
  /** Plugin-relative path of the file that emits it. */
  readonly sourceFile: string;
}

export interface WireObservation {
  readonly capabilityId: string;
  readonly action: string;
  /**
   * File that matches the action string and routes it. Three of the five
   * domains route in their dispatch file and emit from a separate `...Info` /
   * `...RuntimeQueries` file, so this is deliberately independent of the
   * per-field `sourceFile`.
   */
  readonly routingFile: string;
  /** File that must write `success` INTO the handler `Result` (T29-B5 gate). */
  readonly successSourceFile: string;
  readonly fields: readonly WireField[];
}

const PLUGIN = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private';
const NETWORKING = `${PLUGIN}/Domains/Networking/McpAutomationBridge_NetworkingHandlersInfo.cpp`;
const NETWORKING_ROUTING = `${PLUGIN}/Domains/Networking/McpAutomationBridge_NetworkingHandlers.cpp`;
const GAME_FRAMEWORK = `${PLUGIN}/Domains/GameFramework/McpAutomationBridge_GameFrameworkHandlersInfo.cpp`;
const SESSIONS = `${PLUGIN}/Domains/Sessions/McpAutomationBridge_SessionsHandlersInfo.cpp`;
const SESSIONS_ROUTING = `${PLUGIN}/Domains/Sessions/McpAutomationBridge_SessionsHandlers.cpp`;
const AUDIO = `${PLUGIN}/Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersInfo.cpp`;
const INPUT = `${PLUGIN}/Domains/Input/McpAutomationBridge_InputHandlersRuntimeQueries.cpp`;
const INPUT_ROUTING = `${PLUGIN}/Domains/Input/McpAutomationBridge_InputHandlers.cpp`;
const VERIFICATION = `${PLUGIN}/Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h`;

const field = (
  key: string,
  type: WireType,
  always: boolean,
  sourceFile: string,
): WireField => ({ key, type, always, sourceFile });

/**
 * CONTROL. `Context.ResultJson->SetObjectField(TEXT("networkingInfo"), ...)` is
 * emitted on every success path, after the blueprint/actor branch has filled
 * `NetworkingInfo` with role / remoteRole / hasAuthority. This record already
 * agrees with the wire and must stay byte-identical through the repair.
 */
const NETWORKING_INFO: WireObservation = {
  capabilityId: 'manage_networking.get_networking_info',
  action: 'get_networking_info',
  routingFile: NETWORKING_ROUTING,
  successSourceFile: NETWORKING,
  fields: [field('networkingInfo', 'object', true, NETWORKING)],
};

/** `Response->SetObjectField(TEXT("gameFrameworkInfo"), InfoObj)` — never `frameworkInfo`. */
const GAME_FRAMEWORK_INFO: WireObservation = {
  capabilityId: 'manage_networking.get_game_framework_info',
  action: 'get_game_framework_info',
  routingFile: GAME_FRAMEWORK,
  successSourceFile: GAME_FRAMEWORK,
  fields: [field('gameFrameworkInfo', 'object', true, GAME_FRAMEWORK)],
};

/**
 * `ResponseJson->SetObjectField(TEXT("sessionsInfo"), SessionsInfo)` — an
 * OBJECT, not the `sessions` array the record declared. `activeVoiceChannels`
 * is an array INSIDE that object (`SessionsInfo->SetArrayField`), not a
 * top-level sibling, so it is part of the reflection boundary rather than a
 * separate output property.
 */
const SESSIONS_INFO: WireObservation = {
  capabilityId: 'manage_networking.get_sessions_info',
  action: 'get_sessions_info',
  routingFile: SESSIONS_ROUTING,
  successSourceFile: SESSIONS,
  fields: [field('sessionsInfo', 'object', true, SESSIONS)],
};

/**
 * FLAT. `HandleAudioInfoActions` writes straight onto `Response`; there is no
 * `audioInfo` wrapper anywhere in the file. `type` is always emitted because
 * the final `else` branch sets it to "Unknown"; everything else is gated on the
 * resolved USoundBase subclass.
 */
const AUDIO_INFO: WireObservation = {
  capabilityId: 'manage_audio.get_audio_info',
  action: 'get_audio_info',
  routingFile: AUDIO,
  successSourceFile: AUDIO,
  fields: [
    field('assetPath', 'string', true, AUDIO),
    field('assetClass', 'string', true, AUDIO),
    field('type', 'string', true, AUDIO),
    field('duration', 'number', false, AUDIO),
    field('nodeCount', 'number', false, AUDIO),
    field('attenuationPath', 'string', false, AUDIO),
    field('sampleRate', 'number', false, AUDIO),
    field('numChannels', 'number', false, AUDIO),
    field('volume', 'number', false, AUDIO),
    field('pitch', 'number', false, AUDIO),
    field('parentClass', 'string', false, AUDIO),
    field('modifierCount', 'number', false, AUDIO),
    field('falloffDistance', 'number', false, AUDIO),
    field('spatialize', 'boolean', false, AUDIO),
  ],
};

/**
 * FLAT. `HandleGetInputInfo` writes onto `Result` and then calls
 * `McpHandlerUtils::AddVerification(Result, Asset)`, which for a non-actor
 * UObject runs `AddAssetVerification` and unconditionally re-stamps assetPath /
 * assetName / assetClass and adds `existsAfter`. There is no `inputInfo`
 * wrapper. `type` is gated: assets that are neither UInputAction nor
 * UInputMappingContext get no `type` at all. `valueType` is a STRING
 * (`FString::FromInt`), not a number.
 */
const INPUT_INFO: WireObservation = {
  capabilityId: 'manage_networking.get_input_info',
  action: 'get_input_info',
  routingFile: INPUT_ROUTING,
  successSourceFile: INPUT,
  fields: [
    field('assetPath', 'string', true, VERIFICATION),
    field('assetClass', 'string', true, VERIFICATION),
    field('assetName', 'string', true, VERIFICATION),
    field('existsAfter', 'boolean', true, VERIFICATION),
    field('type', 'string', false, INPUT),
    field('valueType', 'string', false, INPUT),
    field('consumeInput', 'boolean', false, INPUT),
    field('mappingCount', 'number', false, INPUT),
  ],
};

/** The four escalated capabilities plus the untouched networking control. */
export const WIRE_OBSERVATIONS: readonly WireObservation[] = Object.freeze([
  NETWORKING_INFO,
  GAME_FRAMEWORK_INFO,
  SESSIONS_INFO,
  AUDIO_INFO,
  INPUT_INFO,
]);

/** `success` / `message` come from the response envelope, not the domain payload. */
export const ENVELOPE_FIELDS: ReadonlySet<string> = new Set(['success', 'message', 'details']);

/** The control record, which the repair must leave exactly as it is. */
export const CONTROL_CAPABILITY_ID = NETWORKING_INFO.capabilityId;
