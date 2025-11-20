import { BaseTool } from './base-tool.js';
import { ISequenceTools } from '../types/tool-interfaces.js';

export interface LevelSequence {
  path: string;
  name: string;
  duration?: number;
  frameRate?: number;
  bindings?: SequenceBinding[];
}

export interface SequenceBinding {
  id: string;
  name: string;
  type: 'actor' | 'camera' | 'spawnable';
  tracks?: SequenceTrack[];
}

export interface SequenceTrack {
  name: string;
  type: string;
  sections?: any[];
}

export class SequenceTools extends BaseTool implements ISequenceTools {
  private sequenceCache = new Map<string, LevelSequence>();

  private async sendAction(action: string, payload: Record<string, unknown> = {}, timeoutMs?: number) {
    const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
    const defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
    const finalTimeout = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : defaultTimeout;

    try {
      // For sequence_* actions, wait for completion to avoid false-positive success
      const isSequenceAction = String(action || '').toLowerCase().startsWith('sequence_');

      const response = await this.sendAutomationRequest(
        action,
        payload,
        { timeoutMs: finalTimeout, waitForEvent: false }
      );

      let success = response && response.success !== false;
      const result = response.result ?? response;
      // Guard against empty/placeholder acks for sequence actions
      if (success && isSequenceAction) {
        const hasMeaningfulResult = result && typeof result === 'object' ? Object.keys(result).length > 0 : result !== undefined && result !== null;
        if (!hasMeaningfulResult) {
          success = false;
          return { success, error: 'NO_RESULT', message: 'Plugin acknowledged request but returned no result', result: undefined, requestId: response.requestId } as any;
        }
      }
      return { success, message: response.message ?? undefined, error: response.success === false ? (response.error ?? response.message) : undefined, result, requestId: response.requestId } as any;
    } catch (err: any) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  private isUnknownActionResponse(res: any): boolean {
    if (!res) return false;
    const txt = String((res.error ?? res.message ?? '')).toLowerCase();
    return txt.includes('unknown') || txt.includes('unknown_action') || txt.includes('unknown automation action');
  }

  async create(params: { name: string; path?: string; timeoutMs?: number }) {
    const name = params.name?.trim();
    const base = (params.path || '/Game/Sequences').replace(/\/$/, '');
    if (!name) return { success: false, error: 'name is required' };

    const payload = { name, path: base } as Record<string, unknown>;
    const resp = await this.sendAction('sequence_create', payload, params.timeoutMs);
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_create' } as const;
    }
    if (resp.success && resp.result && resp.result.sequencePath) {
      const sequence: LevelSequence = { path: resp.result.sequencePath, name };
      this.sequenceCache.set(sequence.path, sequence);
    }
    return resp;
  }

  async open(params: { path: string }) {
    const resp = await this.sendAction('sequence_open', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_open' } as const;
    }
    return resp;
  }

  async addCamera(params: { spawnable?: boolean }) {
    const resp = await this.sendAction('sequence_add_camera', { spawnable: params.spawnable !== false });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_camera' } as const;
    }
    return resp;
  }

  async addActor(params: { actorName: string; createBinding?: boolean }) {
    const resp = await this.sendAction('sequence_add_actor', { actorName: params.actorName, createBinding: params.createBinding });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_actor' } as const;
    }
    return resp;
  }

  /**
   * Play the current level sequence
   */
  async play(params?: { startTime?: number; loopMode?: 'once' | 'loop' | 'pingpong' }) {
    const resp = await this.sendAction('sequence_play', { startTime: params?.startTime, loopMode: params?.loopMode });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_play' } as const;
    }
    return resp;
  }

  /**
   * Pause the current level sequence
   */
  async pause() {
    const resp = await this.sendAction('sequence_pause');
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_pause' } as const;
    }
    return resp;
  }

  /**
   * Stop/close the current level sequence
   */
  async stop() {
    const resp = await this.sendAction('sequence_stop');
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_stop' } as const;
    }
    return resp;
  }

  /**
   * Set sequence properties including frame rate and length
   */
  async setSequenceProperties(params: {
    path?: string;
    frameRate?: number;
    lengthInFrames?: number;
    playbackStart?: number;
    playbackEnd?: number;
  }) {
    const payload: Record<string, unknown> = {
      path: params.path,
      frameRate: params.frameRate,
      lengthInFrames: params.lengthInFrames,
      playbackStart: params.playbackStart,
      playbackEnd: params.playbackEnd
    };
    const resp = await this.sendAction('sequence_set_properties', payload);
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_set_properties' } as const;
    }
    return resp;
  }

  /**
   * Get sequence properties
   */
  async getSequenceProperties(params: { path?: string }) {
    const resp = await this.sendAction('sequence_get_properties', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_properties' } as const;
    }
    return resp;
  }

  /**
   * Set playback speed/rate
   */
  async setPlaybackSpeed(params: { speed: number }) {
    const resp = await this.sendAction('sequence_set_playback_speed', { speed: params.speed });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_set_playback_speed' } as const;
    }
    return resp;
  }

  /**
   * Get all bindings in the current sequence
   */
  async getBindings(params?: { path?: string }) {
    const resp = await this.sendAction('sequence_get_bindings', { path: params?.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_bindings' } as const;
    }
    return resp;
  }

  /**
   * Add multiple actors to sequence at once
   */
  async addActors(params: { actorNames: string[] }) {
    const resp = await this.sendAction('sequence_add_actors', { actorNames: params.actorNames });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_actors' } as const;
    }
    return resp;
  }

  /**
   * Remove actors from binding
   */
  async removeActors(params: { actorNames: string[] }) {
    const resp = await this.sendAction('sequence_remove_actors', { actorNames: params.actorNames });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_remove_actors' } as const;
    }
    return resp;
  }

  /**
   * Create a spawnable from an actor class
   */
  async addSpawnableFromClass(params: { className: string; path?: string }) {
    const resp = await this.sendAction('sequence_add_spawnable_from_class', { className: params.className, path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_spawnable_from_class' } as const;
    }
    return resp;
  }

  async list(params?: { path?: string }) {
    const resp = await this.sendAction('sequence_list', { path: params?.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_list' } as const;
    }
    return resp;
  }

  async duplicate(params: { path: string; destinationPath: string }) {
    const resp = await this.sendAction('sequence_duplicate', { path: params.path, destinationPath: params.destinationPath });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_duplicate' } as const;
    }
    return resp;
  }

  async rename(params: { path: string; newName: string }) {
    const resp = await this.sendAction('sequence_rename', { path: params.path, newName: params.newName });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_rename' } as const;
    }
    return resp;
  }

  async deleteSequence(params: { path: string }) {
    const resp = await this.sendAction('sequence_delete', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_delete' } as const;
    }
    return resp;
  }

  async getMetadata(params: { path?: string }) {
    const resp = await this.sendAction('sequence_get_metadata', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_metadata' } as const;
    }
    return resp;
  }
}