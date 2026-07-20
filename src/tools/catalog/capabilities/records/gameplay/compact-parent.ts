import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

export type GameplayActionMode = 'authoring' | 'read' | 'runtime';

export type GameplayActionSpec = {
  readonly action: string;
  readonly mode: GameplayActionMode;
  readonly summary: string;
  readonly plugins?: readonly string[];
};

export type GameplayParentSpec = {
  readonly parentTool: string;
  readonly family: string;
  readonly actions: readonly GameplayActionSpec[];
};

export function buildGameplayParent(spec: GameplayParentSpec): readonly CapabilityRecordSource[] {
  return spec.actions.map((entry) => {
    const isRead = entry.mode === 'read';
    const isRuntime = entry.mode === 'runtime';
    return buildRecord({
      parentTool: spec.parentTool,
      id: `${spec.parentTool}.${entry.action}`,
      action: entry.action,
      family: spec.family,
      summary: entry.summary,
      whenToUse: [`Use the leaf-backed ${entry.action} capability.`],
      whenNotToUse: ['Do not substitute a similarly named action with different semantics.'],
      inputProps: {
        action: P.action,
        assetPath: P.assetPath,
        blueprintPath: P.blueprintPath,
        actorName: P.actorName,
        name: P.name,
        path: P.path,
        properties: P.properties,
      },
      required: ['action'],
      outputProps: isRuntime ? { actorName: P.actorName } : { assetPath: P.assetPath },
      outputRequired: [],
      effect: isRead ? 'read' : 'write',
      behavior: isRuntime ? { supportsUndo: false } : undefined,
      latency: 'interactive',
      resources: 'medium',
      plugins: entry.plugins,
      editorStates: isRuntime ? ['pie', 'simulate'] : ['edit'],
      exampleInput: { assetPath: '/Game/Example' },
      exampleOutput: { success: true, message: `${entry.action} handled` },
    });
  });
}
