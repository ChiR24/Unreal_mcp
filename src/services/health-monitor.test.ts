import { describe, expect, it, vi } from 'vitest';
import { HealthMonitor } from './health-monitor.js';
import { Logger } from '../utils/logging/logger.js';

function createLogger(): Logger {
  return new Logger('HealthMonitorTest', 'error');
}

describe('HealthMonitor', () => {
  it('marks the bridge disconnected without sending a ping', async () => {
    const monitor = new HealthMonitor(createLogger());
    monitor.metrics.connectionStatus = 'connected';
    const executeConsoleCommand = vi.fn();
    const bridge = {
      isConnected: false,
      executeConsoleCommand
    };

    const healthy = await monitor.performHealthCheck(bridge);

    expect(healthy).toBe(false);
    expect(monitor.metrics.connectionStatus).toBe('disconnected');
    expect(executeConsoleCommand).not.toHaveBeenCalled();
  });

  it('keeps response time metrics finite for invalid or future start times', () => {
    const monitor = new HealthMonitor(createLogger());

    monitor.trackPerformance(Number.NaN, false, {});
    monitor.trackPerformance(Date.now() + 1000, true, {});

    const snapshot = monitor.telemetry.snapshot();
    expect(snapshot.totals).toEqual({ requests: 2, failures: 1 });
    for (const entry of snapshot.byActionClass) {
      expect(entry.p50Seconds).toBe(0);
      expect(entry.p95Seconds).toBe(0);
    }
  });
});
