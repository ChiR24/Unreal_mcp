export const DRAFT_2020_12_SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema' as const;

export const UNREAL_RELEASE_CHANNELS = ['stable', 'preview'] as const;
export const EDITOR_STATES = ['edit', 'pie', 'simulate'] as const;
export const BEHAVIOR_EFFECTS = ['read', 'write', 'destructive'] as const;
export const IDEMPOTENCY_CLASSES = ['non-idempotent', 'idempotent', 'idempotency-key'] as const;
export const POLICY_SCOPES = ['read', 'write', 'destructive', 'admin'] as const;
export const CONSENT_MODES = ['none', 'explicit', 'elevated'] as const;
export const DATA_ACCESS_CLASSES = ['none', 'project-read', 'project-write', 'engine-read'] as const;
export const LATENCY_CLASSES = ['instant', 'interactive', 'long-running'] as const;
export const RESOURCE_CLASSES = ['low', 'medium', 'high'] as const;
export const DISPATCH_MODES = ['tool', 'action', 'local'] as const;
export const NORMALIZATION_CLASSES = [
  'A_TRUE_DUPLICATE',
  'B_ALIAS',
  'C_SAME_VERB_DIFFERENT_TARGET',
  'D_COMPOSITE',
  'E_PRESET_WORKFLOW',
  'F_OBSOLETE_VERSION_SPECIFIC'
] as const;
export const NORMALIZATION_DISPOSITIONS = [
  'canonical',
  'merge',
  'alias',
  'retain',
  'decompose',
  'preset',
  'remove'
] as const;
export const DEPRECATION_STATUSES = ['active', 'deprecated', 'removed'] as const;
export const HASH_ALGORITHM = 'sha256' as const;
