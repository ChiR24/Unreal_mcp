import axios, { AxiosInstance } from 'axios';
import http from 'http';
import https from 'https';
import { Logger } from './logger.js';

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

  // Add request interceptor for timing
  client.interceptors.request.use(
    (config) => {
      // Add metadata for performance tracking
      (config as any).metadata = { startTime: Date.now() };
      return config;
    },
    (error) => {
      return Promise.reject(error);
    }
  );

  // Add response interceptor for timing and logging
  client.interceptors.response.use(
    (response) => {
      const duration = Date.now() - ((response.config as any).metadata?.startTime || 0);
      if (duration > 5000) {
        log.warn(`[HTTP] Slow request: ${response.config.url} took ${duration}ms`);
      }
      return response;
    },
    (error) => {
      return Promise.reject(error);
    }
  );

  return client;
}

// No retry helpers are exported; consolidated command flows rely on
// Unreal's own retry/backoff semantics to avoid duplicate side effects.