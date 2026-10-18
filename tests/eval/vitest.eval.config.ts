// tests/eval/vitest.eval.config.ts
// Separate config so the main run (src + tests/unit) never reaches the eval gate.
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['tests/eval/**/*.test.ts'],
    // Each measure runs hundreds of searches over the whole catalog, which a cold run (CI, a fresh transform cache)
    // took past 10 s; the warm latency budget is gate.test.ts's own assertion.
    testTimeout: 60000,
    hookTimeout: 60000,
  },
});
