// src/resources/capability-resources.ts
// Task 31: bounded, revisioned capability catalog and per-capability record.
// The data source is injected (default reads the neutral gateway manifest, the
// single source of truth for the gateway catalog) and the revision is injected
// via the shared RevisionProvider so the catalog participates in the future
// subscription lane (Task 34) without this module owning any mutation. No full
// input schema is ever emitted — only identifiers, categories, and action
// names — so the payload stays bounded.

import { getGatewayManifest } from '../gateway/gateway-manifest.js';
import type { GatewayManifest } from '../gateway/gateway-manifest-types.js';
import {
  type ResourceRevision,
  type RevisionProvider,
  type RevisionedResource,
} from '../server/mcp-primitives/resource-revision.js';
import { RESOURCE_ERROR_CODES, ResourceError } from './resource-errors.js';

const CATALOG_URI = 'ue://capability/catalog';
const MAX_CATALOG_ENTRIES = 64;
const MAX_RECORD_ACTIONS = 200;

export interface CapabilityCatalogEntry {
  readonly id: string;
  readonly category: string | null;
  readonly actionCount: number;
}

export interface CapabilityCatalogData {
  readonly count: number;
  readonly totalCount: number;
  readonly truncated: boolean;
  readonly capabilities: readonly CapabilityCatalogEntry[];
}

export interface CapabilityRecordData {
  readonly id: string;
  readonly category: string | null;
  readonly actionCount: number;
  readonly parameterCount: number;
  readonly truncated: boolean;
  readonly actions: readonly string[];
}

/** Injected bounded capability source (default: the neutral gateway manifest). */
export interface CapabilitySource {
  entries(): readonly CapabilityCatalogEntry[];
  record(id: string): CapabilityRecordData | undefined;
}

export class GatewayManifestCapabilitySource implements CapabilitySource {
  private readonly manifest: GatewayManifest;

  constructor(manifest: GatewayManifest = getGatewayManifest()) {
    this.manifest = manifest;
  }

  entries(): readonly CapabilityCatalogEntry[] {
    return this.manifest.tools.map((tool) => ({
      id: tool.name,
      category: tool.category,
      actionCount: tool.actions.length,
    }));
  }

  record(id: string): CapabilityRecordData | undefined {
    const tool = this.manifest.tools.find((candidate) => candidate.name === id);
    if (tool === undefined) {
      return undefined;
    }
    const actions = tool.actions.slice(0, MAX_RECORD_ACTIONS);
    return {
      id: tool.name,
      category: tool.category,
      actionCount: tool.actions.length,
      parameterCount: tool.parameterNames.length,
      truncated: tool.actions.length > actions.length,
      actions,
    };
  }
}

export class CapabilityResources {
  constructor(
    private readonly source: CapabilitySource,
    private readonly revisions: RevisionProvider,
  ) {}

  private catalogRevision(): ResourceRevision {
    return this.revisions.currentRevision('ue://capability/catalog');
  }

  readCatalog(): RevisionedResource<CapabilityCatalogData> {
    const all = this.source.entries();
    const capped = all.slice(0, MAX_CATALOG_ENTRIES);
    return {
      uri: CATALOG_URI,
      revision: this.catalogRevision(),
      data: {
        count: capped.length,
        totalCount: all.length,
        truncated: all.length > capped.length,
        capabilities: capped,
      },
    };
  }

  readRecord(uri: string, id: string): RevisionedResource<CapabilityRecordData> {
    const record = this.source.record(id);
    if (record === undefined) {
      throw new ResourceError(RESOURCE_ERROR_CODES.NOT_FOUND, uri, `Unknown capability: ${id}`);
    }
    return { uri, revision: this.catalogRevision(), data: record };
  }
}
