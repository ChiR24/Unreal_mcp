/**
 * Metadata records: get_metadata, set_metadata.
 *
 * get_metadata reads sequence asset metadata via the manage_sequence native
 * dispatch (SequenceHandlersAssetLibrary.cpp). set_metadata is the sole
 * CROSS-PARENT action: TS sequence-asset-actions.ts:74-77 routes it to the
 * `set_metadata` tool (Level domain), NOT manage_sequence. The native
 * manage_sequence dispatch does NOT implement set_metadata; routing it through
 * manage_sequence would fall through to NOT_IMPLEMENTED.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'metadata';
const D = 'sequence';

export const METADATA_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.get_metadata', action: 'get_metadata', family: F, domain: D,
    summary: 'Read metadata key-value pairs from a Level Sequence asset.',
    whenToUse: ['Sequence asset metadata must be inspected.'],
    whenNotToUse: ['Metadata is being written rather than read.'],
    inputProps: { action: P.action, path: P.path },
    required: ['action', 'path'],
    // Native HandleSequenceGetMetadata (SequenceHandlersAssetLibrary.cpp:196-229)
    // emits path/name/class on success — NOT a metadata object. Declared exactly.
    outputProps: {
      path: { type: 'string', description: 'Resolved sequence asset path.' },
      name: { type: 'string', description: 'Sequence asset name.' },
      class: { type: 'string', description: 'Sequence asset class name.' },
    },
    outputRequired: ['path', 'name', 'class'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_metadata', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, path: '/Game/Cinematics/SEQ_Master', name: 'SEQ_Master', class: 'LevelSequence' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Metadata read is a distinct Sequencer query with unique asset target.',
  }),
  buildRecord({
    id: 'sequence.set_metadata', action: 'set_metadata', family: F, domain: D,
    summary: 'Write metadata key-value pairs to a Level Sequence asset via the cross-parent Level domain.',
    whenToUse: ['Sequence asset metadata must be written.'],
    whenNotToUse: ['The target is not a /Game asset path.'],
    inputProps: { action: P.action, path: P.path, metadata: P.metadata },
    required: ['action', 'path', 'metadata'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    dispatchMode: 'action',
    exampleInput: { action: 'set_metadata', path: '/Game/Cinematics/SEQ_Master', metadata: { author: 'MCP' } },
    exampleOutput: { success: true, message: 'Metadata set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'Cross-parent metadata write: TS routes to set_metadata tool (Level domain), bypassing manage_sequence native dispatch.',
    aliases: ['sequence.metadata.set'],
  }),
];
