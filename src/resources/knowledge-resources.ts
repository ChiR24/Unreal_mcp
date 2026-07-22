// src/resources/knowledge-resources.ts
// Task 31: template-backed reads for stable Unreal knowledge and for normalized
// object / asset reference handles. Knowledge is static and keyed by engine
// version + topic (no live editor). Object / asset handles normalize the path
// (rejecting traversal and host paths) and then check existence across the
// injected game-thread boundary; an unavailable editor yields a typed error and
// never mutates.

import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';
import { isRecord } from '../utils/validation/type-guards.js';
import {
  INITIAL_REVISION,
  type RevisionProvider,
  type RevisionedResource,
} from '../server/mcp-primitives/resource-revision.js';
import { RESOURCE_ERROR_CODES, ResourceError, normalizeContentPath } from './resource-errors.js';

export interface KnowledgeData {
  readonly engineVersion: string;
  readonly topic: string;
  readonly title: string;
  readonly summary: string;
  readonly references: readonly string[];
}

export interface ObjectHandleData {
  readonly kind: 'object' | 'asset';
  readonly path: string;
  readonly exists: boolean;
}

/** Injected game-thread boundary for asset/object existence checks. */
export interface AssetLookupSource {
  isAvailable(): Promise<boolean>;
  objectExists(path: string): Promise<boolean>;
  assetExists(path: string): Promise<boolean>;
}

interface KnowledgeEntry {
  readonly title: string;
  readonly summary: string;
  readonly references: readonly string[];
}

const KNOWLEDGE: Readonly<Record<string, KnowledgeEntry>> = {
  paths: {
    title: 'Content Paths',
    summary: 'Asset paths are addressed under UE mount roots (/Game, /Engine, /Script, /Temp, /Niagara). /Content maps to /Game.',
    references: ['ue://project', 'ue://asset/{assetPath}'],
  },
  safety: {
    title: 'Editor Safety',
    summary: 'Hazardous editor operations (save, load, delete) run through safe wrappers on the game thread; reads never mutate state.',
    references: ['ue://editor'],
  },
  gateway: {
    title: 'Gateway Surface',
    summary: 'A single unreal gateway tool exposes search, describe, execute, and configure operations; canonical tools are internal.',
    references: ['ue://capability/catalog', 'ue://capability/{capabilityId}'],
  },
  transports: {
    title: 'Transports',
    summary: 'Two transports exist: the TypeScript stdio bridge and the native /mcp HTTP/SSE server. Both are loopback-first.',
    references: ['ue://capability/catalog'],
  },
  resources: {
    title: 'Resource Surface',
    summary: 'Read-only resources return bounded, redacted data tagged with a monotonic revision; no host paths or secrets are exposed.',
    references: ['ue://capability/catalog', 'ue://project', 'ue://selection'],
  },
};

const VERSION_TOKEN = /^[0-9A-Za-z._-]{1,32}$/u;

export class KnowledgeResources {
  constructor(
    private readonly lookup: AssetLookupSource,
    private readonly revisions: RevisionProvider,
  ) {}

  readKnowledge(uri: string, engineVersion: string, topic: string): RevisionedResource<KnowledgeData> {
    const version = this.sanitizeVersion(uri, engineVersion);
    const entry = KNOWLEDGE[topic.toLowerCase()];
    if (entry === undefined) {
      throw new ResourceError(RESOURCE_ERROR_CODES.NOT_FOUND, uri, `Unknown knowledge topic: ${topic}`);
    }
    // Knowledge is stable and versioned by the URI itself, so it carries the
    // fixed initial revision rather than a live subscription revision.
    return {
      uri,
      revision: INITIAL_REVISION,
      data: { engineVersion: version, topic: topic.toLowerCase(), ...entry },
    };
  }

  async readObject(uri: string, rawPath: string): Promise<RevisionedResource<ObjectHandleData>> {
    return this.readHandle(uri, rawPath, 'object');
  }

  async readAsset(uri: string, rawPath: string): Promise<RevisionedResource<ObjectHandleData>> {
    return this.readHandle(uri, rawPath, 'asset');
  }

  private async readHandle(uri: string, rawPath: string, kind: 'object' | 'asset'): Promise<RevisionedResource<ObjectHandleData>> {
    const path = normalizeContentPath(uri, rawPath);
    if (!(await this.lookup.isAvailable())) {
      throw new ResourceError(RESOURCE_ERROR_CODES.UNAVAILABLE, uri, `${kind} reference resolution requires a connected Unreal Editor`);
    }
    const exists = kind === 'asset' ? await this.lookup.assetExists(path) : await this.lookup.objectExists(path);
    return {
      uri,
      revision: this.revisions.currentRevision('ue://asset-registry'),
      data: { kind, path, exists },
    };
  }

  private sanitizeVersion(uri: string, engineVersion: string): string {
    let decoded: string;
    try {
      decoded = decodeURIComponent(engineVersion);
    } catch {
      throw new ResourceError(RESOURCE_ERROR_CODES.INVALID_URI, uri, 'Malformed engine version token');
    }
    if (!VERSION_TOKEN.test(decoded)) {
      throw new ResourceError(RESOURCE_ERROR_CODES.INVALID_URI, uri, `Invalid engine version token: ${decoded}`);
    }
    return decoded;
  }
}

/**
 * Default asset lookup over the automation bridge using the `asset_exists`
 * action. Object resolution is best-effort against the same action; Task 37
 * refines live object resolution. Never throws — availability gating is the
 * caller's responsibility.
 */
export class BridgeAssetLookupSource implements AssetLookupSource {
  constructor(
    private readonly automationBridge: AutomationRequestBridge | undefined,
    private readonly ensureConnected: () => Promise<boolean>,
  ) {}

  async isAvailable(): Promise<boolean> {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return false;
    }
    return this.ensureConnected();
  }

  async objectExists(path: string): Promise<boolean> {
    return this.exists(path);
  }

  async assetExists(path: string): Promise<boolean> {
    return this.exists(path);
  }

  private async exists(path: string): Promise<boolean> {
    if (!this.automationBridge) {
      return false;
    }
    const response = await this.automationBridge.sendAutomationRequest('asset_exists', { asset_path: path });
    if (!isRecord(response)) {
      return false;
    }
    const result = isRecord(response.result) ? response.result : response;
    return response.success !== false && result.exists === true;
  }
}
