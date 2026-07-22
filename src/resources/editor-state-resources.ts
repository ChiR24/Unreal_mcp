// src/resources/editor-state-resources.ts
// Task 31: bounded, revisioned project / editor / selection resources. All live
// editor-state reads cross the game-thread boundary through an injected
// `EditorStateSource`; the default implementation queues through the automation
// bridge (which dispatches on the subsystem game thread). Unit tests inject a
// fake source, so no live editor is required. When the editor is unavailable the
// read throws a typed `ResourceError` and never mutates. No host filesystem path
// or secret is emitted — the project name is pre-redacted by the caller.

import type { AutomationRequestBridge } from '../types/tools/tool-interfaces.js';
import { coerceString } from '../utils/responses/result-helpers.js';
import { isRecord } from '../utils/validation/type-guards.js';
import type { RevisionProvider, RevisionedResource } from '../server/mcp-primitives/resource-revision.js';
import { RESOURCE_ERROR_CODES, ResourceError } from './resource-errors.js';

const MAX_SELECTION = 200;
const CONTENT_ROOT = '/Game';

export interface ProjectData {
  readonly projectName: string | null;
  readonly engineVersion: string | null;
  readonly contentRoot: string;
  readonly connected: boolean;
}

export interface EditorData {
  readonly pieActive: boolean;
  readonly currentLevel: { readonly name: string; readonly path: string };
}

export interface SelectionEntry {
  readonly name: string;
  readonly path: string;
}

export interface SelectionData {
  readonly count: number;
  readonly totalCount: number;
  readonly truncated: boolean;
  readonly actors: readonly SelectionEntry[];
}

/**
 * Injected game-thread boundary for editor-state reads. Task 31 defines the
 * contract and tests it with fakes; the default binds it to the automation
 * bridge. Task 37 owns the live protocol wiring.
 */
export interface EditorStateSource {
  isAvailable(): Promise<boolean>;
  engineVersion(): Promise<string | null>;
  pieActive(): Promise<boolean>;
  currentLevel(): Promise<{ name: string; path: string }>;
  selectedActors(): Promise<readonly SelectionEntry[]>;
}

export class EditorStateResources {
  constructor(
    private readonly source: EditorStateSource,
    private readonly revisions: RevisionProvider,
    private readonly projectName: string | null,
  ) {}

  async readProject(): Promise<RevisionedResource<ProjectData>> {
    const uri = 'ue://project';
    const connected = await this.source.isAvailable();
    const engineVersion = connected ? await this.source.engineVersion() : null;
    return {
      uri,
      revision: this.revisions.currentRevision('ue://project'),
      data: { projectName: this.projectName, engineVersion, contentRoot: CONTENT_ROOT, connected },
    };
  }

  async readEditor(): Promise<RevisionedResource<EditorData>> {
    const uri = 'ue://editor';
    if (!(await this.source.isAvailable())) {
      throw new ResourceError(RESOURCE_ERROR_CODES.UNAVAILABLE, uri, 'Editor state requires a connected Unreal Editor');
    }
    const [pieActive, currentLevel] = await Promise.all([this.source.pieActive(), this.source.currentLevel()]);
    // `ue://editor` is not independently subscribable; its state tracks PIE.
    return { uri, revision: this.revisions.currentRevision('ue://pie'), data: { pieActive, currentLevel } };
  }

  async readSelection(): Promise<RevisionedResource<SelectionData>> {
    const uri = 'ue://selection';
    if (!(await this.source.isAvailable())) {
      throw new ResourceError(RESOURCE_ERROR_CODES.UNAVAILABLE, uri, 'Selection requires a connected Unreal Editor');
    }
    const all = await this.source.selectedActors();
    const capped = all.slice(0, MAX_SELECTION);
    return {
      uri,
      revision: this.revisions.currentRevision('ue://selection'),
      data: { count: capped.length, totalCount: all.length, truncated: all.length > capped.length, actors: capped },
    };
  }
}

/**
 * Default editor-state source over the automation bridge. Uses the real
 * `inspect` read actions (`pie_report`, `get_selected_actors`) and the
 * `list_levels` action; each is best-effort and returns a safe empty/false
 * default when the bridge yields nothing. Never throws — availability gating is
 * the caller's responsibility.
 */
export class BridgeEditorStateSource implements EditorStateSource {
  constructor(
    private readonly automationBridge: AutomationRequestBridge | undefined,
    private readonly ensureConnected: () => Promise<boolean>,
    private readonly engineVersionProvider: () => Promise<string | null>,
  ) {}

  async isAvailable(): Promise<boolean> {
    if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
      return false;
    }
    return this.ensureConnected();
  }

  async engineVersion(): Promise<string | null> {
    return this.engineVersionProvider();
  }

  async pieActive(): Promise<boolean> {
    const result = this.nested(await this.request('inspect', { action: 'pie_report' }));
    if (result === undefined) {
      return false;
    }
    return Boolean(result.isPlaying ?? result.pie ?? result.active ?? result.isInPIE);
  }

  async currentLevel(): Promise<{ name: string; path: string }> {
    const resp = await this.request('list_levels', {});
    const result = this.nested(resp);
    return {
      name: coerceString(resp?.currentMap) ?? coerceString(result?.currentMap) ?? 'None',
      path: coerceString(resp?.currentMapPath) ?? coerceString(result?.currentMapPath) ?? 'None',
    };
  }

  async selectedActors(): Promise<readonly SelectionEntry[]> {
    const result = this.nested(await this.request('inspect', { action: 'get_selected_actors' }));
    const list = Array.isArray(result?.actors) ? result.actors : Array.isArray(result?.selected) ? result.selected : [];
    const entries: SelectionEntry[] = [];
    for (const raw of list) {
      if (!isRecord(raw)) {
        continue;
      }
      entries.push({
        name: coerceString(raw.name) ?? coerceString(raw.label) ?? '',
        path: coerceString(raw.path) ?? coerceString(raw.actorPath) ?? '',
      });
    }
    return entries;
  }

  private async request(action: string, payload: Record<string, unknown>): Promise<Record<string, unknown> | undefined> {
    if (!this.automationBridge) {
      return undefined;
    }
    const response = await this.automationBridge.sendAutomationRequest(action, payload);
    return isRecord(response) ? response : undefined;
  }

  private nested(response: Record<string, unknown> | undefined): Record<string, unknown> | undefined {
    if (response === undefined) {
      return undefined;
    }
    return isRecord(response.result) ? response.result : response;
  }
}
