import axios, { AxiosInstance, AxiosRequestConfig, AxiosError } from 'axios';
import http from 'http';
import https from 'https';
import { Logger } from './logger.js';

// Connection pooling configuration for better performance
const httpAgent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 30000,
  maxSockets: 10,
  maxFreeSockets: 5,
  timeout: 30000
});

const httpsAgent = new https.Agent({
  keepAlive: true,
  keepAliveMsecs: 30000,
  maxSockets: 10,
  maxFreeSockets: 5,
  timeout: 30000
});

// Retry configuration interface
interface RetryConfig {
  maxRetries: number;
  initialDelay: number;
  maxDelay: number;
  backoffMultiplier: number;
  retryableStatuses: number[];
  retryableErrors: string[];
}

const log = new Logger('HTTP');

const defaultRetryConfig: RetryConfig = {
  maxRetries: 3,
  initialDelay: 1000,
  maxDelay: 10000,
  backoffMultiplier: 2,
  retryableStatuses: [408, 429, 500, 502, 503, 504],
  retryableErrors: ['ECONNABORTED', 'ETIMEDOUT', 'ECONNRESET', 'ENOTFOUND']
};

/**
 * Calculate exponential backoff delay with jitter
 */
function calculateBackoff(attempt: number, config: RetryConfig): number {
  const delay = Math.min(
    config.initialDelay * Math.pow(config.backoffMultiplier, attempt - 1),
    config.maxDelay
  );
  // Add jitter to prevent thundering herd
  return delay + Math.random() * 1000;
}

/**
 * Enhanced HTTP client factory with connection pooling and retry logic
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

/**
 * Execute request with retry logic for resilience
 */
export async function requestWithRetry<T = any>(
  client: AxiosInstance,
  config: AxiosRequestConfig,
  retryConfig: Partial<RetryConfig> = {}
): Promise<T> {
  const retry = { ...defaultRetryConfig, ...retryConfig };
  let lastError: Error | null = null;

  for (let attempt = 1; attempt <= retry.maxRetries; attempt++) {
    try {
      const response = await client.request<T>(config);
      
      // Check if we should retry based on status
      if (retry.retryableStatuses.includes(response.status)) {
        throw new Error(`Retryable status: ${response.status}`);
      }
      
      return response.data;
    } catch (error) {
      lastError = error as Error;
      const axiosError = error as AxiosError;
      
      // Check if error is retryable
      const isRetryable = 
        retry.retryableErrors.includes(axiosError.code || '') ||
        (axiosError.response && retry.retryableStatuses.includes(axiosError.response.status));

      if (!isRetryable || attempt === retry.maxRetries) {
        throw error;
      }

      // Calculate delay and wait
const delay = calculateBackoff(attempt, retry);
      log.debug(`[HTTP] Retry attempt ${attempt}/${retry.maxRetries} after ${Math.round(delay)}ms`);
      await new Promise(resolve => setTimeout(resolve, delay));
    }
  }

  throw lastError || new Error('Request failed after retries');
}

/**
 * Batch multiple requests for efficiency
 */
export async function batchRequests<T = any>(
  client: AxiosInstance,
  requests: AxiosRequestConfig[],
  options: { concurrency?: number; throwOnError?: boolean } = {}
): Promise<(T | Error)[]> {
  const { concurrency = 5, throwOnError = false } = options;
  const results: (T | Error)[] = [];
  
  // Process requests in batches
  for (let i = 0; i < requests.length; i += concurrency) {
    const batch = requests.slice(i, i + concurrency);
    const batchPromises = batch.map(req => 
      client.request<T>(req)
        .then(res => res.data)
        .catch(err => throwOnError ? Promise.reject(err) : err)
    );
    
    const batchResults = await Promise.all(batchPromises);
    results.push(...batchResults);
  }
  
  return results;
}
