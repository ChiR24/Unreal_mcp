import { defineConfig } from 'vitest/config';

export default defineConfig({
    test: {
        globals: true,
        environment: 'node',
        include: [
            'src/**/*.test.ts',
            'tests/unit/**/*.test.ts'
        ],
        exclude: [
            '**/node_modules/**',
            '**/dist/**'
        ],
        coverage: {
            provider: 'v8',
            reporter: ['text', 'text-summary', 'html'],
            include: ['src/**/*.ts'],
            exclude: [
                'src/**/*.test.ts',
                'src/types/**',
                'src/**/*.d.ts'
            ],
            thresholds: {
                // Start with low thresholds, increase as coverage improves
                lines: 20,
                functions: 20,
                branches: 20,
                statements: 20
            }
        },
        testTimeout: 10000,
        hookTimeout: 10000
    }
});
