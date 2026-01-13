export interface CacheOptions {
  ttl?: number;
  refresh?: boolean;
}

interface CacheEntry<T> {
  data: T;
  timestamp: number;
  ttl: number;
}

export class ResourceCache<T> {
  private cache = new Map<string, CacheEntry<T>>();
  private defaultTTL: number;

  constructor(defaultTTLMs: number = 5000) {
    this.defaultTTL = defaultTTLMs;
  }

  get(key: string): T | null {
    const entry = this.cache.get(key);
    if (!entry) return null;

    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }

    return entry.data;
  }

  set(key: string, data: T, ttl?: number): void {
    this.cache.set(key, {
      data,
      timestamp: Date.now(),
      ttl: ttl ?? this.defaultTTL
    });
  }

  clear(): void {
    this.cache.clear();
  }

  /**
   * Invalidates keys matching a pattern.
   * If pattern is a string, invalidates keys starting with that string.
   * If pattern is a RegExp, invalidates keys matching the regex.
   */
  invalidate(pattern: string | RegExp): void {
    for (const key of this.cache.keys()) {
      if (typeof pattern === 'string') {
        if (key.startsWith(pattern)) {
          this.cache.delete(key);
        }
      } else if (pattern.test(key)) {
        this.cache.delete(key);
      }
    }
  }

  async getOrFetch(
    key: string,
    fetchFn: () => Promise<T>,
    options: CacheOptions = {}
  ): Promise<T> {
    const { refresh = false, ttl } = options;

    if (!refresh) {
      const cached = this.get(key);
      if (cached !== null) {
        return cached;
      }
    }

    const data = await fetchFn();
    this.set(key, data, ttl);
    return data;
  }
}
