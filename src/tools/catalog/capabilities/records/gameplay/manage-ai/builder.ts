/**
 * Local record builder for manage_ai.
 *
 * Private to manage_ai. Wraps the gameplay buildRecord() with the constants
 * every AI record shares (parent tool, family, cost tier) so each shard
 * declares only the part that varies: the EXACT per-action property set.
 *
 * editorStates: every manage_ai action is an editor-state ('edit') operation.
 * The native AI domain edits assets/Blueprint SCS only, and the navigation
 * actions that need a world resolve it through
 * GEditor->GetEditorWorldContext().World() (NO_WORLD on failure) -- the EDITOR
 * world, not a PIE world. run_behavior_tree / stop_behavior_tree add or remove
 * a Blueprint variable on the controller ASSET; no handler touches
 * GEditor->PlayWorld. manage_ai therefore declares no 'pie'/'simulate' action.
 */
import type { CapabilityRecordSource, JsonObject } from '../../../index.js';
import { buildRecord } from '../helpers.js';
import type { PropertyMap } from '../properties.js';

const T = 'manage_ai';
const F = 'ai';

/** Plugin gates observed in the native AI/Navigation domains. */
export const BT = ['BehaviorTreeEditor'] as const;
export const EQS = ['EnvironmentQueryEditor'] as const;
export const STATE_TREE = ['StateTree'] as const;
export const SMART_OBJECTS = ['SmartObjects'] as const;
export const MASS_AI = ['MassAI'] as const;

export type AiRecordSpec = {
  readonly action: string;
  readonly summary: string;
  readonly use: string;
  readonly avoid: string;
  /** Exact input properties this action reads. Never a parent-wide union. */
  readonly props: PropertyMap;
  readonly required?: readonly string[];
  readonly out?: PropertyMap;
  readonly effect?: 'read' | 'write';
  readonly plugins?: readonly string[];
  readonly example: JsonObject;
  readonly result: string;
  /**
   * Set on a capability authored after the gateway migration. Its legacyIds pair
   * still carries the live routing identity, but the pair never shipped on the
   * pre-gateway surface, so the normalization audit must not count it.
   */
  readonly provenance?: CapabilityRecordSource['normalization']['provenance'];
  readonly rationale?: string;
};

export function aiRecord(spec: AiRecordSpec): CapabilityRecordSource {
  const effect = spec.effect ?? 'write';
  return buildRecord({
    parentTool: T,
    id: `${T}.${spec.action}`,
    action: spec.action,
    family: F,
    summary: spec.summary,
    whenToUse: [spec.use],
    whenNotToUse: [spec.avoid],
    inputProps: spec.props,
    required: ['action', ...(spec.required ?? [])],
    outputProps: spec.out,
    outputRequired: [],
    effect,
    latency: 'interactive',
    resources: 'medium',
    plugins: spec.plugins,
    editorStates: ['edit'],
      exampleInput: { action: spec.action, ...spec.example },
      exampleOutput: { success: true, message: spec.result },
      ...(spec.provenance === undefined
        ? {}
        : { normalizationProvenance: spec.provenance, normalizationRationale: spec.rationale }),
    });
}
