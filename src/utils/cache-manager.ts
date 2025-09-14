/**
 * Cache Manager for API responses
 * Implements LRU cache with TTL support for optimizing repeated API calls
 */

interface CacheEntry<T> {
  data: T;
  timestamp: number;
  hits: number;
}

interface CacheOptions {
  maxSize?: number;
  defaultTTL?: number; // in milliseconds
  enableMetrics?: boolean;
}

interface CacheMetrics {
  hits: number;
  misses: number;
  evictions: number;
  size: number;
}

export class CacheManager<T = any> {
  private cache: Map<string, CacheEntry<T>>;
  private readonly maxSize: number;
  private readonly defaultTTL: number;
  private readonly enableMetrics: boolean;
  private metrics: CacheMetrics;

  constructor(options: CacheOptions = {}) {
    this.cache = new Map();
    this.maxSize = options.maxSize || 100;
    this.defaultTTL = options.defaultTTL || 60000; // 1 minute default
    this.enableMetrics = options.enableMetrics || false;
    this.metrics = {
      hits: 0,
      misses: 0,
      evictions: 0,
      size: 0
    };
  }

  /**
   * Get item from cache
   */
  get(key: string): T | null {
    const entry = this.cache.get(key);
    
    if (!entry) {
      if (this.enableMetrics) this.metrics.misses++;
      return null;
    }

    // Check if expired
    if (Date.now() - entry.timestamp > this.defaultTTL) {
      this.cache.delete(key);
      if (this.enableMetrics) {
        this.metrics.misses++;
        this.metrics.size--;
      }
      return null;
    }

    // Update hit count and move to end (LRU)
    entry.hits++;
    this.cache.delete(key);
    this.cache.set(key, entry);
    
    if (this.enableMetrics) this.metrics.hits++;
    return entry.data;
  }

  /**
   * Set item in cache
   */
  set(key: string, data: T): void {
    // Evict oldest if at max size
    if (this.cache.size >= this.maxSize && !this.cache.has(key)) {
      const firstKey = this.cache.keys().next().value;
      if (firstKey) {
        this.cache.delete(firstKey);
        if (this.enableMetrics) {
          this.metrics.evictions++;
          this.metrics.size--;
        }
      }
    }

    const entry: CacheEntry<T> = {
      data,
      timestamp: Date.now(),
      hits: 0
    };

    this.cache.set(key, entry);
    if (this.enableMetrics) this.metrics.size = this.cache.size;
  }

  /**
   * Check if key exists and is valid
   */
  has(key: string): boolean {
    const entry = this.cache.get(key);
    if (!entry) return false;
    
    // Check expiration
    if (Date.now() - entry.timestamp > this.defaultTTL) {
      this.cache.delete(key);
      if (this.enableMetrics) this.metrics.size--;
      return false;
    }
    
    return true;
  }

  /**
   * Clear specific key or all cache
   */
  clear(key?: string): void {
    if (key) {
      this.cache.delete(key);
      if (this.enableMetrics) this.metrics.size = this.cache.size;
    } else {
      this.cache.clear();
      if (this.enableMetrics) {
        this.metrics.size = 0;
      }
    }
  }

  /**
   * Get cache metrics
   */
  getMetrics(): CacheMetrics {
    return { ...this.metrics };
  }

  /**
   * Get cache hit rate
   */
  getHitRate(): number {
    const total = this.metrics.hits + this.metrics.misses;
    return total > 0 ? this.metrics.hits / total : 0;
  }

  /**
   * Wrap async function with cache
   */
  async wrap<R = T>(
    key: string,
    fn: () => Promise<R>
  ): Promise<R> {
    // Check cache first
    const cached = this.get(key) as R | null;
    if (cached !== null) {
      return cached;
    }

    // Execute function and cache result
    const result = await fn();
    this.set(key, result as any);
    return result;
  }

  /**
   * Batch get multiple keys
   */
  getBatch(keys: string[]): Map<string, T | null> {
    const results = new Map<string, T | null>();
    for (const key of keys) {
      results.set(key, this.get(key));
    }
    return results;
  }

  /**
   * Invalidate cache entries by pattern
   */
  invalidatePattern(pattern: RegExp): number {
    let count = 0;
    for (const key of this.cache.keys()) {
      if (pattern.test(key)) {
        this.cache.delete(key);
        count++;
      }
    }
    if (this.enableMetrics) {
      this.metrics.size = this.cache.size;
    }
    return count;
  }
}

// Global cache instances for different purposes
export const assetCache = new CacheManager({
  maxSize: 500,
  defaultTTL: 300000, // 5 minutes for assets
  enableMetrics: true
});

export const engineCache = new CacheManager({
  maxSize: 50,
  defaultTTL: 600000, // 10 minutes for engine info
  enableMetrics: true
});

export const commandCache = new CacheManager({
  maxSize: 100,
  defaultTTL: 30000, // 30 seconds for commands
  enableMetrics: true
});