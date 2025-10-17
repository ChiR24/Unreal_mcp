import { UnrealBridge } from '../unreal-bridge.js';
import { coerceNumber, coerceString } from '../utils/result-helpers.js';

interface CacheEntry {
  data: any;
  timestamp: number;
}

export class ActorResources {
  private cache = new Map<string, CacheEntry>();
  private readonly CACHE_TTL_MS = 5000; // 5 seconds cache for actors (they change more frequently)
  private automationBridgeAvailable = false;

  constructor(private bridge: UnrealBridge, private automationBridge?: any) {
    this.automationBridgeAvailable = Boolean(automationBridge && typeof automationBridge.sendAutomationRequest === 'function');
  }
  
  private getFromCache(key: string): any | null {
    const entry = this.cache.get(key);
    if (entry && (Date.now() - entry.timestamp) < this.CACHE_TTL_MS) {
      return entry.data;
    }
    this.cache.delete(key);
    return null;
  }
  
  private setCache(key: string, data: any): void {
    this.cache.set(key, { data, timestamp: Date.now() });
  }

  async listActors() {
    // Check cache first
    const cached = this.getFromCache('listActors');
    if (cached !== null) {
      return cached;
    }
    
    try {
      if (!this.automationBridgeAvailable) {
        return { success: false, error: 'Automation bridge is not available. Please ensure Unreal Engine is running with the MCP Automation Bridge plugin.' };
      }

      const resp: any = await this.automationBridge.sendAutomationRequest('control_actor', { action: 'list' });
      if (resp && resp.success !== false && Array.isArray((resp.result || resp).actors)) {
        const actors = (resp.result || resp).actors as any[];
        const count = coerceNumber((resp.result || resp).count) ?? actors.length;
        const payload = { success: true as const, count, actors };
        this.setCache('listActors', payload);
        return payload;
      }

      return { success: false, error: 'Failed to retrieve actor list from automation bridge' };
    } catch (err) {
      return { success: false, error: `Failed to list actors: ${err}` };
    }
  }

  async getActorByName(actorName: string) {
    try {
      if (!this.automationBridgeAvailable) {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp: any = await this.automationBridge.sendAutomationRequest('control_actor', {
        action: 'find_by_name',
        name: actorName
      });

      if (resp && resp.success !== false && resp.result) {
        return {
          success: true as const,
          name: coerceString(resp.result.name) ?? actorName,
          path: coerceString(resp.result.path),
          class: coerceString(resp.result.class)
        };
      }

      return {
        success: false as const,
        error: `Actor not found: ${actorName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to get actor: ${err}` };
    }
  }

  async getActorTransform(actorPath: string) {
    try {
      return await this.bridge.getObjectProperty({
        objectPath: actorPath,
        propertyName: 'ActorTransform'
      });
    } catch (err) {
      return { error: `Failed to get transform: ${err}` };
    }
  }

  async listActorComponents(actorPath: string) {
    try {
      if (!this.automationBridgeAvailable) {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp: any = await this.automationBridge.sendAutomationRequest('control_actor', {
        action: 'list_components',
        actor_path: actorPath
      });

      if (resp && resp.success !== false && Array.isArray(resp.result?.components)) {
        return {
          success: true as const,
          components: resp.result.components
        };
      }

      return {
        success: false as const,
        error: `Failed to resolve components for ${actorPath}`
      };
    } catch (err) {
      return {
        success: false as const,
        error: `Component lookup failed: ${err}`
      };
    }
  }
}
