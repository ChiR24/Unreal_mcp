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
    
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorLevelLibrary',
        functionName: 'GetAllLevelActors',
        parameters: {}
      });
      const result = res?.Result ?? res;
      this.setCache('listActors', result);
      return result;
    } catch (err) {
      return { error: `Failed to list actors: ${err}` };
    }
  }

  async getActorByName(actorName: string) {
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/Engine.Default__GameplayStatics',
        functionName: 'GetActorOfClass',
        parameters: {
          WorldContextObject: null,
          ActorName: actorName
        }
      });
      return res?.Result ?? res;
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
