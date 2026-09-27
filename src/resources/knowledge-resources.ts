// src/resources/knowledge-resources.ts
// ue://knowledge/{topic}: static Unreal knowledge (no live editor).
// ue://asset/{assetPath}: a normalized content-path handle plus whether it exists.
// The path is normalized first (traversal and host paths are refused); an
// unavailable editor yields a typed error and never mutates.

import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';
import { isRecord } from '../utils/validation/type-guards.js';
import {
  INITIAL_REVISION,
  type RevisionProvider,
  type RevisionedResource,
} from '../server/mcp-primitives/resource-revision.js';
import { RESOURCE_ERROR_CODES, ResourceError, normalizeContentPath } from './resource-errors.js';

export interface KnowledgeData {
  readonly topic: string;
  readonly title: string;
  readonly summary: string;
  readonly references: readonly string[];
}

export interface AssetHandleData {
  readonly path: string;
  readonly exists: boolean;
}

interface KnowledgeEntry {
  readonly title: string;
  readonly summary: string;
  readonly references: readonly string[];
}

/**
 * The knowledge topics that actually resolve. Exported so the completion source
 * derives its suggestions from this table instead of restating them: the two
 * lists had drifted to ZERO overlap, so every suggested topic answered
 * RESOURCE_NOT_FOUND. Deriving makes that divergence unrepresentable.
 */
export const knowledgeTopics = (): readonly string[] => Object.keys(KNOWLEDGE).sort();

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

export class KnowledgeResources {
  constructor(
    private readonly automationBridge: AutomationRequestBridge | undefined,
    private readonly ensureConnected: () => Promise<boolean>,
    private readonly revisions: RevisionProvider,
  ) {}

  readKnowledge(uri: string, topic: string): RevisionedResource<KnowledgeData> {
    const entry = KNOWLEDGE[topic.toLowerCase()];
    if (entry === undefined) {
      throw new ResourceError(RESOURCE_ERROR_CODES.NOT_FOUND, uri, `Unknown knowledge topic: ${topic}`);
    }
    return { uri, revision: INITIAL_REVISION, data: { topic: topic.toLowerCase(), ...entry } };
  }

  async readAsset(uri: string, rawPath: string): Promise<RevisionedResource<AssetHandleData>> {
    const path = normalizeContentPath(uri, rawPath);
    if (!this.automationBridge || !(await this.ensureConnected())) {
      throw new ResourceError(RESOURCE_ERROR_CODES.UNAVAILABLE, uri, 'asset reference resolution requires a connected Unreal Editor');
    }
    const response = await this.automationBridge.sendAutomationRequest('manage_asset', { subAction: 'exists', assetPath: path });
    const result = isRecord(response) && isRecord(response.result) ? response.result : response;
    const exists = isRecord(response) && response.success !== false && isRecord(result) && result.exists === true;
    return { uri, revision: this.revisions.currentRevision('ue://asset-registry'), data: { path, exists } };
  }
}
