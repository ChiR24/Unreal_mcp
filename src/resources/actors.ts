import { UnrealBridge } from '../unreal-bridge.js';
import { PythonHelper } from '../utils/python-helpers.js';
import { bestEffortInterpretedText, coerceNumber, coerceString, interpretStandardResult } from '../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

interface CacheEntry {
  data: any;
  timestamp: number;
}

export class ActorResources {
  private cache = new Map<string, CacheEntry>();
  private readonly CACHE_TTL_MS = 5000; // 5 seconds cache for actors (they change more frequently)
  private readonly python: PythonHelper;
  private automationBridgeAvailable = false;

  constructor(private bridge: UnrealBridge, private automationBridge?: any) {
    this.python = new PythonHelper(bridge);
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
      // Prefer using the automation bridge transport if available; the
      // bridge can route this to the plugin's native actor listing
      // implementation which is more robust and version-safe.
      if (this.automationBridgeAvailable) {
        try {
          const resp: any = await this.automationBridge.sendAutomationRequest('control_actor', { action: 'list' });
          if (resp && resp.success !== false && Array.isArray((resp.result || resp).actors)) {
            const actors = (resp.result || resp).actors as any[];
            const count = coerceNumber((resp.result || resp).count) ?? actors.length;
            const payload = { success: true as const, count, actors };
            this.setCache('listActors', payload);
            return payload;
          }
        } catch (_err) {
          // If automation bridge fails or is not connected, fall back to
          // optional Python execution (deprecated and gated on server env).
        }
      }

      // Last-resort: use Python fallback if the server and plugin allow it.
      const pythonCode = `
import unreal, json
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors() if actor_subsystem else []
actor_list = []
for actor in actors:
    try:
        if actor:
            actor_list.append({
                'name': actor.get_name(),
                'class': actor.get_class().get_name() if actor.get_class() else '',
                'path': actor.get_path_name()
            })
    except Exception:
        pass
print('RESULT:' + json.dumps({'success': True, 'count': len(actor_list), 'actors': actor_list}))
      `.trim();

  const response = await (this.bridge as any).executeEditorPython(pythonCode, { allowPythonFallback: allowPythonFallbackFromEnv() });
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Retrieved actor list',
        failureMessage: 'Failed to retrieve actor list'
      });

      if (interpreted.success && Array.isArray(interpreted.payload.actors)) {
        const actors = interpreted.payload.actors as any[];
        const count = coerceNumber(interpreted.payload.count) ?? actors.length;
        const payload = { success: true as const, count, actors };
        this.setCache('listActors', payload);
        return payload;
      }

      return { success: false, error: coerceString(interpreted.payload.error) ?? interpreted.error ?? 'Failed to parse actors list', note: bestEffortInterpretedText(interpreted) };
    } catch (err) {
      return { success: false, error: `Failed to list actors: ${err}` };
    }
  }

  async getActorByName(actorName: string) {
    // GetActorOfClass expects a class, not a name. Use Python to find by name
    try {
      const pythonCode = `
import unreal
import json

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors() if actor_subsystem else []

found = None
for actor in actors:
    if actor and actor.get_name() == ${JSON.stringify(actorName)}:
        found = {
            'success': True,
            'name': actor.get_name(),
            'path': actor.get_path_name(),
            'class': actor.get_class().get_name()
        }
        break

if not found:
    found = {'success': False, 'error': f"Actor not found: {actorName}"}

print('RESULT:' + json.dumps(found))
      `.trim();

  const response = await (this.bridge as any).executeEditorPython(pythonCode, { allowPythonFallback: allowPythonFallbackFromEnv() });
      const interpreted = interpretStandardResult(response, {
        successMessage: `Actor resolved: ${actorName}`,
        failureMessage: `Actor not found: ${actorName}`
      });

      if (interpreted.success) {
        return {
          success: true as const,
          name: coerceString(interpreted.payload.name) ?? actorName,
          path: coerceString(interpreted.payload.path),
          class: coerceString(interpreted.payload.class)
        };
      }

      return {
        success: false as const,
        error: coerceString(interpreted.payload.error) ?? interpreted.error ?? `Actor not found: ${actorName}`
      };
    } catch (err) {
      return { error: `Failed to get actor: ${err}` };
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
      const result = await this.python.listActorComponents(actorPath);
      if (result?.success) {
        return {
          success: true as const,
          components: Array.isArray(result.components) ? result.components : []
        };
      }
      return {
        success: false as const,
        error: coerceString(result?.error) ?? `Failed to resolve components for ${actorPath}`
      };
    } catch (err) {
      return {
        success: false as const,
        error: `Component lookup failed: ${err}`
      };
    }
  }
}
