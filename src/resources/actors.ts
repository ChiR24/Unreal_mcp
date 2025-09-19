import { UnrealBridge } from '../unreal-bridge.js';

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
      
      const resp = await this.bridge.executePythonWithResult(pythonCode);
      if (resp && typeof resp === 'object' && resp.success === true && Array.isArray((resp as any).actors)) {
        this.setCache('listActors', resp);
        return resp;
      }

      // Fallback manual extraction with bracket matching
      const raw = await this.bridge.executePython(pythonCode);
      let output = '';
      if (raw?.LogOutput && Array.isArray(raw.LogOutput)) output = raw.LogOutput.map((l: any) => l.Output || '').join('');
      else if (typeof raw === 'string') output = raw; else output = JSON.stringify(raw);
      const marker = 'RESULT:';
      const idx = output.lastIndexOf(marker);
      if (idx !== -1) {
        let i = idx + marker.length;
        while (i < output.length && output[i] !== '{') i++;
        if (i < output.length) {
          let depth = 0, inStr = false, esc = false, j = i;
          for (; j < output.length; j++) {
            const ch = output[j];
            if (esc) { esc = false; continue; }
            if (ch === '\\') { esc = true; continue; }
            if (ch === '"') { inStr = !inStr; continue; }
            if (!inStr) {
              if (ch === '{') depth++;
              else if (ch === '}') { depth--; if (depth === 0) { j++; break; } }
            }
          }
          const jsonStr = output.slice(i, j);
          try {
            const parsed = JSON.parse(jsonStr);
            this.setCache('listActors', parsed);
            return parsed;
          } catch {}
        }
      }

      return { success: false, error: 'Failed to parse actors list' };
    } catch (err) {
      return { success: false, error: `Failed to list actors: ${err}` };
    }
  }

  async getActorByName(actorName: string) {
    // GetActorOfClass expects a class, not a name. Use Python to find by name
    try {
      const pythonCode = `
import unreal
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors()
for actor in actors:
    if actor and actor.get_name() == "${actorName}":
        print(f"Found actor: {actor.get_path_name()}")
        break
else:
    print(f"Actor not found: ${actorName}")
      `.trim();
      
      const result = await this.bridge.executePython(pythonCode);
      return result;
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
