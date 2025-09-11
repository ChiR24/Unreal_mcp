import { defineConfig } from 'vitest/config'

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    coverage: {
      reporter: ['text', 'json', 'html'],
      exclude: [
        'node_modules/',
        'dist/',
        'tests/',
        '*.config.ts'
      ]
    },
    testTimeout: 10000,
    hookTimeout: 10000,
    threads: false, // Disable threading for WebSocket tests
  },
  resolve: {
    extensions: ['.ts', '.js', '.json']
  }
})
