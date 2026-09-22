import js from '@eslint/js';
import tseslintPlugin from '@typescript-eslint/eslint-plugin';
import tsParser from '@typescript-eslint/parser';
import nPlugin from 'eslint-plugin-n';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));

const tsRecommendedConfigs = tseslintPlugin.configs['flat/recommended'].map((config) => ({
  ...config,
  languageOptions: {
    ...config.languageOptions,
    parser: tsParser,
    parserOptions: {
      ...(config.languageOptions?.parserOptions ?? {}),
      ecmaVersion: 2022,
      sourceType: 'module',
      tsconfigRootDir: __dirname,
    },
  },
}));

export default [
  {
    ignores: [
      'dist/**',
      'node_modules/**',
      '**/*.js',
      '**/*.d.ts',
      '**/*.cjs',
      '**/*.mjs',
      'eslint.config.*',
      'vitest.config.ts',
      'test-*.js',
      'tests/**/*.mjs',
      // Gitignored scratch: absent in CI, so linting it locally reports
      // failures the pipeline will never see and hides ones it will.
      '.omo/**',
      '.kilo/**',
      'tmp/**',
    ],
  },
  js.configs.recommended,
  ...tsRecommendedConfigs,
  {
    files: ['**/*.ts'],
    rules: {
      '@typescript-eslint/no-unused-vars': [
        'warn',
        {
          args: 'all',
          argsIgnorePattern: '^_',
          caughtErrors: 'all',
          caughtErrorsIgnorePattern: '^_',
          destructuredArrayIgnorePattern: '^_',
          varsIgnorePattern: '^_',
          ignoreRestSiblings: true,
        },
      ],
      // Both of these are long-standing project rules (see CLAUDE.md), and
      // both were 'off', so nothing enforced them -- the rule lived only in
      // prose. Turning them on costs nothing: `src/` is already free of
      // explicit `any`, and the only console callers are the two sanctioned
      // sinks exempted below.
      '@typescript-eslint/no-explicit-any': 'error',
      '@typescript-eslint/explicit-module-boundary-types': 'off',
      '@typescript-eslint/no-non-null-assertion': 'warn',
      'no-console': 'error',
      semi: ['error', 'always'],
      quotes: ['error', 'single', { avoidEscape: true }],
      'no-empty': ['error', { allowEmptyCatch: true }],
      'no-useless-escape': 'warn',
      'no-case-declarations': 'off',
      'prefer-const': 'warn',
      'no-unused-vars': 'off',
      // ESLint 10 new rules - disabled to maintain compatibility with existing codebase
      'preserve-caught-error': 'off',
      'no-useless-assignment': 'off',
    },
  },
  {
    // Generators and tests are not the shipped surface, which is what the two
    // rules above exist to protect. A CLI generator's output channel IS the
    // console, and test fixtures and mocks use `any` deliberately rather than
    // restating a type the production code already owns. Scoping the rules to
    // `src/` keeps them meaningful instead of inviting a spray of inline
    // disables that would make them meaningless everywhere.
    files: ['scripts/**/*.ts', 'tests/**/*.ts'],
    rules: {
      'no-console': 'off',
      '@typescript-eslint/no-explicit-any': 'off',
    },
  },
  {
    // The two files that are ALLOWED to touch console, because they are what
    // keeps everyone else off it:
    //   logger.ts        the single sanctioned sink; everything routes here.
    //   server-factory   routeStdoutLogsToStderr(), which reassigns the
    //                    console methods so a stray log cannot corrupt the
    //                    JSON-RPC stdout stream.
    files: ['src/utils/logging/logger.ts', 'src/server/server-factory.ts'],
    rules: {
      'no-console': 'off',
    },
  },
  {
    // Enforce the declared Node.js runtime floor (>=20.19.0). Unsupported
    // built-in APIs and ES syntax are surfaced as warnings so they trip the
    // CI `--max-warnings=0` gate. We intentionally do NOT downgrade @types/node
    // or cast unsupported APIs away; the floor is enforced structurally here.
    files: ['**/*.ts'],
    plugins: {
      n: nPlugin,
    },
    rules: {
      'n/no-unsupported-features/node-builtins': [
        'warn',
        { version: '20.19.0' },
      ],
      'n/no-unsupported-features/es-syntax': [
        'warn',
        { version: '20.19.0' },
      ],
    },
  },
];
