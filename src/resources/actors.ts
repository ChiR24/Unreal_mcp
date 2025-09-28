import { UnrealBridge } from '../unreal-bridge.js';
import { bestEffortInterpretedText, coerceNumber, coerceString, interpretStandardResult } from '../utils/result-helpers.js';

interface CacheEntry {
  data: any;
  timestamp: number;
}

export class ActorResources {
  private cache = new Map<string, CacheEntry>();
  private readonly CACHE_TTL_MS = 5000; // 5 seconds cache for actors (they change more frequently)
  
  constructor(private bridge: UnrealBridge) {}
  
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
    
    // Use Python to get actors via EditorActorSubsystem
    try {
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
                'class': actor.get_class().get_name(),
                'path': actor.get_path_name()
            })
    except Exception:
        pass
print('RESULT:' + json.dumps({'success': True, 'count': len(actor_list), 'actors': actor_list}))
      `.trim();
      
      const response = await this.bridge.executePython(pythonCode);
      const interpreted = interpretStandardResult(response, {
        successMessage: 'Retrieved actor list',
        failureMessage: 'Failed to retrieve actor list'
      });

      if (interpreted.success && Array.isArray(interpreted.payload.actors)) {
        const actors = interpreted.payload.actors as any[];
        const count = coerceNumber(interpreted.payload.count) ?? actors.length;
        const payload = {
          success: true as const,
          count,
          actors
        };
        this.setCache('listActors', payload);
        return payload;
      }

      return {
        success: false,
        error: coerceString(interpreted.payload.error) ?? interpreted.error ?? 'Failed to parse actors list',
        note: bestEffortInterpretedText(interpreted)
      };
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

      const response = await this.bridge.executePython(pythonCode);
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
      const res = await this.bridge.httpCall('/remote/object/property', 'GET', {
        objectPath: actorPath,
        propertyName: 'ActorTransform'
      });
      return res;
    } catch (err) {
      return { error: `Failed to get transform: ${err}` };
    }
  }
}
