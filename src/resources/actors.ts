import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { coerceNumber, coerceString } from '../utils/result-helpers.js';
import { ResourceCache } from '../cache/resource-cache.js';

export class ActorResources {
  private cache = new ResourceCache<unknown>(5000); // 5 seconds cache for actors (they change more frequently)
  private automationBridgeAvailable = false;

  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {
    this.automationBridgeAvailable = Boolean(automationBridge && typeof automationBridge.sendAutomationRequest === 'function');
  }

  async listActors(options?: { refresh?: boolean }) {
    try {
      return await this.cache.getOrFetch('listActors', async () => {
        if (!this.automationBridgeAvailable || !this.automationBridge) {
          throw new Error('Automation bridge is not available. Please ensure Unreal Engine is running with the MCP Automation Bridge plugin.');
        }

        const resp = await this.automationBridge.sendAutomationRequest('control_actor', { action: 'list' }) as Record<string, unknown>;
        const resultObj = (resp?.result ?? resp) as Record<string, unknown>;
        
        if (resp && resp.success !== false && Array.isArray(resultObj.actors)) {
          const actors = resultObj.actors as Array<Record<string, unknown>>;
          const count = coerceNumber(resultObj.count) ?? actors.length;
          return { success: true as const, count, actors };
        }

        throw new Error('Failed to retrieve actor list from automation bridge');
      }, { refresh: options?.refresh });
    } catch (err: unknown) {
      // Return error object instead of throwing, to match previous API
      const errorMessage = err instanceof Error ? err.message : String(err);
      return { success: false, error: errorMessage };
    }
  }

  async getActorByName(actorName: string) {
    try {
      if (!this.automationBridgeAvailable || !this.automationBridge) {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp = await this.automationBridge.sendAutomationRequest('control_actor', {
        action: 'find_by_name',
        name: actorName
      }) as Record<string, unknown>;

      const resultObj = resp?.result as Record<string, unknown> | null;
      if (resp && resp.success !== false && resultObj) {
        return {
          success: true as const,
          name: coerceString(resultObj.name) ?? actorName,
          path: coerceString(resultObj.path),
          class: coerceString(resultObj.class)
        };
      }

      return {
        success: false as const,
        error: `Actor not found: ${actorName}`
      };
    } catch (err: unknown) {
      return { success: false, error: `Failed to get actor: ${err}` };
    }
  }

  async getActorTransform(actorPath: string) {
    try {
      return await this.bridge.getObjectProperty({
        objectPath: actorPath,
        propertyName: 'ActorTransform'
      });
    } catch (err: unknown) {
      return { error: `Failed to get transform: ${err}` };
    }
  }

  async listActorComponents(actorPath: string) {
    try {
      if (!this.automationBridgeAvailable || !this.automationBridge) {
        return { success: false, error: 'Automation bridge is not available' };
      }

      const resp = await this.automationBridge.sendAutomationRequest('control_actor', {
        action: 'list_components',
        actor_path: actorPath
      }) as Record<string, unknown>;

      const resultObj = resp?.result as Record<string, unknown> | null;
      if (resp && resp.success !== false && Array.isArray(resultObj?.components)) {
        return {
          success: true as const,
          components: resultObj.components
        };
      }

      return {
        success: false as const,
        error: `Failed to resolve components for ${actorPath}`
      };
    } catch (err: unknown) {
      return {
        success: false as const,
        error: `Component lookup failed: ${err}`
      };
    }
  }
}
