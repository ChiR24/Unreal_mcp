// src/resources/resource-catalog.ts
// Task 31: the normalized catalog of NEW version-aware read-only resources and
// templates. This is the TypeScript half of the TS/native normalized fixture;
// the native mirror is
// `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Resources/McpResourceCatalog.h`.
// The plugin source-contract test asserts the two halves agree. The six
// pre-existing resources stay registered in resource-registry.ts; these entries
// are strictly additive so existing clients remain compatible.

export interface ResourceDefinition {
  readonly uri: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}

export interface ResourceTemplateDefinition {
  readonly uriTemplate: string;
  readonly name: string;
  readonly description: string;
  readonly mimeType: string;
}

const JSON_MIME = 'application/json';

/** NEW static resources added by Task 31 (beyond the pre-existing six). */
export const NEW_RESOURCE_DEFINITIONS: readonly ResourceDefinition[] = [
  {
    uri: 'ue://capability/catalog',
    name: 'Capability Catalog',
    description: 'Bounded catalog of gateway capabilities with a monotonic revision',
    mimeType: JSON_MIME,
  },
  {
    uri: 'ue://project',
    name: 'Project',
    description: 'Redacted project name, engine version, and content root',
    mimeType: JSON_MIME,
  },
  {
    uri: 'ue://editor',
    name: 'Editor State',
    description: 'Bounded editor state: PIE status and current level',
    mimeType: JSON_MIME,
  },
  {
    uri: 'ue://selection',
    name: 'Selection',
    description: 'Bounded list of selected actor handles',
    mimeType: JSON_MIME,
  },
  {
    uri: 'ue://state/revisions',
    name: 'Live State Revisions',
    description: 'Current selection, level, asset-registry, and package revision counters',
    mimeType: JSON_MIME,
  },
];

/** Read-only resource templates added by Task 31. */
export const RESOURCE_TEMPLATES: readonly ResourceTemplateDefinition[] = [
  {
    uriTemplate: 'ue://capability/{capabilityId}',
    name: 'Capability Record',
    description: 'Bounded record for one capability (identifier, category, action count; no full schema)',
    mimeType: JSON_MIME,
  },
  {
    uriTemplate: 'ue://knowledge/{engineVersion}/{topic}',
    name: 'Engine Knowledge',
    description: 'Stable Unreal knowledge keyed by engine version and topic',
    mimeType: JSON_MIME,
  },
  {
    uriTemplate: 'ue://object/{objectPath}',
    name: 'Object Reference',
    description: 'Normalized handle for an object at a UE content path',
    mimeType: JSON_MIME,
  },
  {
    uriTemplate: 'ue://asset/{assetPath}',
    name: 'Asset Reference',
    description: 'Normalized handle for an asset at a UE content path',
    mimeType: JSON_MIME,
  },
];
