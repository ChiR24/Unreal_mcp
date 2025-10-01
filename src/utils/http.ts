import axios, { AxiosInstance } from 'axios';
import http from 'http';
import https from 'https';
import { Logger } from './logger.js';

/**
 * Simple in-memory cache for HTTP responses
 * Based on best practices from Axios optimization guides
 */
interface CacheEntry {
  data: any;
  timestamp: number;
  ttl: number;
}

class SimpleCache {
  private cache = new Map<string, CacheEntry>();
  private readonly maxSize = 100;

  set(key: string, data: any, ttl: number = 60000): void {
    // Prevent unbounded growth
    if (this.cache.size >= this.maxSize) {
      const firstKey = this.cache.keys().next().value;
      if (firstKey !== undefined) {
        this.cache.delete(firstKey);
      }
    }
    
    this.cache.set(key, {
      data,
      timestamp: Date.now(),
      ttl
    });
  }

  get(key: string): any | null {
    const entry = this.cache.get(key);
    if (!entry) return null;

    // Check if expired
    if (Date.now() - entry.timestamp > entry.ttl) {
      this.cache.delete(key);
      return null;
    }

    return entry.data;
  }

  clear(): void {
    this.cache.clear();
  }

  getStats(): { size: number; maxSize: number } {
    return { size: this.cache.size, maxSize: this.maxSize };
  }
}

const responseCache = new SimpleCache();

// Enhanced connection pooling configuration to prevent socket failures
const httpAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 60000, // Increased keep-alive time
  maxSockets: 20, // Increased socket pool
  maxFreeSockets: 10, // More free sockets
  timeout: 60000, // Longer timeout
});

const httpsAgent = new https.Agent({
  keepAlive: true,
  keepAliveMsecs: 60000, // Increased keep-alive time
  maxSockets: 20, // Increased socket pool
  maxFreeSockets: 10, // More free sockets
  timeout: 60000, // Longer timeout
});

const log = new Logger('HTTP');

/**
 * Enhanced HTTP client factory with connection pooling and request timing
 */
export function createHttpClient(baseURL: string): AxiosInstance {
  const client = axios.create({
    baseURL,
    headers: {
      'Content-Type': 'application/json',
      'Accept': 'application/json'
    },
    timeout: 15000,
    httpAgent,
    httpsAgent,
    // Ensure proper handling of request body transformation
    transformRequest: [(data, headers) => {
      // Remove Content-Length if it's set incorrectly
      delete headers['Content-Length'];
      delete headers['content-length'];

      // Properly stringify JSON data
      if (data && typeof data === 'object') {
        const jsonStr = JSON.stringify(data);
        // Let axios set Content-Length automatically
        return jsonStr;
      }
      return data;
    }],
    // Optimize response handling
    maxContentLength: 50 * 1024 * 1024, // 50MB
    maxBodyLength: 50 * 1024 * 1024,
    decompress: true
  });

  // Request interceptor: timing, caching check, and logging
  client.interceptors.request.use(
    (config) => {
      // Add metadata for performance tracking
      (config as any).metadata = { startTime: Date.now() };
      
      // Check cache for GET requests
      if (config.method?.toLowerCase() === 'get' && config.url) {
        const cacheKey = `${config.url}:${JSON.stringify(config.params || {})}`;
        const cached = responseCache.get(cacheKey);
        if (cached) {
          log.debug(`[HTTP Cache Hit] ${config.url}`);
          // Return cached response
          (config as any).cached = cached;
        }
      }
      
      return config;
    },
    (error) => {
      log.error('[HTTP Request Error]', error);
      return Promise.reject(error);
    }
  );

  // Response interceptor: timing, caching, and error handling
  client.interceptors.response.use(
    (response) => {
      // Check if we used cached response
      if ((response.config as any).cached) {
        return Promise.resolve({
          ...response,
          data: (response.config as any).cached,
          status: 200,
          statusText: 'OK (Cached)',
          headers: {},
          config: response.config
        });
      }

      // Performance tracking
      const duration = Date.now() - ((response.config as any).metadata?.startTime || 0);
      if (duration > 5000) {
        log.warn(`[HTTP Slow] ${response.config.url} took ${duration}ms`);
      } else if (duration > 1000) {
        log.debug(`[HTTP] ${response.config.url} took ${duration}ms`);
      }

      // Cache successful GET responses
      if (response.config.method?.toLowerCase() === 'get' && 
          response.status === 200 && 
          response.config.url) {
        const cacheKey = `${response.config.url}:${JSON.stringify(response.config.params || {})}`;
        // Cache for 30 seconds by default
        responseCache.set(cacheKey, response.data, 30000);
      }

      return response;
    },
    (error) => {
      // Enhanced error logging
      const duration = Date.now() - ((error.config as any)?.metadata?.startTime || 0);
      log.error(`[HTTP Error] ${error.config?.url} failed after ${duration}ms:`, {
        status: error.response?.status,
        message: error.message,
        code: error.code
      });
      return Promise.reject(error);
    }
  );

  return client;
}

// No retry helpers are exported; consolidated command flows rely on
// Unreal's own retry/backoff semantics to avoid duplicate side effects.