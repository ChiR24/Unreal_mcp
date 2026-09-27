/**
 * Metadata records: get_metadata, set_metadata.
 *
 * get_metadata (Metadata/SequenceHandlersGetMetadata.cpp) reads the package
 * metadata tags, or one tag by key. set_metadata
 * (Metadata/SequenceHandlersSetMetadata.cpp) writes a metadata object or one
 * key and value pair, then saves the sequence.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord, P, SEQ_PLUGINS } from './helpers.js';

const F = 'metadata';
const D = 'sequence';

export const METADATA_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'sequence.get_metadata', action: 'get_metadata', family: F, domain: D,
    summary: 'Read metadata key-value pairs from a Level Sequence asset.',
    whenToUse: ['Sequence asset metadata must be inspected.'],
    whenNotToUse: ['Metadata is being written rather than read.'],
    inputProps: { path: P.path, key: { type: 'string', description: 'Metadata key to look up; the reply then carries found and value.' } },
    required: ['path'],
    // Native HandleSequenceGetMetadata emits path/name/class plus the stored metadata pairs
    // (editor metadata tags written by set_metadata, dogfood #127).
    outputProps: {
      path: { type: 'string', description: 'Resolved sequence asset path.' },
      name: { type: 'string', description: 'Sequence asset name.' },
      class: { type: 'string', description: 'Sequence asset class name.' },
      metadata: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'Stored metadata key-value pairs.' },
      metadataCount: { type: 'number', description: 'Number of stored metadata pairs.' },
      key: { type: 'string', description: 'Requested key, when one was passed.' },
      found: { type: 'boolean', description: 'Whether the requested key exists.' },
      value: { type: 'string', description: 'Value of the requested key.' },
    },
    outputRequired: ['path', 'name', 'class'],
    effect: 'read', latency: 'instant', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'get_metadata', path: '/Game/Cinematics/SEQ_Master' },
    exampleOutput: { success: true, path: '/Game/Cinematics/SEQ_Master', name: 'SEQ_Master', class: 'LevelSequence' },
  }),
  buildRecord({
    id: 'sequence.set_metadata', action: 'set_metadata', family: F, domain: D,
    summary: 'Write metadata key-value pairs to a Level Sequence asset and save it.',
    whenToUse: ['Sequence asset metadata must be written.'],
    whenNotToUse: ['The target is not a /Game asset path.'],
    inputProps: { path: P.path, metadata: P.metadata, key: { type: 'string', description: 'Single metadata key to write (paired with value).' }, value: { type: ['string', 'number', 'boolean'], description: 'Value written under key.' } },
    required: ['path'],
    requiredOneOf: ['metadata', 'key'],
    effect: 'write', behavior: { idempotency: 'idempotent' },
    latency: 'interactive', resources: 'low', plugins: SEQ_PLUGINS,
    exampleInput: { action: 'set_metadata', path: '/Game/Cinematics/SEQ_Master', metadata: { author: 'MCP' } },
    aliases: ['sequence.metadata.set'],
  }),
];
