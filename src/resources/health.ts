import { AutomationBridge } from '../automation/index.js';

/**
 * Health check response structure
 */
export interface HealthStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  timestamp: string;
  uptime: number;
  bridge: {
    connected: boolean;
    host: string;
    port: number;
    lastMessageAt: string | null;
    pendingRequests: number;
  };
  cache?: {
    assetListTTL: number;
    cacheHits?: number;
    cacheMisses?: number;
  };
  metrics?: {
    totalRequests: number;
    successfulRequests: number;
    failedRequests: number;
    averageResponseTimeMs?: number;
  };
}

/**
 * Health check resource for monitoring the MCP server status
 */
export class HealthResource {
  private startTime: Date;
  private requestCount = 0;
  private successCount = 0;
  private failureCount = 0;
  private totalResponseTimeMs = 0;

  constructor(private automationBridge: AutomationBridge) {
    this.startTime = new Date();
  }

  /**
   * Track a request for metrics
   */
  trackRequest(success: boolean, responseTimeMs: number): void {
    this.requestCount++;
    if (success) {
      this.successCount++;
    } else {
      this.failureCount++;
    }
    this.totalResponseTimeMs += responseTimeMs;
  }

  /**
   * Get the current health status
   */
  getHealth(): HealthStatus {
    const bridgeStatus = this.automationBridge.getStatus();
    const uptime = Math.floor((Date.now() - this.startTime.getTime()) / 1000);

    // Determine overall status
    let status: HealthStatus['status'] = 'healthy';
    if (!bridgeStatus.connected) {
      status = 'degraded';
    }
    if (!bridgeStatus.enabled) {
      status = 'unhealthy';
    }

    const assetListTTL = parseInt(process.env.ASSET_LIST_TTL_MS ?? '10000', 10);

    return {
      status,
      timestamp: new Date().toISOString(),
      uptime,
      bridge: {
        connected: bridgeStatus.connected,
        host: bridgeStatus.host,
        port: bridgeStatus.activePort ?? bridgeStatus.port,
        lastMessageAt: bridgeStatus.lastMessageAt,
        pendingRequests: bridgeStatus.pendingRequests
      },
      cache: {
        assetListTTL
      },
      metrics: {
        totalRequests: this.requestCount,
        successfulRequests: this.successCount,
        failedRequests: this.failureCount,
        averageResponseTimeMs: this.requestCount > 0
          ? Math.round(this.totalResponseTimeMs / this.requestCount)
          : undefined
      }
    };
  }

  /**
   * Get health status as JSON string (for MCP resource)
   */
  getHealthJson(): string {
    return JSON.stringify(this.getHealth(), null, 2);
  }
}

/**
 * Create a health resource instance
 */
export function createHealthResource(automationBridge: AutomationBridge): HealthResource {
  return new HealthResource(automationBridge);
}
