import { Logger } from '../utils/logging/logger.js';
import { TelemetryRegistry } from './telemetry-registry.js';

/**
 * Bounded telemetry dimensions for one tracked request. Both fields are coerced
 * against the closed schema sets before they can become a metric label, so a
 * caller that passes a capability id or a content path here degrades to
 * `unknown` rather than exporting it.
 */
export interface PerformanceDimensions {
  readonly actionClass?: unknown;
  readonly failureClass?: unknown;
  readonly queueWaitMs?: number;
}

export interface PerformanceMetrics {
  connectionStatus: 'connected' | 'disconnected' | 'error';
  lastHealthCheck: Date;
  uptime: number;
  recentErrors: Array<{ time: string; scope: string; type: string; message: string; retriable: boolean }>;
}

interface HealthCheckBridge {
  readonly isConnected: boolean;
  executeConsoleCommand(command: string): Promise<unknown>;
}

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
  /**
   * Bounded counters/histograms for the metrics and resource surfaces.
   * Observation only: nothing reads this back to change server behavior.
   */
  public readonly telemetry: TelemetryRegistry = new TelemetryRegistry({ surface: 'typescript' });
  private healthCheckTimer: NodeJS.Timeout | undefined;
  private lastHealthSuccessAt = 0;
  private readonly HEALTH_CHECK_INTERVAL_MS = 30000;
  private readonly HEALTH_CHECK_PAUSE_AFTER_MS = 5 * 60 * 1000;

  constructor(logger: Logger) {
    this.logger = logger;
    this.metrics = {
      connectionStatus: 'disconnected',
      lastHealthCheck: new Date(),
      uptime: Date.now(),
      recentErrors: []
    };
  }

  trackPerformance(startTime: number, success: boolean, dimensions: PerformanceDimensions) {
    const responseTime = elapsedSince(startTime);
    this.telemetry.observeRequest({
      actionClass: dimensions.actionClass,
      outcome: success ? 'success' : 'failure',
      failureClass: dimensions.failureClass,
      durationSeconds: responseTime / 1000,
      ...(typeof dimensions.queueWaitMs === 'number'
        ? { queueWaitSeconds: Math.max(0, dimensions.queueWaitMs) / 1000 }
        : {})
    });
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
      this.logger.debug('Failed to record health monitor error response', error instanceof Error ? error : String(error));
    }
  }

  async performHealthCheck(bridge: HealthCheckBridge): Promise<boolean> {
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
      this.logger.debug('Health check failed (console):', err1 instanceof Error ? err1 : String(err1));
      return false;
    }
  }

  startHealthChecks(bridge: HealthCheckBridge) {
    if (this.healthCheckTimer) return;
    this.lastHealthSuccessAt = Date.now();
    this.healthCheckTimer = setInterval(async () => {
      // Only ping while connected; stay silent otherwise.
      if (bridge.isConnected) await this.performHealthCheck(bridge);
      else this.markDisconnected();
      if (this.healthCheckTimer && Date.now() - (this.lastHealthSuccessAt ?? 0) > this.HEALTH_CHECK_PAUSE_AFTER_MS) {
        this.stopHealthChecks();
        this.logger.info('Health checks paused after 5 minutes without a successful response');
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
