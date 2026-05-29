import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';

export interface PerformanceMetrics {
  totalRequests: number;
  successfulRequests: number;
  failedRequests: number;
  averageResponseTime: number;
  responseTimes: number[];
  connectionStatus: 'connected' | 'disconnected' | 'error';
  lastHealthCheck: Date;
  uptime: number;
  recentErrors: Array<{ time: string; scope: string; type: string; message: string; retriable: boolean }>;
}

const RESPONSE_TIME_SAMPLE_LIMIT = 100;
const RECENT_ERROR_LIMIT = 20;

function elapsedSince(startTime: number): number {
  if (!Number.isFinite(startTime)) {
    return 0;
  }

  return Math.max(0, Date.now() - startTime);
}

export class HealthMonitor {
  private logger: Logger;
  public metrics: PerformanceMetrics;
  private healthCheckTimer: NodeJS.Timeout | undefined;
  private lastHealthSuccessAt = 0;
  private responseTimeTotal = 0;
  private readonly HEALTH_CHECK_INTERVAL_MS = 30000;
  private readonly HEALTH_CHECK_PAUSE_AFTER_MS = 5 * 60 * 1000;

  constructor(logger: Logger) {
    this.logger = logger;
    this.metrics = {
      totalRequests: 0,
      successfulRequests: 0,
      failedRequests: 0,
      averageResponseTime: 0,
      responseTimes: [],
      connectionStatus: 'disconnected',
      lastHealthCheck: new Date(),
      uptime: Date.now(),
      recentErrors: []
    };
  }

  trackPerformance(startTime: number, success: boolean) {
    const responseTime = elapsedSince(startTime);
    this.metrics.totalRequests++;
    if (success) {
      this.metrics.successfulRequests++;
    } else {
      this.metrics.failedRequests++;
    }

    // Keep last 100 response times for average calculation
    this.metrics.responseTimes.push(responseTime);
    this.responseTimeTotal += responseTime;
    if (this.metrics.responseTimes.length > RESPONSE_TIME_SAMPLE_LIMIT) {
      const removed = this.metrics.responseTimes.shift();
      if (removed !== undefined) this.responseTimeTotal -= removed;
    }

    this.metrics.averageResponseTime = this.responseTimeTotal / this.metrics.responseTimes.length;
  }

  recordError(errorResponse: Record<string, unknown>) {
    try {
      const debugObj = errorResponse._debug as Record<string, unknown> | undefined;
      this.metrics.recentErrors.push({
        time: new Date().toISOString(),
        scope: typeof errorResponse.scope === 'string' ? errorResponse.scope : 'unknown',
        type: typeof debugObj?.errorType === 'string' ? debugObj.errorType : 'UNKNOWN',
        message: typeof errorResponse.error === 'string' ? errorResponse.error : (typeof errorResponse.message === 'string' ? errorResponse.message : 'Unknown error'),
        retriable: Boolean(errorResponse.retriable)
      });
      if (this.metrics.recentErrors.length > RECENT_ERROR_LIMIT) this.metrics.recentErrors.splice(0, this.metrics.recentErrors.length - RECENT_ERROR_LIMIT);
    } catch (error) {
      this.logger.debug('Failed to record health monitor error response', error);
    }
  }

  async performHealthCheck(bridge: UnrealBridge): Promise<boolean> {
    // If not connected, do not attempt any ping (stay quiet)
    if (!bridge.isConnected) {
      this.markDisconnected();
      return false;
    }
    try {
      // Use a safe, no-op stats command that always exists
      await bridge.executeConsoleCommand('stat none');
      this.metrics.connectionStatus = 'connected';
      this.metrics.lastHealthCheck = new Date();
      this.lastHealthSuccessAt = Date.now();
      return true;
    } catch (err1) {
      this.metrics.connectionStatus = 'error';
      this.metrics.lastHealthCheck = new Date();
      // Avoid noisy warnings when engine may be shutting down; log at debug
      this.logger.debug('Health check failed (console):', err1);
      return false;
    }
  }

  startHealthChecks(bridge: UnrealBridge) {
    if (this.healthCheckTimer) return;
    this.lastHealthSuccessAt = Date.now();
    this.healthCheckTimer = setInterval(async () => {
      // Only attempt health pings while connected; stay silent otherwise
      if (!bridge.isConnected) {
        this.markDisconnected();
        // Optionally pause fully after 5 minutes of no success
        if (!this.lastHealthSuccessAt || Date.now() - this.lastHealthSuccessAt > this.HEALTH_CHECK_PAUSE_AFTER_MS) {
          if (this.healthCheckTimer) {
            clearInterval(this.healthCheckTimer);
            this.healthCheckTimer = undefined;
          }
          this.logger.info('Health checks paused after 5 minutes without a successful response');
        }
        return;
      }

      await this.performHealthCheck(bridge);
      // Stop sending echoes if we haven't had a successful response in > 5 minutes
      if (!this.lastHealthSuccessAt || Date.now() - this.lastHealthSuccessAt > this.HEALTH_CHECK_PAUSE_AFTER_MS) {
        if (this.healthCheckTimer) {
          clearInterval(this.healthCheckTimer);
          this.healthCheckTimer = undefined;
          this.logger.info('Health checks paused after 5 minutes without a successful response');
        }
      }
    }, this.HEALTH_CHECK_INTERVAL_MS);
  }

  stopHealthChecks() {
    if (this.healthCheckTimer) {
      clearInterval(this.healthCheckTimer);
      this.healthCheckTimer = undefined;
    }
  }

  setLastHealthSuccessAt(time: number) {
    this.lastHealthSuccessAt = time;
  }

  private markDisconnected(): void {
    this.metrics.connectionStatus = 'disconnected';
    this.metrics.lastHealthCheck = new Date();
  }
}
