import { BaseTool } from './base-tool.js';
import { ISequenceTools, StandardActionResponse } from '../types/tool-interfaces.js';
import { SequenceResponse } from '../types/automation-responses.js';
import { wasmIntegration } from '../wasm/index.js';

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
  private activeSequencePath?: string;

  private resolveSequencePath(explicitPath?: unknown): string | undefined {
    if (typeof explicitPath === 'string' && explicitPath.trim().length > 0) {
      return explicitPath.trim();
    }
    return this.activeSequencePath;
  }

  private async sendAction(action: string, payload: Record<string, unknown> = {}, timeoutMs?: number) {
    const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
    const defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
    const finalTimeout = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : defaultTimeout;

    try {
      const response = await this.sendAutomationRequest<SequenceResponse>(
        action,
        payload,
        { timeoutMs: finalTimeout, waitForEvent: false }
      );

      const success = response && response.success !== false;
      const result = response.result ?? response;

      return { success, message: response.message ?? undefined, error: response.success === false ? (response.error ?? response.message) : undefined, result, requestId: response.requestId } as any;
    } catch (err: unknown) {
      return { success: false, error: String(err), message: String(err) } as const;
    }
  }

  private isUnknownActionResponse(res: any): boolean {
    if (!res) return false;
    const txt = String((res.error ?? res.message ?? '')).toLowerCase();
    // Only treat specific error codes as "not implemented"
    return txt.includes('unknown_action') || txt.includes('unknown automation action') || txt.includes('not_implemented') || txt === 'unknown_plugin_action';
  }

  async create(params: { name: string; path?: string; timeoutMs?: number }): Promise<StandardActionResponse> {
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
      return { ...resp, sequence: resp.result.sequencePath };
    }
    return resp;
  }

  async open(params: { path: string }): Promise<StandardActionResponse> {
    const path = params.path?.trim();
    const resp = await this.sendAction('sequence_open', { path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_open' } as const;
    }
    if (resp && resp.success !== false && path) {
      this.activeSequencePath = path;
    }
    return resp;
  }

  async addCamera(params: { spawnable?: boolean; path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params.path);
    const resp = await this.sendAction('sequence_add_camera', { path, spawnable: params.spawnable !== false });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_camera' } as const;
    }
    return resp;
  }

  async addActor(params: { actorName: string; createBinding?: boolean; path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params.path);
    const resp = await this.sendAction('sequence_add_actor', { path, actorName: params.actorName, createBinding: params.createBinding });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_actor' } as const;
    }
    return resp;
  }

  /**
   * Play the current level sequence
   */
  async play(params?: { path?: string; startTime?: number; loopMode?: 'once' | 'loop' | 'pingpong' }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params?.path);
    const resp = await this.sendAction('sequence_play', { path, startTime: params?.startTime, loopMode: params?.loopMode });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_play' } as const;
    }
    return resp;
  }

  /**
   * Pause the current level sequence
   */
  async pause(params?: { path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params?.path);
    const resp = await this.sendAction('sequence_pause', { path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_pause' } as const;
    }
    return resp;
  }

  /**
   * Stop/close the current level sequence
   */
  async stop(params?: { path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params?.path);
    const resp = await this.sendAction('sequence_stop', { path });
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
  }): Promise<StandardActionResponse> {
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
   * Set display rate (fps)
   */
  async setDisplayRate(params: { path?: string; frameRate: string | number }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_set_display_rate', { path: params.path, frameRate: params.frameRate });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_set_display_rate' } as const;
    }
    return resp;
  }

  /**
   * Get sequence properties
   */
  async getSequenceProperties(params: { path?: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_get_properties', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_properties' } as const;
    }
    return resp;
  }

  /**
   * Set playback speed/rate
   */
  async setPlaybackSpeed(params: { speed: number; path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params.path);
    const resp = await this.sendAction('sequence_set_playback_speed', { path, speed: params.speed });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_set_playback_speed' } as const;
    }
    return resp;
  }

  /**
   * Get all bindings in the current sequence
   */
  async getBindings(params?: { path?: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_get_bindings', { path: params?.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_bindings' } as const;
    }
    return resp;
  }

  /**
   * Add multiple actors to sequence at once
   */
  async addActors(params: { actorNames: string[]; path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params.path);
    const resp = await this.sendAction('sequence_add_actors', { path, actorNames: params.actorNames });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_actors' } as const;
    }
    return resp;
  }

  /**
   * Remove actors from binding
   */
  async removeActors(params: { actorNames: string[]; path?: string }): Promise<StandardActionResponse> {
    const path = this.resolveSequencePath(params.path);
    const resp = await this.sendAction('sequence_remove_actors', { path, actorNames: params.actorNames });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_remove_actors' } as const;
    }
    return resp;
  }

  /**
   * Create a spawnable from an actor class
   */
  async addSpawnableFromClass(params: { className: string; path?: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_add_spawnable_from_class', { className: params.className, path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_spawnable_from_class' } as const;
    }
    return resp;
  }

  async list(params?: { path?: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_list', { path: params?.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_list' } as const;
    }
    if (resp.success) {
      const sequences = resp.sequences || resp.data || resp.result || [];
      return {
        ...resp,
        sequences,
        count: Array.isArray(sequences) ? sequences.length : undefined
      };
    }
    return resp;
  }

  async duplicate(params: { path: string; destinationPath: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_duplicate', { path: params.path, destinationPath: params.destinationPath });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_duplicate' } as const;
    }
    return resp;
  }

  async rename(params: { path: string; newName: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_rename', { path: params.path, newName: params.newName });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_rename' } as const;
    }
    return resp;
  }

  async deleteSequence(params: { path: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_delete', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_delete' } as const;
    }
    return resp;
  }

  async getMetadata(params: { path?: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_get_metadata', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_get_metadata' } as const;
    }
    return resp;
  }

  /**
   * Add a keyframe to a sequence binding
   */
  async addKeyframe(params: {
    path?: string;
    bindingId?: string;
    actorName?: string;
    property: 'Transform';
    frame: number;
    value: {
      location?: { x: number; y: number; z: number };
      rotation?: { roll: number; pitch: number; yaw: number };
      scale?: { x: number; y: number; z: number };
    };
  }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_add_keyframe', {
      path: params.path,
      bindingId: params.bindingId,
      actorName: params.actorName,
      property: params.property,
      frame: params.frame,
      value: params.value
    });

    // Use WASM for transform processing
    if (params.property === 'Transform' && params.value) {
      const loc = params.value.location;
      const rot = params.value.rotation;
      const scale = params.value.scale;
      if (loc && rot && scale) {
        const locArr: [number, number, number] = [loc.x, loc.y, loc.z];
        const rotArr: [number, number, number] = [rot.pitch, rot.yaw, rot.roll];
        const scaleArr: [number, number, number] = [scale.x, scale.y, scale.z];
        wasmIntegration.composeTransform(locArr, rotArr, scaleArr);
        console.error('[WASM] Using composeTransform for keyframe validation');
      }
    }
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_add_keyframe' } as const;
    }
    return resp;
  }

  /**
   * List tracks in a sequence
   */
  async listTracks(params: { path: string }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_list_tracks', { path: params.path });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_list_tracks' } as const;
    }
    if (resp.success) {
      const tracks = resp.tracks || resp.data || resp.result || [];
      return {
        ...resp,
        tracks,
        count: Array.isArray(tracks) ? tracks.length : undefined
      };
    }
    return resp;
  }

  /**
   * List available track types
   */
  async listTrackTypes(): Promise<StandardActionResponse> {
    const resp = await this.sendAction('list_track_types', {});
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement list_track_types' } as const;
    }
    return resp;
  }

  /**
   * Set playback work range
   */
  async setWorkRange(params: { path?: string; start: number; end: number }): Promise<StandardActionResponse> {
    const resp = await this.sendAction('sequence_set_work_range', { path: params.path, start: params.start, end: params.end });
    if (!resp.success && this.isUnknownActionResponse(resp)) {
      return { success: false, error: 'UNKNOWN_PLUGIN_ACTION', message: 'Automation plugin does not implement sequence_set_work_range' } as const;
    }
    return resp;
  }
}