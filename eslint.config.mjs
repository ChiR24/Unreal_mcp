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
      '@typescript-eslint/no-explicit-any': 'off',
      '@typescript-eslint/explicit-module-boundary-types': 'off',
      '@typescript-eslint/no-non-null-assertion': 'warn',
      'no-console': 'off',
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
