export const DRAFT_SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema';

export function validCapabilitySource() {
  return {
    id: 'asset.delete',
    aliases: ['asset.remove'],
    legacyIds: [
      { tool: 'manage_asset', action: 'delete_asset' }
    ],
    discovery: {
      domain: 'asset',
      family: 'lifecycle',
      topics: ['delete', 'remove'],
      summary: 'Delete one Unreal asset after explicit confirmation.',
      whenToUse: ['A known project asset must be permanently removed.'],
      whenNotToUse: ['The caller only wants to unload or hide an asset.']
    },
    schemas: {
      input: {
        $schema: DRAFT_SCHEMA_URI,
        type: 'object',
        properties: {
          assetPath: {
            type: 'string',
            minLength: 1,
            description: 'Canonical /Game asset path.'
          }
        },
        required: ['assetPath'],
        additionalProperties: false
      },
      output: {
        $schema: DRAFT_SCHEMA_URI,
        type: 'object',
        properties: {
          success: { type: 'boolean' },
          deletedPath: { type: 'string' }
        },
        required: ['success', 'deletedPath'],
        additionalProperties: false
      }
    },
    examples: [
      {
        title: 'Delete one confirmed asset',
        input: { assetPath: '/Game/MCPTest/Disposable' },
        output: { success: true, deletedPath: '/Game/MCPTest/Disposable' }
      }
    ],
    availability: {
      unreal: {
        min: { major: 5, minor: 0, patch: 0, channel: 'stable' },
        max: { major: 5, minor: 8, patch: 0, channel: 'preview', preview: 1 }
      },
      requiredPlugins: ['EditorScriptingUtilities'],
      editorStates: ['edit']
    },
    behavior: {
      effect: 'destructive',
      idempotency: 'idempotent',
      longRunning: true,
      safeToRetry: false,
      supportsPreview: true,
      supportsUndo: false
    },
    policy: {
      requiredScope: 'destructive',
      consent: 'elevated',
      dataAccess: 'project-write'
    },
    cost: {
      latency: 'long-running',
      resources: 'high'
    },
    routing: {
      parentTool: 'manage_asset',
      dispatchAction: 'delete_asset',
      dispatchMode: 'tool'
    },
    normalization: {
      class: 'C_SAME_VERB_DIFFERENT_TARGET',
      disposition: 'retain',
      rationale: 'Asset deletion has distinct target, policy, and rollback semantics.'
    },
    deprecation: {
      status: 'active'
    }
  };
}

export function secondCapabilitySource() {
  const source = validCapabilitySource();
  return {
    ...source,
    id: 'actor.delete',
    aliases: ['actor.remove'],
    legacyIds: [
      { tool: 'control_actor', action: 'delete_actor' }
    ],
    discovery: {
      ...source.discovery,
      domain: 'actor',
      summary: 'Delete one actor from the current level.'
    },
    routing: {
      parentTool: 'control_actor',
      dispatchAction: 'delete_actor',
      dispatchMode: 'tool'
    }
  };
}
