/**
 * Semantic output-field glossary for the utility capability records.
 *
 * WHY THIS FILE EXISTS
 * `helpers.ts` used to build the OUTPUT schema with the same name-keyed
 * inference it uses for INPUTS. That inference has no entry for `success`, so
 * all 127 utility records shipped `success: { type: 'string' }` while both
 * transports write a boolean (`SetBoolField(TEXT("success"), true)` in
 * Private/Domains/{Networking,AudioAuthoring,GameFramework}/...HandlersInfo.cpp)
 * and every example declared `success: true`. The same fallback also stamped
 * `description: <fieldName>`, producing 294 degenerate descriptions.
 *
 * The fix is deliberately NOT "add success to the shared boolean set": that set
 * is keyed on bare field names and is shared with the input schema, so adding
 * `success` there would silently retype any future input named `success`. The
 * output envelope is therefore built explicitly here, and only the output side
 * consults this glossary. `helpers.ts` keeps its input inference untouched.
 *
 * The `success` / `message` wording is copied verbatim from the five already
 * correct builders (`records/core/builder.ts#outputSchema`) so the utility lane
 * is indistinguishable from them downstream.
 */
import type { JsonObject } from '../../index.js';

/** An intentionally open object whose interior is arbitrary Unreal reflection data. */
const REFLECTION_BOUNDARY = 'x-unreal-reflection-boundary';

function reflected(description: string): JsonObject {
  return {
    type: 'object',
    description,
    additionalProperties: true,
    [REFLECTION_BOUNDARY]: true,
  };
}

const str = (description: string): JsonObject => ({ type: 'string', description });
const num = (description: string): JsonObject => ({ type: 'number', description });
const bool = (description: string): JsonObject => ({ type: 'boolean', description });

/** Verbatim from `records/core/builder.ts` — do not reword independently. */
export const OUTPUT_HEADER: Readonly<Record<string, JsonObject>> = Object.freeze({
  success: { type: 'boolean', description: 'Whether the action succeeded.' },
  message: { type: 'string', description: 'Human-readable result message.' },
});

/**
 * Every non-header output name produced by the utility data files, with the
 * type the bridge actually returns and prose a caller can act on.
 *
 * Three entries correct a type the name inference got wrong: `hasAuthority`,
 * `isLocallyControlled` and `isLocalController` were emitted as `string` but
 * are written with `SetBoolField` by the networking domain. Correcting them
 * NARROWS the contract (boolean is a strict subset of the previous string), so
 * this is a strengthening, never a weakening to make an example pass.
 *
 * The four `*Info` wrapper names this table used to carry (`frameworkInfo`,
 * `audioInfo`, `inputInfo` and the `sessions` array) described keys no
 * transport ever sent. The plugin nests the GameMode payload under
 * `gameFrameworkInfo`, nests session state under a `sessionsInfo` OBJECT, and
 * writes the audio and Enhanced Input payloads FLAT with no wrapper at all.
 * The flat scalars below are those payloads; `tests/unit/capability-records/
 * utility-wire-contract.test.ts` re-derives every name and type from
 * the shipping C++ and fails if either side drifts.
 */
export const UTILITY_OUTPUT_FIELDS: Readonly<Record<string, JsonObject>> = Object.freeze({
  actorName: str('Name of the actor that was spawned or acted upon.'),
  assetClass: str('Unreal class name of the asset that was inspected, such as SoundCue or InputAction.'),
  assetName: str('Object name of the asset that was inspected, without its package path.'),
  assetPath: str('Full /Game path of the asset that was created, modified or inspected.'),
  attenuationPath: str('Path of the Sound Attenuation asset referenced by the inspected Sound Cue.'),
  componentName: str('Name of the audio component that was created or spawned.'),
  consumeInput: bool('Whether the Input Action consumes the input it handles.'),
  duration: num('Length of the inspected sound in seconds.'),
  existsAfter: bool('Whether the asset still resolved after the action completed.'),
  falloffDistance: num('Distance in centimetres over which the attenuation falls off.'),
  functionName: str('Name of the Blueprint RPC function that was created.'),
  gameFrameworkInfo: reflected('Game Framework class assignments and rule state read from the GameMode.'),
  hasAuthority: bool('Whether the actor currently holds network authority.'),
  isLocalController: bool('Whether the controller possessing this actor is the local controller.'),
  isLocallyControlled: bool('Whether the actor is controlled by the local player.'),
  mappingCount: num('Number of key mappings declared by the inspected Input Mapping Context.'),
  modifierCount: num('Number of Sound Class effects declared by the inspected Sound Mix.'),
  networkingInfo: reflected('Replication, relevancy and network role state read from the Blueprint or actor.'),
  nodeCount: num('Number of nodes in the inspected Sound Cue graph.'),
  nodeId: str('Identifier of the graph node that was added.'),
  numChannels: num('Number of audio channels in the inspected Sound Wave.'),
  parentClass: str('Path of the parent Sound Class the inspected Sound Class inherits from.'),
  pitch: num('Pitch multiplier declared by the inspected Sound Class.'),
  playerIndex: num('Index assigned to the local player that was added.'),
  role: str('Network role of the actor, such as ROLE_Authority.'),
  sampleRate: num('Sample rate of the inspected Sound Wave for the current platform.'),
  serverAddress: str('Address of the LAN server that was hosted or joined.'),
  sessionName: str('Name of the online session that was created or joined.'),
  sessionsInfo: reflected('Local and online session state, including player counts, split-screen, voice and hosting flags.'),
  spatialize: bool('Whether the inspected Sound Attenuation spatializes its source.'),
  type: str('Kind of asset the bridge resolved, such as SoundCue, SoundWave or InputAction.'),
  valueType: str('Enhanced Input value type of the Input Action, reported as its numeric enum index.'),
  volume: num('Volume multiplier declared by the inspected Sound Class.'),
});

/**
 * Resolve one output property. Unknown names fall back to a string with prose
 * rather than throwing, so a new data-file field can never break module load;
 * `utility-contract-honesty.test.ts` is what fails instead, naming the
 * field so it gets a real entry above.
 */
export function outputProperty(name: string): JsonObject {
  const header = OUTPUT_HEADER[name];
  if (header !== undefined) return { ...header };
  const known = UTILITY_OUTPUT_FIELDS[name];
  if (known !== undefined) return { ...known };
  return { type: 'string', description: `Value reported by the bridge for ${name}.` };
}
