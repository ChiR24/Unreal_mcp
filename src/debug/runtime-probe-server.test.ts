import { describe, expect, it } from 'vitest';
import { validateProbeSnapshot } from './runtime-probe-server.js';

describe('runtime probe validation', () => {
  const valid = {
    type: 'probe_snapshot',
    provider: 'missile_demo',
    schemaVersion: 1,
    frame: 42,
    simulationTime: 0.7,
    monotonicTimestamp: 1234.5,
    snapshot: { seed: 7 }
  };

  it('accepts a bounded versioned read-only snapshot', () => {
    expect(validateProbeSnapshot(valid)).toMatchObject({ provider: 'missile_demo', frame: 42 });
  });

  it('rejects invalid schemas and snapshots over the byte limit', () => {
    expect(() => validateProbeSnapshot({ ...valid, schemaVersion: 0 })).toThrow('schemaVersion');
    expect(() => validateProbeSnapshot({ ...valid, snapshot: 'x'.repeat(200) }, 100)).toThrow('exceeds');
  });
});
