// src/server/mcp-primitives/completions/completion-sources.ts
// Task 33: the concrete completion candidate source over safe, static, in-memory
// data. Capability ids and legacy migration ids come from the generated
// canonical registry (read-only, via capabilityIndex()); enum values are the
// bounded schema value sets for each template/prompt variable; project handles
// are the class-alias cache keys. It reads NO live editor, opens NO socket, and
// edits NO source. Pools build once and are only read afterward. Task 37 injects
// this into the provider; the pure provider unit tests use their own fixtures.

import { ACTOR_CLASS_ALIASES } from '../../../config/class-aliases.js';
import { capabilityIndex } from '../../gateway/gateway-capability-index.js';
import type {
  CandidateKind,
  CompletionCandidate,
  CompletionCandidateSource,
  CompletionSlot,
} from './completion-types.js';

// Bounded schema value sets for the enum slots. These are the allowed values for
// each template/prompt variable, not a live editor read: the supported UE range,
// the closed knowledge-topic set, and the Task 32 workflow import/output formats.
const ENGINE_VERSIONS = ['5.0', '5.1', '5.2', '5.3', '5.4', '5.5', '5.6', '5.7', '5.8'] as const;
const KNOWLEDGE_TOPICS = ['overview', 'assets', 'actors', 'blueprints', 'levels', 'sequencer', 'niagara', 'physics'] as const;
const ASSET_IMPORT_FORMATS = ['fbx', 'obj', 'gltf', 'png', 'wav'] as const;
const SEQUENCE_OUTPUT_FORMATS = ['png', 'jpeg', 'exr', 'custom'] as const;

function enumValuesFor(slot: CompletionSlot): readonly string[] {
  if (slot.refType === 'ref/resource' && slot.argumentName === 'engineVersion') return ENGINE_VERSIONS;
  if (slot.refType === 'ref/resource' && slot.argumentName === 'topic') return KNOWLEDGE_TOPICS;
  if (slot.refType === 'ref/prompt' && slot.refId === 'asset-import' && slot.argumentName === 'sourceFormat') return ASSET_IMPORT_FORMATS;
  if (slot.refType === 'ref/prompt' && slot.refId === 'sequence-render' && slot.argumentName === 'outputFormat') return SEQUENCE_OUTPUT_FORMATS;
  return [];
}

/**
 * Build the capability pool once from the generated canonical registry: every
 * canonical id (kind capability), plus its single-string aliases and its legacy
 * tool.action pairs (kind legacy-id), each tagged with the canonical id so a
 * capability-scoped slot filters by the session enabled set. Deduplicated and
 * deterministic; the registry records are already id-sorted.
 */
function buildCapabilityPool(): readonly CompletionCandidate[] {
  const pool: CompletionCandidate[] = [];
  const seen = new Set<string>();
  const add = (value: string, kind: CandidateKind, capabilityId: string): void => {
    if (value.length > 0 && !seen.has(value)) {
      seen.add(value);
      pool.push({ value, kind, capabilityId });
    }
  };
  for (const record of capabilityIndex().records) {
    const id = String(record.id);
    add(id, 'capability', id);
    for (const alias of record.aliases) add(String(alias), 'legacy-id', id);
    for (const legacy of record.legacyIds) add(`${String(legacy.tool)}.${String(legacy.action)}`, 'legacy-id', id);
  }
  return pool;
}

/**
 * Safe cached project handles: the friendly class-alias keys (PointLight,
 * StaticMeshActor, ...). Bounded, in-memory, sorted; never a raw filesystem
 * path and never a live editor scan.
 */
function buildProjectHandlePool(): readonly CompletionCandidate[] {
  return Object.keys(ACTOR_CLASS_ALIASES)
    .sort()
    .map((value) => ({ value, kind: 'project-handle' as const }));
}

let capabilityPool: readonly CompletionCandidate[] | undefined;
let projectHandlePool: readonly CompletionCandidate[] | undefined;

/**
 * The concrete completion candidate source over the generated registry, the
 * bounded enum sets, and the class-alias cache. Safe by construction: no editor
 * scan, no socket, no raw filesystem path. Task 37 injects it into the provider.
 */
export function createStaticCompletionSource(): CompletionCandidateSource {
  return {
    capabilityCandidates(): readonly CompletionCandidate[] {
      capabilityPool ??= buildCapabilityPool();
      return capabilityPool;
    },
    enumCandidates(slot: CompletionSlot): readonly CompletionCandidate[] {
      return enumValuesFor(slot).map((value) => ({ value, kind: 'enum' as const }));
    },
    projectHandleCandidates(): readonly CompletionCandidate[] {
      projectHandlePool ??= buildProjectHandlePool();
      return projectHandlePool;
    },
  };
}
