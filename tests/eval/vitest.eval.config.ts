// tests/eval/vitest.eval.config.ts
// Dedicated Vitest configuration for the Task-4 evaluation corpus and scorer.
// Kept separate so the main `vitest run` (src + tests/unit) is untouched and
// the Task-4 suite can be run in focused isolation.
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
