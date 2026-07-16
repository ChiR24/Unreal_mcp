// tests/eval/task4-baseline-capture.test.ts
// Captures what the CURRENT gateway discovery (offline lexical model) produces
// for each corpus intent, as an observation baseline that is not treated as a
// target ceiling. This is the plan's explicit "Capture the current gateway
// baseline" requirement, which run-baseline.ts does not produce (it only scores
// the golden corpus). The baseline is observability of current discovery: it
// must route each intent to the correct parent and be deterministic, but it is
// not required to equal the golden target ranking.

import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { corpus } from './corpus.js';
import { loadManifestModel, type ManifestModel } from './manifest-model.js';
import { buildCurrentGatewayBaselineAttempts, type CurrentGatewayBaselineAttempt } from './scorer.js';

const MANIFEST_PATH = resolve(process.cwd(), 'src/gateway/gateway-manifest.generated.json');
const manifest: ManifestModel = loadManifestModel(MANIFEST_PATH);

describe('current gateway baseline capture', () => {
  it('Given the golden corpus and current manifest, When the current discovery model ranks every intent, Then one baseline attempt per case is produced', () => {
    const attempts = buildCurrentGatewayBaselineAttempts(corpus, manifest);
    expect(attempts).toHaveLength(corpus.cases.length);
    for (const attempt of attempts) {
      expect(typeof attempt.caseId).toBe('string');
      expect(Array.isArray(attempt.rankedCandidates)).toBe(true);
      expect(attempt.rankedCandidates.length).toBeGreaterThan(0);
    }
  });

  it('Given an exact asset-import intent, When the current lexical model ranks it, Then it routes to the manage_asset parent', () => {
    const attempts = buildCurrentGatewayBaselineAttempts(corpus, manifest);
    const attempt = attempts.find((a) => a.caseId === 'e.manage_asset') as CurrentGatewayBaselineAttempt;
    expect(attempt.top1).not.toBeNull();
    expect(attempt.top1?.tool).toBe('manage_asset');
  });

  it('Given repeated runs, When the current baseline is built twice, Then the result is identical and non-networked', () => {
    const a = buildCurrentGatewayBaselineAttempts(corpus, manifest);
    const b = buildCurrentGatewayBaselineAttempts(corpus, manifest);
    expect(b).toEqual(a);
  });
});
