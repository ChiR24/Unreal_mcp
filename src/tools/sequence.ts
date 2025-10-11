import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { Logger } from '../utils/logger.js';

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

export class SequenceTools {
    private log = new Logger('SequenceTools');
    private sequenceCache = new Map<string, LevelSequence>();

    // Plugin-first: delegate all sequence operations to the Automation Bridge
    // plugin. Do not attempt fallbacks to editor Python or other engine
    // plugins from the server; surface clear errors instead.
    constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

    private async sendAction(action: string, payload: Record<string, unknown> = {}, timeoutMs?: number) {
        if (!this.automationBridge || typeof this.automationBridge.sendAutomationRequest !== 'function') {
            return { success: false, error: 'AUTOMATION_BRIDGE_UNAVAILABLE', message: 'Automation bridge is not available' } as const;
        }
        const envDefault = Number(process.env.MCP_AUTOMATION_REQUEST_TIMEOUT_MS ?? '120000');
        const defaultTimeout = Number.isFinite(envDefault) && envDefault > 0 ? envDefault : 120000;
        const finalTimeout = typeof timeoutMs === 'number' && timeoutMs > 0 ? timeoutMs : defaultTimeout;
        try {
            const response: any = await this.automationBridge.sendAutomationRequest(action, payload, { timeoutMs: finalTimeout });
            const success = response && response.success !== false;
            const result = response.result ?? response;
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
        const resp = await this.sendAction('sequence_create', payload, params.timeoutMs as any);
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
}