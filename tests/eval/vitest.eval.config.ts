// tests/eval/vitest.eval.config.ts
// Separate config so the main run (src + tests/unit) never reaches the eval gate.
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['tests/eval/**/*.test.ts'],
    testTimeout: 10000,
    hookTimeout: 10000,
  },
});
